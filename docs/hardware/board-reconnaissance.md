# Board reconnaissance: ESP32-P4 + ESP32-C5 4.3-inch display

**Status:** research only (issue #2, M0), compiled 2026-10-12 from vendor documents, vendor source and upstream references. **No physical test has been performed.**

Companion pages: [pin/resource matrix](pin-resource-matrix.md), [bring-up checklist](bringup-checklist.md), [source inventory](vendor-source-inventory.md) (source IDs such as V2 and R1 are defined there). Planning context: [Hardware and mounting](../wiki/Hardware-and-Mounting.md), [Development and testing](../wiki/Development-and-Validation.md).

## Evidence classes

| Class | Meaning |
|---|---|
| CONFIRMED | Directly supported by authoritative, matching documentation or source for the ZX4D30CE40S V1.3 design (guide V1, schematic V2, vendor SDK R1/R2). |
| PROVISIONAL | Plausible reference, but not verified for this exact board/revision, or secondary-source only. |
| UNKNOWN | Insufficient evidence. |
| PHYSICAL | Physical validation required; cannot be established without inspecting or testing the device. |

**Identity gate.** Every CONFIRMED item describes the documented V1.3 design. It applies to the purchased unit only after [bring-up step 1](bringup-checklist.md#1-visual-identification--gate) confirms the silkscreen, revision and module markings.

## 1. Summary

- **Most likely board:** ZX4D30CE40S-C5, hardware V1.3, by Shenzhen Qiming Zhixian Technology (brand QM PanelLan, GitHub org `smartpanle`). The sister company Wireless-Tag also sells it as WT32P4C5-43S. Confidence is high for the product and medium for the exact revision until the markings are read.
- **Display:** a 4.3-inch 800×480 panel on the P4's parallel RGB bus with 18 data lines wired. It does not use MIPI-DSI.
- **Coprocessor link:** the ESP32-C5 talks to the P4 over 4-bit SDIO using ESP-Hosted. The P4 switches the C5's power rail with GPIO28.
- **GPIO budget:** nearly every P4 GPIO is committed on the board. Only seven reach the 2×7 expansion header J9: GPIO19–23 and GPIO26/27.
- **USB:** the J10 USB-C port (USB-Serial/JTAG) handles flashing, console and JTAG. J5 is the high-speed USB port for application use.
- **Recommended foundation:** a clean ESP-IDF project with version-pinned components. Vendor code serves only as a factual reference, because its licensing is unclear.

## 2. Board identity

| Item | Finding | Class | Evidence |
|---|---|---|---|
| Manufacturer | Shenzhen Qiming Zhixian Technology Co., Ltd. (深圳市启明智显科技有限公司), brand QM PanelLan / smartpanle; sister company Wireless-Tag | CONFIRMED | V1, V2 title block, R1 |
| Product | ZX4D30CE40S, sub-model **-C5** (WT0132C5-S5 module) or -C61 (WT0132C61-S5) | CONFIRMED (family); PHYSICAL (which sub-model this unit is) | V1 sub-model table |
| Alternative name | Wireless-Tag WT32P4C5-43S | PROVISIONAL | V7 |
| PCB revision | V1.3 ("V13"). The guide introduces hardware V13 on 2026-07-21; earlier hardware has existed since 2024. | CONFIRMED (docs); PHYSICAL (unit) | V1, V2 |
| Naming inconsistency | The schematic root file and the slave README say `ZX4D30CE08S`; CNX prints `ZX4D30CE405` | UNKNOWN significance | V2, R2, V7 |
| Purchase listing | Listing summary (P4 + C5, 4.3", 800×480) is consistent with ZX4D30CE40S-C5. The listing page could not be machine-read. | PROVISIONAL | V6 |
| Main module | WT0132P4-A1-N16R32X (ESP32-P4); part code and vendor config indicate 16 MB flash and 32 MB PSRAM | CONFIRMED | V2 U1, R1 `sdkconfig` |
| P4 silicon | Vendor builds require **revision v3.1–v3.99**; ESP-IDF treats pre-v3.0 and v3.x support as mutually exclusive at build time | CONFIRMED (requirement); PHYSICAL (actual chip) | R1 image header, `Select version.png` |
| Wireless module | WT0132C5-S5 (ESP32-C5). The vendor slave image targets C5 revision v1.x with 4 MB flash. | CONFIRMED (design); PHYSICAL (unit) | V1, V2 U3, R1 |
| Schematic | Available: 4 sheets, Rev V1.3 | CONFIRMED | V2 |
| Mechanical | Overall 117.20 × 81.04 × 16.40 mm. STEP/DWG are published but not reviewed. Mounting points are unknown. | PROVISIONAL / UNKNOWN | V7, V3 |

## 3. Architecture overview

```text
USB-C J10 (USB-Serial/JTAG) ──┐                ┌── USB-C J5 (USB 2.0 HS OTG)
J9 pins 13/14 (5 V in/out) ───┼──── +5 V rail ─┘   (both VBUS pins tied directly)
                              ├─ ME2212 boost ──► LCD backlight (~60.6 mA)
                              └─ ETA3417 buck ──► +3V3
                                   ├─ WT0132P4 module (ESP32-P4, 16 MB flash, 32 MB PSRAM)
                                   ├─ MT9700 switch (EN = GPIO28) ──► ESP_VCC ──► WT0132C5 module
                                   ├─ AO3401A switch (GPIO18, low = on) ──► microSD
                                   └─ panel logic, touch, camera FPC, J9 pins 9/10
P4 ── RGB, 18 bits ──► 40-pin FPC J4 (800×480 panel)
P4 ── SDIO 4-bit (GPIO29–34) ──► C5
P4 ── I2C (GPIO14/15) ──► CST3240 touch (J3) + camera FPC (J2)
```

## 4. Capability inventory

### Compute and memory

| Finding | Class |
|---|---|
| ESP32-P4 module WT0132P4-A1-N16R32X with 16 MB NOR flash (vendor: 80 MHz) and 32 MB PSRAM (vendor: HEX mode, 200 MHz) | CONFIRMED (design) |
| Usable flash/PSRAM on this unit | PHYSICAL (boot log, esptool) |
| Module pin 32 ("LDOv4") is unconnected on the board; module-internal power-domain wiring for GPIO39–48 is unknown | UNKNOWN — read the module datasheet |

### Display

| Finding | Class |
|---|---|
| 4.3-inch TFT, 800×480 | CONFIRMED |
| IPS, 500 cd/m² | PROVISIONAL (V7) |
| Parallel RGB with 18 data lines wired to panel bits R2–R7, G2–G7 and B2–B7. Each channel's two MSBs are replicated into the panel's unused LSB pins. | CONFIRMED (V2 sheet 3) |
| No SPI initialisation interface and no reset line to the panel (vendor SPI and reset pins are -1) | CONFIRMED |
| Panel controller IC and panel datasheet | UNKNOWN |
| Vendor timing: PCLK 25 MHz, falling edge; HSYNC 4 / HBP 8 / HFP 8; VSYNC 4 / VBP 8 / VFP 8 → ~61 Hz | CONFIRMED (vendor-proven); panel limits UNKNOWN |
| Vendor firmware drives a 16-bit RGB565 bus and holds the other two lines (GPIO10 → R2, GPIO54 → B2) high as plain GPIO outputs. The README's claim that the PCB pulls them high is contradicted by V2. | CONFIRMED (R1, V2) |
| Panel DISP (standby) is derived in hardware from VSYNC activity (diode + RC). No GPIO controls it. | CONFIRMED (V2) |
| One RGB565 framebuffer is 768,000 bytes. The vendor keeps 1–3 of them in PSRAM plus two 48,000-byte bounce buffers in internal SRAM. Scan-out at ~61 Hz reads ≈46.8 MB/s from PSRAM. | CONFIRMED (arithmetic on vendor config) |
| Driver: ESP-IDF `esp_lcd` RGB panel (`esp_lcd_new_rgb_panel`) | CONFIRMED (used by R1) |
| LVGL: vendor runs LVGL 9.4.0 through esp_lvgl_adapter 0.6.2 | CONFIRMED (R1); other versions PROVISIONAL |
| 18- or 24-bit output (for example RGB565 input converted to RGB888 output) to avoid the fixed LSB lines | PROVISIONAL — driver capability not checked |

### Backlight

| Finding | Class |
|---|---|
| ME2212 boost converter from +5 V; LED current set by a 3.3 Ω sense resistor (annotated ≈60.6 mA) | CONFIRMED (V2) |
| Controlled by GPIO16 PWM, active high. A 20 kΩ pull-down keeps the backlight off until firmware drives it. | CONFIRMED (V2, V1) |
| Vendor PWM: LEDC 1 kHz, 10-bit. A 1–99 % UI value is mapped to 38–100 % duty because low duties drop out. | CONFIRMED (R1) |
| Minimum stable brightness, flicker, LED string voltage and backlight power | PHYSICAL — measure at TP1 (LED_A) and TP2 (LED_K) |

### Touch

| Finding | Class |
|---|---|
| Hynitron CST3240 capacitive controller on 6-pin FPC J3 | CONFIRMED (R1); V7 agrees |
| I2C address 0x5A at 400 kHz; SDA GPIO14, SCL GPIO15, INT GPIO0, RST GPIO1. 4.7 kΩ pull-ups here plus another 4.7 kΩ pair at the camera FPC give ≈2.35 kΩ combined. | CONFIRMED |
| Vendor driver reports a single point | CONFIRMED |
| Controller multitouch capability | UNKNOWN — not needed for LaunchPad |
| Vendor waits 1.3 s after releasing touch reset | CONFIRMED (R1); reason unknown |
| Touch orientation and calibration relative to the panel | PHYSICAL |

### ESP32-P4 ↔ ESP32-C5

| Finding | Class |
|---|---|
| SDIO 4-bit on P4 SDMMC slot 1, 40 MHz in vendor config. P4 GPIO32 CLK, 31 CMD, 33 D0, 34 D1, 30 D2, 29 D3 connect to C5 IO9, IO10, IO8, IO7, IO14, IO13. | CONFIRMED (V2, R1) |
| 51 kΩ pull-ups on all six SDIO lines, to the **switched** C5 rail | CONFIRMED (V2) |
| C5 power comes from an MT9700 load switch off +3V3, enabled by P4 GPIO28. A 10 kΩ pull-down keeps the C5 unpowered by default. The C5 EN pin only has an RC delay (10 kΩ / 1 µF) and is not driven by the P4. | CONFIRMED (V2) |
| Software: ESP-Hosted-MCU (host `esp_hosted` + `esp_wifi_remote`); the slave runs ESP-Hosted slave firmware. Vendor pins 2.12.3 on both sides. | CONFIRMED (R1, R2) |
| Host and slave must run the same ESP-Hosted version and the same SDIO checksum setting, otherwise the handshake times out | CONFIRMED (R1, R2 READMEs) |
| Vendor's committed host config targets ESP32-**C61**; a C5 build must select the C5 coprocessor target | CONFIRMED (R1) |
| Vendor's "slave reset" is GPIO28, configured active-high with a 1500 ms delay. Because GPIO28 is a power enable, this is effectively a C5 power cycle. | PROVISIONAL — verify on ESP_VCC |
| ESP32-C5 radio: dual-band 2.4/5 GHz Wi-Fi 6, BLE 5, 802.15.4 | PROVISIONAL (chip-level; module antenna and certification not reviewed) |
| Wi-Fi over ESP-Hosted: station and SoftAP. The vendor provisions through a SoftAP captive portal, then joins as a station. | PROVISIONAL (R1 README) |
| WPA2/WPA3-Enterprise is disabled in the vendor slave defaults | CONFIRMED (R2) |
| Factory C5 firmware version on this unit | PHYSICAL |
| C5 flashing: (a) over UART on J6 while GPIO28 powers the C5; (b) host-driven slave OTA over the transport. USB flashing of the C5 is impossible because its USB pins (IO13/14) carry SDIO. | CONFIRMED (a, V2); PROVISIONAL (b) |

### USB, debug and recovery

| Finding | Class |
|---|---|
| J10 USB-C: P4 USB-Serial/JTAG (GPIO24/25) for firmware download, serial console and JTAG; also powers the board | CONFIRMED (V1, V2, U2) |
| J5 USB-C: P4 USB 2.0 HS OTG on dedicated pins, for application USB; also powers the board | CONFIRMED |
| Both ports are sink-only (5.1 kΩ Rd on CC). Both VBUS pins connect directly to the +5 V rail with no diode or switch. | CONFIRMED (V2); PHYSICAL (verify on the unit) |
| No on-board USB-UART bridge | CONFIRMED (V2) |
| J1 (P4) and J6 (C5) are 7-pin 1.25 mm headers (Molex SD-53398-0771) carrying UART, EN and BOOT; the vendor debugger is ZXACC-ESPDB | CONFIRMED |
| J1/J6 power-pin assignment differs between guide and schematic | PHYSICAL — measure before connecting any power lead |
| SW1 resets the P4 (EN). The schematic shows no BOOT button; P4 BOOT (GPIO35) is reachable at J1 pin 6 and TP8. | CONFIRMED (V2); PHYSICAL (button population) |
| Strapping pins are GPIO34–38. GPIO35 (BOOT) and GPIO36 are pulled up; GPIO37/38 are UART0; **GPIO34 is SDIO D1 to the C5**. | CONFIRMED (U2, V2); GPIO34 strap function UNKNOWN |

### Storage, camera, expansion and absent features

| Finding | Class |
|---|---|
| microSD (push-push) on SDMMC slot 0: GPIO39 D0, 40 D1, 41 D2, 42 D3, 43 CLK, 44 CMD. Card detect on GPIO17 with a 10 kΩ pull-up. Power through a PMOS gated by GPIO18 (low or floating = powered). 51 kΩ pull-ups and a 1 kΩ bleeder. | CONFIRMED |
| MIPI-CSI 15-pin 1.0 mm FPC J2: 2 lanes, Raspberry-Pi-style pinout, I2C shared with touch, CSI_EN pulled high | CONFIRMED |
| MIPI-DSI connector J7 is marked not fitted on V1.3 | CONFIRMED (V2) |
| J9: 2×7 2.54 mm header with GPIO19–23, GPIO26/27, +3V3 ×2, GND ×2, +5 V ×2; pin 8 not connected | CONFIRMED |
| No audio codec, battery charger, battery-backed RTC or Ethernet PHY in the V1.3 schematic | CONFIRMED (absent) |

### Power

| Finding | Class |
|---|---|
| 5 V input through J10, J5 or J9 pins 13/14. The vendor recommends a supply of at least 5 V / 1 A; this is a recommendation, not a measured requirement. | CONFIRMED |
| SMF5.0A TVS on the +5 V rail; ESD diodes on the USB data lines | CONFIRMED |
| 3.3 V from an ETA3417 buck converter (EN tied to VIN, 2.2 µH inductor, 100 kΩ/22 kΩ divider) | CONFIRMED (topology); output and current rating PROVISIONAL (datasheet not reviewed) |
| The backlight boost runs directly from +5 V, so cable drop affects it | CONFIRMED (topology) |
| Vendor enables the brownout detector at level 7 | CONFIRMED (R1); threshold voltage UNKNOWN |
| Idle and peak current, cable drop, temperatures | PHYSICAL — not measured; see [checklist step 15](bringup-checklist.md#15-power-and-thermal-measurement) |

## 5. Documentation conflicts

1. **Panel size.** The R1 README says 5.0 inch. The schematic title, the guide and CNX all say 4.3 inch, so 4.3 inch is taken as correct.
2. **Coprocessor.** The R1 README describes only an ESP32-C61. The guide defines both -C5 and -C61 sub-models, and R1 ships images for both.
3. **Debug-header pinout.** The guide lists J1/J6 pin 1 = +5 V, pin 2 = +3.3 V and pin 6 = "GPIO 9". The schematic shows J1 pin 2 unconnected, J6 pin 1 unconnected and J6 pin 2 = switched ESP_VCC. It places BOOT on P4 GPIO35 and C5 IO28.
4. **J9 header.** The symbol is named 2×6 but has 14 pins; the guide and CNX say 2×7. The guide's pitch "2.45" is presumably 2.54 mm.
5. **RGB LSB lines.** The README says the PCB pulls D0/D12 high. The schematic has no such pull-ups; the firmware drives them high.
6. **Channel naming.** `custom_board.h` comments call GPIO54–48 red and GPIO10–5 blue. The schematic routes GPIO10–5 to panel red and GPIO54–48 to panel blue. ESP-IDF's RGB565 bit order appears to compensate (see the [pin matrix](pin-resource-matrix.md#vendor-rgb565-bus-order)); this needs a colour-bar test.
7. **Slave flashing.** The README says to flash at 0x0 but gives an `esptool load-ram` command. That only runs the image from RAM and does not persist. It also names `slave_c61_20260706.bin`, which no longer exists in the repo.
8. **Stale component notes.** The README lists esp_lvgl_adapter 0.4.3 while the lock file pins 0.6.2. It also calls `sdkconfig.defaults.esp32p4` "MIPI-DSI defaults", but the board uses RGB.
9. **Programming port.** CNX describes J5 as the programming port; the guide assigns firmware download to J10.
10. **Template residue.** Schematic sheet 1 has a block labelled "ESP32-S3" around the P4 debug header. There is no S3 on the board.

## 6. Firmware ecosystem assessment

### Versions

| Component | Vendor-proven (R1/R2) | Upstream on 2026-10-12 | Note |
|---|---|---|---|
| ESP-IDF | v6.0.1 | v6.1; v6.0.3 is the latest v6.0 patch | Build for P4 v3.x only; keep "<3.0 revisions" unselected |
| LVGL | 9.4.0 | 9.6.0 | |
| ESP-Hosted (host / slave) | 2.12.3 / 2.12.3 | 3.0.9; 2.13.0; 2.12.13 | Upgrade both sides together |
| LVGL glue | esp_lvgl_adapter 0.6.2 | esp_lvgl_port 2.9.0 | Both are Espressif components; choose at implementation |

### Starting-point options

| Option | Advantages | Disadvantages |
|---|---|---|
| A. Vendor demo firmware (R1) | The only configuration proven on this board; includes hosted networking, provisioning, SD and OTA | No licence grant (no LICENSE file, 33 unlicensed files). Large surface: 40+ managed components, demos, TinyUSB, a debug shell. Modified ESP-Hosted reset handling. Defaults to C61. Stale documentation. |
| B. Espressif BSP or reference example | Clean licensing, maintained upstream | No Espressif BSP for ZX4D30CE40S was identified (registry not exhaustively searched). Generic `esp_lcd` RGB and ESP-Hosted examples still need this board's pins and timings. |
| C. Clean ESP-IDF project with only required, pinned components | Clear GPL-3.0 + Apache-2.0/MIT licensing; minimal surface; every line reviewed; fits the mock-first plan | The board layer must be written, but it is small (RGB config, backlight, touch). Hosted bring-up comes later. |

### Licensing

- **LaunchPad:** GPL-3.0.
- **Dependencies:** ESP-IDF, esp-hosted and the esp-bsp components are Apache-2.0; LVGL is MIT. Re-check these when pinning. Apache-2.0 code is generally regarded as compatible with GPLv3.
- **Vendor code:** vendor-original code carries no licence grant and must not be copied. Hardware facts (pins, timings) are taken from the schematic and re-implemented.
- **Exception:** the vendor CST3240 driver ships an Apache-2.0 licence file, so it could be reused with attribution if writing an own driver proves unnecessary.
- **Dragon:** no Dragon-family code or dependency is involved.

### Build, flash, debug and recovery

- **Build:** ESP-IDF `idf.py set-target esp32p4`, configured for P4 revision v3.x.
- **Flash and console:** over J10 USB-Serial/JTAG; automatic download-mode entry is PROVISIONAL until tested. Fallback: UART0 on J1 with a 3.3 V USB-UART adapter, with BOOT (J1-6/TP8) held low while pressing SW1.
- **Debug:** JTAG over J10 with ESP-IDF's OpenOCD (PROVISIONAL). The vendor build also keeps a core-dump partition.
- **Recovery:** download mode through the BOOT strap, then restore the full-flash backup taken during bring-up.
- **C5:** over UART on J6 while the C5 is powered, or by host-driven slave OTA.

### Wi-Fi provisioning options (an M2 decision)

1. Development only: credentials entered over the serial console into NVS. Never committed.
2. SoftAP captive portal (the vendor approach). Needs no extra radio stack on the P4.
3. BLE provisioning. Needs a Bluetooth host stack on the P4 over the hosted HCI link, which adds complexity.

Recommendation: option 1 during development and option 2 for the product. Enterprise Wi-Fi is disabled in the vendor slave build.

## 7. Recommended firmware foundation

Option C, a clean ESP-IDF project:

- **Toolchain:** pin the exact ESP-IDF tag **v6.0.3**, the bugfix release on the vendor-validated v6.0 line. Keep v6.0.1 as a bisect fallback and evaluate v6.1 only after first light.
- **Components:** declare exact versions in `idf_component.yml` and commit `dependencies.lock`. Use LVGL **9.4.0** for parity with the vendor-proven stack. Use a pinned esp_lvgl_port for LVGL glue, unless RGB bounce-buffer integration forces esp_lvgl_adapter.
- **Board layer:** one small board component written from the schematic. It covers RGB panel config, LEDC backlight, CST3240 touch, an explicit GPIO28 (C5 power) policy and an SD power policy.
- **Networking:** defer ESP-Hosted to M2. Start host and slave at 2.12.3 to match the vendor slave image, then upgrade both sides deliberately.
- **Vendor material:** use it for facts and as prebuilt baseline images for hardware testing only.

## 8. First minimal firmware slice (not implemented)

Goal: prove the display path end to end with nothing else running.

1. Initialise backlight PWM (GPIO16) at 0 %.
2. Leave GPIO28 low so the C5 stays unpowered. Do not start ESP-Hosted, Wi-Fi, SD, USB or touch.
3. Configure the RGB panel: 800×480, 16-bit bus in the vendor data order, PCLK 25 MHz on the falling edge, 4/8/8 timings, framebuffer in PSRAM, 30-line bounce buffer. Drive GPIO10/GPIO54 to a fixed level (high, for vendor parity).
4. Show an LVGL 9.4 static screen: full-width red, green, blue, white and black bars plus a fixed "DEMO 12:34" label.
5. Set the backlight to a fixed duty above the 38 % knee.
6. Log chip revision, PSRAM size and heap at boot.

**Acceptance** (on hardware, by the maintainer): correct colours and orientation, no visible flicker or tearing over 10 minutes, and a clean reboot via SW1. CI may build the slice, but a green CI run is not display validation.

## 9. Power and enclosure considerations

**Known:**

- **Supply:** continuous 5 V through one USB-C port; the vendor recommends at least 5 V / 1 A. The route from the outlet is ≈7 ft plus slack (see the planning page).
- **One input only:** both USB-C VBUS pins are tied together, so use exactly one power input.
- **Load paths:** the backlight boost draws from +5 V directly. The 3.3 V loads (P4, PSRAM, C5 radio, SD) come through the buck.
- **Heat sources** (by topology): the P4 module, buck U2, boost U6 with its Schottky D10, and the C5 module during transmit.
- **Dimming:** PWM 0–100 % is available and the backlight can be switched fully off. The dimmest stable level is unknown (38 % knee). There is no ambient-light sensor on board.
- **No battery-backed RTC:** time is lost on every power interruption until SNTP succeeds. This matches the CLOCK_UNSYNCED state in the Architecture page.
- **Service access** inside an enclosure: J10 (flashing), SW1 (reset), J1/J6 (recovery), J9 (sensors) and microSD if used. Keep the USB-C plug reachable and strain-relieved.

**Must be measured** ([checklist step 15](bringup-checklist.md#15-power-and-thermal-measurement)):

- Supply current for each load case.
- Voltage at the board (J9 pins 13/14) through the real cable versus a short cable, and the 3.3 V rail.
- Backlight string voltage and current.
- Boot and C5-enable transients, and any brownout events.
- Temperatures after a soak.

No adapter or cable has been selected and no enclosure has been designed.

## 10. Open blockers (issue #2 stays open)

1. Physical identification: silkscreen, revision, modules and button population.
2. Debug-header power pins (guide versus schematic).
3. P4 chip revision and factory C5 firmware version.
4. GPIO34 strap function and level at reset; residual voltage on ESP_VCC while the C5 is switched off.
5. Colour order, touch orientation and minimum backlight level.
6. Power and thermal measurements through the real cable.
7. Mounting points (STEP/DWG review).
