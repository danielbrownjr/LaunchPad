# Vendor and reference source inventory

**Status:** research record for issue #2 (M0), compiled 2026-10-12 from public sources. Nothing here has been physically validated. Companion pages: [board reconnaissance](board-reconnaissance.md), [pin/resource matrix](pin-resource-matrix.md), [bring-up checklist](bringup-checklist.md).

**Authority** describes how much weight a source carries for *this* board:

- **Primary** — manufacturer document or vendor build output for ZX4D30CE40S V1.3.
- **Upstream** — Espressif/LVGL source of truth for chips and software.
- **Secondary** — third-party reporting.
- **Unverified** — could not be read or was not reviewed.

Source IDs (V1, R1, U2, …) are referenced throughout the hardware pages.

## Manufacturer documentation

| ID | Source | Version / identifier | Authority | Notes |
|---|---|---|---|---|
| V1 | ZX4D30CE40S User Guide (Chinese), Shenzhen Qiming Zhixian Technology (QM PanelLan) | Dated 2026-09-18. Revision log: V1.0 2024-12-05, V1.1 2026-07-21 (hardware V13), V1.2 2026-09-18 | Primary | Sub-models -C5/-C61, connector list and packages, J1/J2/J3/J4/J5/J6/J9/J10 descriptions, ≥ 5 V / 1 A supply recommendation, links to schematic, STEP, DWG and both SDK repositories. <https://img03.71360.com/w3/4e1e0o/20260918/fdf06126567e0d2e6db63892d066b829.pdf> |
| V2 | Schematic "4.3' RGB666 LCD with ESP32-P4" | Rev V1.3, KiCad 9.0.7, sheet date 2026-04-03, PDF created 2026-04-29, 4 sheets; root sheet file `ZX4D30CE08S.kicad_sch` | Primary | Reviewed copy: `doc/ZX4D30CE40S-V13.pdf` in R1, sha256 `9d4ccf16f4c96b21f458448cf15d5f47616aa2d2bf9ecbeab68169976596f32d` (877,830 bytes). Vendor-hosted copy linked from V1 (`https://res.8ms.xyz/wiki_assets/ZX4D30CE40S/ZX4D30CE40S-V13.pdf`) was not byte-compared. |
| V3 | STEP 3D model and DWG 2D drawing | `ZX4D30CE40S-V13.step`, `ZX4D30CE40S-V13-AIO.DWG` (links in V1 §6) | Primary, not reviewed | Needed for mounting holes and enclosure work (M3). |
| V4 | QM PanelLan product page "ESP32-P4 4.3-inch IPS Touch Display" | <http://en.smartpanle.com/product-item-53.html> | Primary, older | Pre-V13 posting (links `ZX4D30CE40S-EN.pdf`). Do not use for V1.3 pin facts. |
| V5 | Wireless-Tag store listing, WT32P4C5-43S | <https://shop.wireless-tag.com/products/4-3-inch-dual-chip-esp32-p4-and-esp-32-c5-wi-fi6-touch-lcd-screen-wt32p4c5-43s-800x480-resolution-dual-band-esp32-display> | Unverified (not fetched) | Same design sold under the sister brand. |
| V6 | AliExpress purchase listing | <https://a.aliexpress.com/_mqn4l4B> | Unverified | Page is JavaScript-rendered and could not be read. Capture the title, seller, specification text and photos manually. |
| V7 | CNX Software, "Wireless-Tag WT32P4C5-43S" | Published 2026-10-11 | Secondary | Corroborates identity (its "ZX4D30CE405-V1.3" reads as a transcription of ZX4D30CE40S-V1.3), module, 2×7 header, 5 V / 1 A, overall size 117.20 × 81.04 × 16.40 mm. <https://www.cnx-software.com/2026/10/11/wireless-tag-wt32p4c5-43s-a-fully-integrated-4-3-inch-esp32-p4-and-esp32-c5-touch-display-devkit/> |

## Vendor source repositories

| ID | Repository | Ref inspected | Contents | Licence status |
|---|---|---|---|---|
| R1 | <https://github.com/smartpanle/ZX4D30CE40S_V13> | `master` @ `91a80aa4ed644af021c4240f1b15f8dcae2e0255` (2026-09-11; 5 commits since 2026-07-06) | ESP32-P4 demo firmware (CMake project `lcd_layer_blending`, v0.0.1): board pin header, RGB/backlight/touch init, CST3240 driver, committed `sdkconfig` and `dependencies.lock`, schematic, prebuilt images | **No LICENSE file.** The README claims Apache-2.0. SPDX headers: 14 Apache-2.0, 1 CC0-1.0, 1 Unlicense. 33 vendor-authored `.c/.h` files carry no licence header. Treat vendor-original code as all rights reserved. |
| R2 | <https://github.com/smartpanle/ZX4D30CE40S_V13_slave> | `master` @ `cb5e8b1f70cc73a471cdad116546143acd9cf5ec` (2026-09-10; 1 commit) | Espressif ESP-Hosted-MCU slave example: coprocessor firmware version 2.12.3, `dependencies.lock` target `esp32c5` on ESP-IDF 6.0.1, C5/C61 defaults with SDIO streaming and checksum | No LICENSE file. 67 Apache-2.0 SPDX headers (Espressif) and 1 Unlicense. Prefer the upstream Espressif slave example at the matching version. |

**Relevance verdict:** V1 §6 names R1 and R2 as the manufacturer's official SDKs for ZX4D30CE40S V13. The GPIO assignments in R1 match V2 exactly (see the [pin matrix](pin-resource-matrix.md)). R1 and R2 are therefore **matching references for the V1.3 design**. Whether the purchased unit *is* that design depends on bring-up step 1.

### Key files in R1 (at `91a80aa`)

| Path | What it establishes |
|---|---|
| `components/custom_board/include/custom_board.h` | Vendor GPIO assignments (identical to V2) |
| `components/hw_init/lcd/lcd_init_rgb.c` | 800×480, PCLK 25 MHz on falling edge, H/V sync 4/8/8, 16-bit bus order, GPIO10/GPIO54 driven high, PSRAM framebuffers plus a 30-line bounce buffer |
| `components/hw_init/lcd/lcd_backlight.c` | LEDC 1 kHz, 10-bit, with a 38 % duty "knee" because low duties drop out |
| `components/hw_init/input/touch_init.c` | CST3240 at 0x5A on I2C0, 400 kHz |
| `components/esp_lcd_touch_cst3240/` | Single-point CST3240 driver. Ships an Apache-2.0 `license.txt`; its README is copied from Espressif's GT1151 driver. |
| `sdkconfig` | P4 minimum revision v3.1; 16 MB flash at 80 MHz; PSRAM in HEX mode at 200 MHz; brownout detector level 7; ESP-Hosted on SDIO slot 1, 4-bit, 40 MHz, GPIO29–34; slave reset on GPIO28; coprocessor target **ESP32-C61** |
| `dependencies.lock` | idf 6.0.1, lvgl/lvgl 9.4.0, esp_hosted 2.12.3, esp_wifi_remote 1.6.1, esp_lvgl_adapter 0.6.2, esp_lcd_touch 1.2.1, esp_tinyusb 1.7.6~2, littlefs 1.22.1 |
| `readme_change_for_esp_host.md` | Vendor patched esp_hosted to allow "no reset pin". It reports an interrupt-watchdog panic near its HTTP probe and recommends a real reset GPIO. |
| `doc/Select version.png` | menuconfig screenshot: leave "ESP32-P4 revisions <3.0" **unselected**; minimum revision v3.1 |

### Prebuilt images in R1 (`doc/`)

All four are merged images written at offset 0x0. The target column is read from the bootloader header at 0x2000.

| File | Bytes | sha256 | Target |
|---|---|---|---|
| `P4C5/hosted_p4c5_20260910_0x0.bin` | 3,452,448 | `55710b7e801dce203c2808545663ef7df02803324bdc53a4da373fdf9a8f038b` | ESP32-P4, revision v3.1–v3.99, 16 MB flash |
| `P4C5/slave_p4c5_20260910_0x0.bin` | 1,387,984 | `da5fe8dbb9e9b44d0e0a8b542340817c4e209fb06af863a95adbf6a21f520dd5` | ESP32-C5, revision v1.0–v1.99, 4 MB flash; partition table at 0xC000 with `ota_0`/`ota_1` |
| `P4C61/hosted_p4c61_20260910_0x0.bin` | 3,452,448 | `694c0c3f8a66e8290ef1ac634085d51e814316402b11dc1fa052ee9ad0f144f3` | C61 variant — not for this board |
| `P4C61/slave_p4c61_20260910_0x0.bin` | 1,285,920 | `2c0e44dff564dfbab93e35365d62425304815fa5428a8cb792d66ac1baacda0d` | C61 variant — not for this board |

Use these images only for hardware baselining on the owner's unit. Do not commit them to LaunchPad, because no licence is stated.

## Upstream references

Versions were read from git tags on 2026-10-12. Dates are the commit dates of the tagged commits and approximate release dates.

| ID | Reference | Versions | Use |
|---|---|---|---|
| U1 | ESP-IDF | v6.1 (2026-08-25); v6.0.3 (2026-08-31); v6.0.1 (2026-04-22, vendor-proven); v5.5.5 (2026-07-16) | Toolchain |
| U2 | ESP-IDF ESP32-P4 GPIO documentation (v6.1) | — | Strapping pins are GPIO34–38. GPIO24/25 belong to USB-JTAG by default. |
| U3 | LVGL | v9.6.0 (2026-09-16); v9.5.0 (2026-02-18); v9.4.0 (2025-10-16, vendor-proven) | UI library |
| U4 | espressif/esp-hosted-mcu | v3.0.9 (2026-09-26); v2.13.0 (2026-10-09); v2.12.13 (2026-09-04). Vendor pins 2.12.3. | P4↔C5 networking (M2) |
| U5 | esp_lvgl_port (espressif/esp-bsp `master`) | 2.9.0, requires ESP-IDF ≥ 5.2 | LVGL glue candidate |
| U6 | ESP32-P4 and ESP32-C5 datasheets | Not reviewed in detail | Strap functions, power domains, electrical limits — **required reading before GPIO code** |
| U7 | Datasheets for modules WT0132P4-A1 and WT0132C5-S5, and for ETA3417, ME2212, MT9700, AO3401A, CST3240 and the LCD panel | Not located or not reviewed | Component values in these pages are schematic-derived only. |

## Reproduce

```sh
git ls-remote https://github.com/smartpanle/ZX4D30CE40S_V13.git
git ls-remote https://github.com/smartpanle/ZX4D30CE40S_V13_slave.git
# inside a clone of R1 at 91a80aa
sha256sum doc/ZX4D30CE40S-V13.pdf doc/P4C5/*.bin doc/P4C61/*.bin
```
