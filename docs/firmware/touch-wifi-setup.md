# Touchscreen Wi-Fi setup (prototype)

**Status:** The touchscreen Wi-Fi functionality in PR #36 passed native ESP32-P4
CI and physical hardware smoke testing on 2026-10-16. The tested firmware
code was commit `cff734551d424c9a3dd2a5d8bd286c4c542939a6`.
The previous clipping and keyboard-alignment failures are historical,
not the behavior of the accepted candidate.

The existing P4 NVS schema (`lpwifi` namespace, `sta` credentials blob)
is preserved. Saved Wi-Fi credentials are automatically used after reboot.
The ESP32-C5 Wi-Fi driver uses `WIFI_STORAGE_RAM`; the ESP32-P4 owns
intentional credential persistence.

## Touch workflow

1. Tap the **white settings gear** in the top-right header. The Wi-Fi
   indicator immediately to its right is blue when a DHCP IP address has
   been acquired, grey while unconfigured/connecting/disconnected.
2. Tap **Scan** for up to 12 visible SSIDs with RSSI and WPA-personal
   security indication. Unsupported open, WEP and Enterprise networks
   cannot be selected. Use **Manual SSID** for hidden networks.
3. Select a WPA/WPA2/WPA3 personal network. Use the on-screen QWERTY
   keyboard with bottom-left `ABC`/`abc` case switching and symbol mode.
   Enter an 8–63-byte password. The password field is masked.
4. Tap **Save & connect**. The UI hands off a bounded request to the
   background network worker, which writes the P4 NVS record, commits it,
   then handles association, DHCP and SNTP. The status changes from
   connecting to **Saved.** after IP acquisition; this confirms IP
   connectivity, not Internet service or authenticated cloud access.
5. When connected, the settings panel displays the currently assigned
   **IPv4 address**. It displays **IP: not assigned** after disconnect.
   The displayed clock synchronizes only after real SNTP confirmation.
6. After reboot the P4 loads the saved NVS record and reconnects.
   **Forget saved** requires two taps, erases the one saved NVS key, and
   disconnects the station; it does not erase the entire NVS partition.

Wi-Fi scanning, connection RPC, flash I/O and NVS mutations run on the
network worker, not the single LVGL-owning task. Cross-task command
handoffs are bounded and protected by short critical sections. SSID
apostrophes missing from the bundled display font are substituted
**only for visual labels**; the original SSID bytes remain unchanged
for connection and storage.

## Current functional limits

- The on-screen symbol keyboard does not yet provide all possible WPA
  password characters (including backtick, tilde, caret and vertical bar).
  Serial provisioning remains an alternative for affected passwords.
- Opening settings can initiate an automatic scan; a scan during an
  in-progress connection may interrupt that attempt, followed by
  retry backoff. It eventually retries, but can delay reconnection.
- LVGL text entry expects valid UTF-8. SSIDs with invalid UTF-8 bytes
  may not survive text editing unchanged. Do not claim support for
  every possible raw SSID byte sequence.
- A saved but incorrect password can replace the previous working NVS
  credential before association succeeds; the UI lacks explicit
  authentication-failure feedback. Recovery requires entering valid
  credentials again (including via serial).
- Open, WEP and Enterprise networks are outside the prototype's
  touchscreen provisioning scope.

These limitations and runtime state-machine test coverage are tracked
in [issue #38](https://github.com/danielbrownjr/LaunchPad/issues/38).
Do not imply successful NVS save means successful Wi-Fi authentication.

## Security scope

**This is prototype credential persistence, not encrypted secure storage.**
The P4 NVS partition is currently unencrypted; physical access to the
flash can expose the saved Wi-Fi passphrase. Use a non-sensitive/guest
network; do not publish passwords or private diagnostics in GitHub,
chat or CI logs. The password field is visually masked and its text
is cleared on save/cancel, and temporary network-worker credential
buffers are wiped, but LVGL allocator internals are **not** a secure
zeroization boundary. Proper encrypted storage and a defined threat
model remain future release requirements.

## Verified hardware acceptance (2026-10-16)

- **Code commit:** `cff734551d424c9a3dd2a5d8bd286c4c542939a6`
- **CI run:** [38842489436](https://github.com/danielbrownjr/LaunchPad/actions/runs/38842489436), successful exact-head ESP-IDF 6.0.3 ESP32-P4 build, native source/credential/UTF-8 tests, C5 configuration audit and partition fit
- **Artifact:** `12021812218`, ZIP SHA-256 `e9305a4a61ba322e3445978ccd1153fc564d019f05975ee334e6dcc9251d201c`
- **Application:** 974,784 bytes, 73,792 bytes spare in the existing 1 MiB factory application partition at `0x10000`
- **Flashing:** operator installed only the P4 application and independently verified complete application readback and SHA-256; no C5 firmware, bootloader, partition-table or NVS erase
- **PASS:** display, touchscreen, clock, Wi-Fi scan and smart-apostrophe SSID labels, QWERTY and symbol keyboard, password masking, focus, repeated opening/closing, save/connect/forget, network switching, IPv4 display/clearing, blue/grey indicator, and automatic reconnection after a documented reboot
- **Keyboard bounds:** `(38,239)-(761,443)`, 724×205 on the 800×480 LCD
- **Observed stack headroom:** 7,472 bytes initially; 7,536 bytes after reboot

The operator observed no watchdog, unexpected reset, display corruption
or touch freeze during the latest smoke test. An **earlier**
`task_wdt` warning was not reproduced or conclusively explained,
and a long-duration stability soak has **not** been completed.
That reliability investigation is tracked in
[issue #37](https://github.com/danielbrownjr/LaunchPad/issues/37).
Passing this smoke test is not a claim of indefinite runtime stability.

Private raw/redacted operator logs remain on the operator's own PC.
Never commit logs containing credentials or private network identifiers.
