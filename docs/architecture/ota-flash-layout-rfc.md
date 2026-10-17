# RFC: Dual 4 MiB OTA application slots for LaunchPad

**Status: design-only proposal; NOT ACTIVE; NOT APPROVED FOR FLASHING.**
**Scope:** partition layout, host verification and staged migration/recovery plan.
**Out of scope:** OTA HTTP endpoint/uploader, settings, bootloader changes,
encrypted NVS, in-device writes, filesystem format/mount, C5 changes,
S3/S4 UI changes, and any physical flash operation.

## What we actually have

The physical LaunchPad board is a ZX4D30CE40S-V13 with an ESP32-P4 and
16 MiB SPI flash. A successfully reviewed and flashed S2 image is installed;
the operator verified it via full application readback and boot log on
2026-10-17, via P4 J10 USB-Serial/JTAG (COM25 at that time). Backup material
is held **privately on the operator's computer**, not in this repository.

The presently installed and IDF-resolved partition table is:

| Name | Type | Offset | Size | End (exclusive) |
|---|---|---|---|---|
| NVS | data/nvs | 0x009000 | 0x006000 | 0x00F000 |
| PHY init | data/phy | 0x00F000 | 0x001000 | 0x010000 |
| factory | app/factory | 0x010000 | 0x100000 | 0x110000 |

ESP32-P4 has 16 MiB of **flash**, not 32 MiB. The separate advertised
32 MiB PSRAM is volatile and is not firmware/storage partition space.
Current main/S2 contains no OTA uploader, updater endpoint, or proven
OTA rollback acceptance policy. Partition support alone does not make
the product OTA-capable.

## Candidate 16 MiB layout

The **inactive** input file is
`docs/partitions/launchpad-ota-4m-4m.csv`. It must stay out of the active
sdkconfig until a separate migration approval.

| Name | Type / subtype | Offset | Size | End (exclusive) |
|---|---|---:|---:|---:|
| NVS | data/nvs | 0x009000 | 0x006000 (24 KiB) | 0x00F000 |
| PHY init | data/phy | 0x00F000 | 0x001000 (4 KiB) | 0x010000 |
| otadata | data/ota | 0x010000 | 0x002000 (8 KiB) | 0x012000 |
| ota_0 | app/ota_0 | 0x020000 | 0x400000 (4 MiB) | 0x420000 |
| ota_1 | app/ota_1 | 0x420000 | 0x400000 (4 MiB) | 0x820000 |
| reserved | data/0x40 | 0x820000 | 0x7E0000 (7.875 MiB) | 0x1000000 |

All entries are inside the existing 0x1000000-byte flash. The 0x012000
to 0x020000 gap remains unallocated for 64 KiB application alignment;
bootloader at 0x002000 and partition table at 0x008000 remain separate.
The factory partition is **removed** from the future table. OTA data
occupies the *old factory-image header* at 0x010000, so this migration
**cannot be installed by blindly reflashing the partition table**.

The `reserved` data partition uses a custom subtype 0x40, not an
implicitly formatted filesystem or a mountable SPIFFS/LittleFS promise.
It is **unallocated product storage** pending future design. In a later
PR we may split the 7.875 MiB for fonts/assets, logs and a capped cache,
with explicit wear, sensitive-data, formatting, update and crash-recovery
policies. Current S2/3/4 **must not** write to or erase this region.

No factory-app slot is required for two-slot OTA. ESP-IDF bootloader
selects `ota_0` by default if `otadata` is erased and there is no
factory partition. OTA metadata must be exactly 0x2000 bytes. Application
offsets are 0x10000-aligned, and all partition offsets are 0x1000-aligned.

## Target support: two separate approvals

### A. Offline partition migration (future, destructive risk)

**Before ANY writing:**

1. Get an explicit owner go-ahead for altering partition table, replacing
   the running factory app and writing otadata. The present approval is
   **design only**. Confirm that exact board, flash size, bootloader,
   encryption and secure-boot/eFuse state, C5 firmware, USB recovery and
   target artifact match. Do not assume module voltage wiring verified.
2. Use the private two-copy 16 MiB factory backup, last-known-good S2/PR36
   app, old partition-table binary, and a *new* fresh 16 MiB pre-migration
   readback; retain hashes, safe local copies and the tested restore recipe.
   Treat full dumps as private, since NVS credentials may be plaintext.
3. Audit contents of **all target ranges** against the private full dump,
   especially 0x010000 (old app), both 4 MiB OTA ranges and 0x820000+
   (future reserved storage). Do not assume flash outside the old 1 MiB
   factory allocation was erased or dispensable.
4. Validate the candidate CSV using ESP-IDF **6.0.3** partition generator
   and cross-check the generated binary against the host geometry tests.
   Record binary/image SHA-256 and flasher arguments. Verify actual image
   size is smaller than 0x400000 and that resolved C5 SDIO pins remain
   unchanged.
5. Review the **currently installed bootloader** compatibility with
   factory-less dual-slot OTA; verify which rollback configuration is
   actually compiled into it. Rebuilding/flashing bootloader is **not**
   implicitly authorized. If a new bootloader is necessary, stop and
   present a new explicit risk/approval gate.

**Staged operator-only migration concept:**

- Use the physical USB ROM/download recovery method, not an OTA upload.
  The new `ota_0` slot at 0x020000 overlaps the old factory app body:
  overwriting it destroys the bootability of the current factory app.
  This migration is **not power-failure-atomic**. The operator must have
  tested ROM recovery and accept a possible temporary nonbootable board.
- Under an approved maintenance procedure, program a pinned, compatible
  P4 application to `ota_0` at 0x020000, and independently read back and
  hash that image. It is reasonable to pre-provision `ota_1` too, but
  equal image contents alone do not establish a verified rollback state.
  Never flash the original S2 image to the old 0x010000 factory offset
  under the candidate table.
- Ensure `otadata` 0x010000..0x011FFF is **erased** (all 0xFF) for initial
  `ota_0` selection. This erases the old factory image header.
- Program and read back the **new partition table at 0x008000 last**,
  after the selected OTA application and otadata are verified. Do not
  write any unlisted region and do not issue `erase-flash`.
- Reboot with serial monitoring, record selected partition, real app
  identity, resolved NVS data and network/touch/clock status, verify
  application version, and keep the known-good ROM recovery method handy.
  If anything is wrong, **stop** and use the privately documented full
  restore strategy. Restoring only the old app while leaving the new
  partition table would not recreate the original boot layout.

**Power-loss warning:** rewriting the app body and partition table is
not an atomic transaction. A failure between those writes can require
USB ROM recovery. Do not claim zero-downtime migration or guaranteed
rollback to an old factory app.

### B. Actual authenticated OTA implementation (later PR)

OTA support is **not** established just because the partition entries
exist. Implement an authenticated, integrity-checked, bounded update
source and app lifecycle separately, with no credential leakage, no
reliance on untrusted filenames, and explicit failure paths.

In particular:

- Identify the inactive slot using ESP-IDF OTA APIs, validate the image,
  check target/build compatibility, and never overwrite the running app.
- Only select the new partition for boot **after** the upload is complete
  and image verification succeeds; support interruptions and insufficient
  space without changing the current boot.
- Decide the pinned ESP-IDF v6.0.3 rollback configuration and validate
  it against **the installed bootloader**, not current online docs alone.
  Prefer an *application-controlled* confirmation checkpoint after the
  critical display, network and settings startup checks succeed, rather
  than assuming reaching `app_main` proves UI functionality.
- Reject or roll back a new image that fails first-boot validation.
  Confirm `ESP_OTA_IMG_VALID` only after defined checks, and test
  unexpected reset, brownout, corrupt metadata/image, stale source,
  a failed update and a verified recovery image in the other slot.
  Don't claim rollback available until a valid previous slot exists.
- Plan safe debug/serial recovery as an independent path even if rollback
  is configured, and keep this prototype's unencrypted NVS risk explicit.
  OTA auth, encryption, signature policy, CSRF/transport model and
  retention limits require their own threat model.

## Implementation separation and acceptance

This RFC PR must **not** edit `sdkconfig.defaults`,
`tools/check_c5_build.py`, active partition generation or firmware code.
The current exact-head CI remains pinned to the **installed factory 1 MiB
layout**, and its partition-fit check must **continue to enforce 1 MiB**.
When a migration PR is approved, switch the active config and size gate
together, not in this proposal.

Acceptance for this RFC is offline-only: parse and validate the proposed
CSV, prove NVS/PHY preservation, ota metadata exactly 8 KiB, both equal
4 MiB aligned app slots, nonoverlap and coverage through 0x1000000,
assert the data reservation, run existing CI and have an independent
ESP-IDF 6.0.3 bootloader/partition review.

**Still unverified:** actual contents in extra flash after 0x110000,
installed bootloader rollback support, first boot on the P4 with the new
table, whether a filesystem will be useful, and a production OTA provider.
No flash/erase/reset is included in this design PR.

## Public technical references

- Espressif ESP-IDF ESP32-P4 partition tables (app 64 KiB alignment, data
  4 KiB alignment and custom subtypes):
  https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32p4/api-guides/partition-tables.html
- ESP-IDF 6.0 partition Kconfig selection:
  https://docs.espressif.com/projects/esp-idf/en/release-v6.0/esp32p4/api-reference/kconfig-reference.html
- ESP-IDF OTA data selection, empty otadata without factory, application
  rollback and first-boot validity:
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html
