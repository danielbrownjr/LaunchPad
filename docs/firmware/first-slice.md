# First firmware slice: offline RGB display and PSRAM diagnostic

**Status:** first implementation, **not flashed or physically validated**. The
GitHub PR is deliberately stacked on the unmerged V1.3 board research PR.
Refs #6 and #2; does not close either issue.

## What it does

- Targets **ESP32-P4** with ESP-IDF **v6.0.3** and LVGL **v9.4.0**.
- Uses V1.3 800×480 RGB565 bus order from
  [the provisional pin matrix](../hardware/pin-resource-matrix.md).
- Leaves **C5 power gate off** (GPIO28 low). No ESP-Hosted, Wi-Fi, API, storage,
  touch driver, real clock, provisioning or employer data.
- Shows a fictional next appointment and afternoon item, both visibly marked
  as demo data. Time is always `--:-- / CLOCK NOT SET`; no false wall clock.
- Displays runtime PSRAM capacity on screen and logs if it differs from 32 MiB.
- Shows three red/green/blue patches for visual panel-bus verification.
- Enables fixed full backlight (GPIO16) only after LVGL's first paint.

## Reproducible build and checks

From a local ESP-IDF **v6.0.3** environment:

```sh
python3 -m unittest discover -s tests -v
idf.py set-target esp32p4
idf.py build
```

ESP-IDF Component Manager resolves `lvgl/lvgl ==9.4.0`. On the first verified
CI build, capture and commit the generated `dependencies.lock` so the full
resolution is preserved. Do not substitute the vendor SDK source: the vendor
repository's licensing status is not suitable for code copying.

The pull-request workflow compiles for the actual ESP32-P4 and retains a
flashable artifact set. A green build is **not** a physical LCD test.

## Physical acceptance, only with explicit flashing authorization

1. Verify the two full factory-flash backups remain byte-identical. Preserve
   an offline encrypted copy and keep a recovery path.
2. Connect LCD and touch FPCs. Use **J10 DEBUG USB only**; never power both
   USB-C ports simultaneously. Confirm the panel is seated.
3. Flash only the exact green ESP32-P4 CI artifact after Dan approves flashing.
4. Check clean cold boot, P4 revision, reported PSRAM size (expected 32 MiB),
   all text visible, landscape orientation, legibility, correct red/green/blue,
   10-minute stability without flicker/tearing, reset recovery, and no brownouts.
5. Measure full-backlight current and voltages, especially with the intended
   long USB cable. Fixed brightness here is for short bring-up only, not a
   finalized wall-installation policy.
6. Restore factory firmware from the preserved backup if needed, using
   documented ESP32-P4 bootloader/partition and flash commands.

## Known limits

- `--:--` is deliberately static until a trusted time source is implemented.
- Appointments are hardcoded fictional fixtures, not a scheduler.
- Touch hardware worked in the factory demo, but no LaunchPad input driver exists.
- C5 firmware/version, network, sync freshness, DST, and Simpro integration
  are not part of this slice.
- PSRAM is **not verified** until this app runs on the actual board.
- Panel timing/pin map came from vendor documentation and have yet to be
  validated with our own firmware. Any mismatch must be fixed before merge.
- This slice does not claim completion of M1 issues #6–#10.
