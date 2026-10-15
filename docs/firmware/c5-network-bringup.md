# ESP32-C5 network bring-up: development gate

**Status: CI candidate only, no physical validation or new flashing authorized.**
This is the first step towards the synchronized clock, not the clock itself.
The synthetic appointments remain fictional. The UI clock correctly stays
`--:-- / CLOCK NOT SET` until SNTP succeeds in a subsequent change.

## Confirmed board wiring / selected software

- ZX4D30CE40S-V13 with ESP32-P4 v3.2 host and C5 companion.
- GPIO28 controls the C5 **power load switch** (active high), not EN.
- P4 SDIO slot 1: CLK 32, CMD 31, D0 33, D1 34, D2 30, D3 29.
- Four-bit 40 MHz, checksum, 1500 ms reset settling, matching the V1.3 vendor
  configuration except its **C61 target is deliberately corrected to C5**.
- ESP-IDF 6.0.3, ESP-Hosted 2.12.3, esp_wifi_remote 1.6.1. Pin both.
- C5 factory slave firmware version/SDIO checksum **not physically verified**.
  Host/slave version or transport mismatch may prevent the handshake.

## Implementation boundaries

- The known-good RGB/LDO4 and CST3240 setup runs first.
- ESP-Hosted/remote Wi-Fi initializes in a separate worker so a slow or
  mismatched C5 cannot block the LVGL owner's first frame.
- The board starts with C5 power OFF; ESP-Hosted then owns the GPIO28 reset
  sequence and SDIO transport. Avoid independent writes to GPIO28 afterward.
- A successful remote Wi-Fi `esp_wifi_start` is **not** a DHCP lease,
  Wi-Fi association, internet connectivity, or clock sync.
- There are no baked-in SSIDs/passwords. This slice does not call
  `esp_wifi_connect`, mutate C5 flash, enable an AP, or start SNTP.
- Footer reports C5 starting / Wi-Fi unconfigured / link error. No background
  task touches LVGL; only the existing UI owner reads network state.
- If ESP-Hosted blocks on a missing C5 handshake, the LCD/touch owner keeps
  running; the footer can remain STARTING. A bounded handshake and recovery
  path belong in the next integration pass.

## Mandatory hardware gate before a P4 application-only flash

1. Preserve exact verified PR #33 application and full factory backups.
2. Inspect the new CI artifact/ESP32-P4 target/partition fit and its hash.
3. Confirm P4 J10 DEBUG is the **only** powered USB input.
4. Operator must separately authorize an **application-only** P4 flash at
   the independently checked app partition offset `0x10000`.
5. Read back the entire written image byte-for-byte.
6. Confirm LCD/backlight and tap test still work after startup; do not change
   known-good LDO4 or LCD timing to solve C5 errors.
7. Capture the transport INIT/version/capability and Wi-Fi RPC logs.
   Confirm the coprocessor reports C5 and a compatible slave version.
8. Check for unresponsive C5, repeated resets, brownouts, watchdogs,
   SDIO checksum failures, and excessive worker stack use.
9. If mismatched, STOP. No C5 reflash or automatic rollback without
   separate authorization and a verified backup/recovery plan.

**Next change:** after a real C5 handshake is proven, provide secure
development-only credential provisioning into NVS, emit association/DHCP
state separately, and then add SNTP with explicit sync validity and
timezone/DST handling. Never publish credentials or workplace data.
