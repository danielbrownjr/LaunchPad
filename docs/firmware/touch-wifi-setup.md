# Touchscreen Wi-Fi setup (prototype)

**Status:** Implementation in a draft pull request; requires native ESP32-P4
build and a separate operator-approved hardware acceptance test.

The existing P4 NVS schema (\`lpwifi\` namespace, \`sta\` credentials blob)
is preserved. Credentials that worked with the previous serial-only Wi-Fi
firmware will continue to load automatically after reboot. The C5 runs with
\`WIFI_STORAGE_RAM\`, so the P4 remains the only intended persistence owner.

## Touch workflow

1. Tap **Wi-Fi Setup** at the top right of the 800×480 dashboard.
2. Tap **Scan** to list up to 12 visible SSIDs, with RSSI and security.
   Empty/hidden SSIDs and duplicate names are suppressed. Unsupported
   open, WEP and Enterprise networks are shown but cannot be selected.
3. Select a WPA/WPA2/WPA3 personal network, or **Manual SSID** for a
   non-broadcast network. Enter an 8–63-byte passphrase using the on-screen
   keyboard. The password field is masked.
4. Tap **Save & connect**. The UI sends a validated, bounded request to the
   background C5 network task. That task writes P4 NVS, commits it,
   disconnects from the previous network if necessary, and attempts
   association/DHCP/SNTP.
5. On later boots, the P4 loads its saved NVS record and automatically
   reconnects to that one saved network. No repeated credential entry needed.
6. **Forget saved** requires a second confirmation tap, erases only that one
   NVS key, and disconnects the station. It does not wipe the NVS partition.

Network scan, Wi-Fi RPC calls, flash I/O and NVS mutation never occur on
the LVGL owner. Cross-task requests are serialized. The existing real clock
and display/touch setup remain unchanged. A scan may temporarily interrupt
an in-progress connection attempt; never claim a scan implies association.

## Security scope

This is **prototype credential persistence**, not encrypted secure storage.
The P4 NVS partition is currently unencrypted; physical access to flash can
expose a saved Wi-Fi password. Use a non-sensitive or guest network
credential, and do not send passwords in chat/GitHub or save them in logs.
The LVGL password widget is masked and cleared on cancel/save, but its
allocator is not a guaranteed secure zeroization boundary. Encryption and
a stronger secrets storage design remain a separate requirement.

## Acceptance and boundaries

The real board previously passed P4 boot, CST3240 touch, C5 SDIO,
Wi-Fi association, DHCP, SNTP and offline/reconnection tests. This **new
touch Wi-Fi UI has not yet been physically tested**.

Do not merge or flash without a separately approved test plan. Preserve
the known-good P4 image, factory backup, partition table, bootloader and
existing P4 NVS. The original 1 MiB app partition at \`0x10000\` must
still accommodate the expanded firmware; native CI checks exact target,
SDIO and application size.

Physical validation should include network scan while unconfigured and
already connected, on-screen keyboard accuracy, password masking, AP
selection and manual entry, NVS save, reboot auto-join, change to a new
network, forget confirmation, and no regression to the live clock.
