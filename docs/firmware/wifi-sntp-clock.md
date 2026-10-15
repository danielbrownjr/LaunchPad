# LaunchPad prototype Wi-Fi and SNTP clock

Status: **draft PR #35, not yet flashed or physically verified**.
Stacked on C5 transport PR #34. C5 INIT and remote Wi-Fi start on the board
were previously confirmed in the operator's P4 startup logs.

## Serial development provisioning

1. Connect the verified **J10 DEBUG USB** to the P4 only.
2. Open the P4 USB serial console. Disable host-side local echo and
   terminal/session input recording before entering secrets.
3. Type the literal command **wifi** and press Enter.
4. At the prompt, enter the SSID (1 to 32 bytes), then Enter.
5. Enter the WPA passphrase (8 to 63 bytes), then Enter.

The firmware does not print credentials or include any SSID/password in
source, logs or CI. Input validation is bounded; temporary buffers are
explicitly wiped. The remote C5 Wi-Fi settings are **RAM-only**.

**Important security limitation:** The P4 NVS namespace lpwifi / key sta
stores the Wi-Fi passphrase in **plaintext on flash** in this prototype.
Physical flash extraction can recover it. NVS encryption and a better
touchscreen provisioning interface must be a separate future work item.
Do not use a sensitive password or treat this as production provisioning.
Open APs, enterprise Wi-Fi and raw 64-digit PSKs are not supported yet.
Credentials should never enter GitHub, chat, repository files or public logs.
NVS errors do not trigger erase/reformat operations.

## States and acceptance

- UNCONFIGURED: no usable P4 credentials.
- CONNECTING / DISCONNECTED: STA association or retry with at most
  32-second exponential backoff.
- IP_READY: real DHCP address acquisition reported by IP_EVENT_STA_GOT_IP.
  A station association alone is not an IP address.
- SYNCING: SNTP started, but no validated wall-clock sample yet.
- SYNCED: actual SNTP time callback, not a guessed plausible epoch.
- OFFLINE TIME: time previously validated, P4 system clock advancing,
  but Wi-Fi/IP subsequently lost.
- SYNC STALE: at least 24 hours since the latest confirmed NTP callback.
- ERROR: initialization or remote Wi-Fi RPC error.

On cold boot without an SNTP callback, render --:-- and CLOCK NOT SET.
After synchronization, render 12-hour local time, fixed to North American
Eastern via the POSIX timezone EST5EDT,M3.2.0/2,M11.1.0/2. Automatic DST
is applied. Clock labels are updated only by the owning LVGL task;
networking and time callbacks never call LVGL. The calendar/appointment
cards remain explicitly synthetic.

NTP service uses pool.ntp.org and requires functioning DNS and outbound
UDP port 123. The board has no battery RTC, so rebooting while disconnected
will return to an unset clock.

## Flash and size limits

Current P4 contains a 16 MiB physical flash but the factory app partition
at offset 0x10000 is only 0x100000 bytes (1 MiB). Firmware enables
CONFIG_COMPILER_OPTIMIZATION_SIZE, and CI validates the *resolved*
configuration, C5 SDIO mapping and actual build/launchpad.bin size.
No partition table, bootloader, factory backup or C5 firmware is modified.

No physical operation is authorized by this PR. The operator must separately
approve application-only flashing of an exact CI artifact, retain the known
PR #34 image and factory backups, and verify a full byte-for-byte readback.
Test screen, touch, C5 INIT, serial provisioning, Wi-Fi association and DHCP,
SNTP callback, Eastern display, network loss and recovery, and boot without
available network. Redact all serial input before sharing logs.
