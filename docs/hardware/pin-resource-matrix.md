# Pin and resource matrix (provisional)

**Status:** provisional allocation for the ZX4D30CE40S V1.3 design (issue #2, M0). Sources are the V1.3 schematic (V2), vendor firmware `custom_board.h` and `sdkconfig` (R1 @ `91a80aa`), and the vendor guide (V1); see the [source inventory](vendor-source-inventory.md). All three agree on every GPIO row unless a row says otherwise. **Nothing here is physically verified. Do not write GPIO code for any pin marked PHY or U.**

**Status codes:** C = CONFIRMED by the V1.3 documents/source, P = PROVISIONAL, U = UNKNOWN, PHY = physical validation required. The "LaunchPad" column is the intended use and is a provisional allocation.

## ESP32-P4 GPIO map

| GPIO | Net | Use / destination | Constraints | Status | LaunchPad |
|---|---|---|---|---|---|
| 0 | TP_INT | Touch interrupt (J3-5) | Input | C | Touch |
| 1 | TP_RST | Touch reset (J3-6) | Output | C | Touch |
| 2 | RGB_D11 | Panel G7 (also drives G1) | | C | Display |
| 3 | RGB_D10 | Panel G6 (also drives G0) | | C | Display |
| 4 | RGB_D6 | Panel G2 | | C | Display |
| 5 | RGB_D5 | Panel R7 (also drives R1) | | C | Display |
| 6 | RGB_D4 | Panel R6 (also drives R0) | | C | Display |
| 7 | RGB_D3 | Panel R5 | | C | Display |
| 8 | RGB_D2 | Panel R4 | | C | Display |
| 9 | RGB_D1 | Panel R3 | | C | Display |
| 10 | RGB_D0 | Panel R2 | Not on the 16-bit bus; vendor holds it high | C | Display (fixed level) |
| 11 | RGB_D7 | Panel G3 | | C | Display |
| 12 | RGB_D8 | Panel G4 | | C | Display |
| 13 | RGB_D9 | Panel G5 | | C | Display |
| 14 | TP_SDA / IIC_SDA | I2C SDA to touch (J3-2) and camera (J2-14) | Two 4.7 kΩ pull-ups | C | I2C (touch) |
| 15 | TP_SCL / IIC_SCL | I2C SCL to touch (J3-3) and camera (J2-13) | Two 4.7 kΩ pull-ups | C | I2C (touch) |
| 16 | LCD_BL | Backlight PWM | 1 kΩ series, 20 kΩ pull-down, active high | C | Backlight |
| 17 | SD_DET | microSD card detect | 10 kΩ pull-up; polarity PHY | C | Unused in M1 |
| 18 | ~SD_PWR | microSD power PMOS gate | Low or floating = card powered | C | Policy TBD |
| 19 | OUT5 | J9-7 | ADC1_CH3 | C | Spare (sensor) |
| 20 | OUT4 | J9-6 | ADC1_CH4 | C | Spare |
| 21 | OUT3 | J9-5 | ADC1_CH5 | C | Spare |
| 22 | OUT2 | J9-4 | ADC1_CH6 | C | Spare |
| 23 | OUT1 | J9-3 | ADC1_CH7 | C | Spare |
| 24 | USB0_DN | J10 D− (USB-Serial/JTAG) | Using it as GPIO disables USB-JTAG | C | Reserved |
| 25 | USB0_DP | J10 D+ (USB-Serial/JTAG) | Using it as GPIO disables USB-JTAG | C | Reserved |
| 26 | OUT0_N | J9-2 | USB1 full-speed PHY D− | C | Spare (use last) |
| 27 | OUT0_P | J9-1 | USB1 full-speed PHY D+ | C | Spare (use last) |
| 28 | WIFI_EN | MT9700 enable → C5 supply | 10 kΩ pull-down | C | C5 power |
| 29 | SDIO_D3 | C5 IO13 | 51 kΩ pull-up to ESP_VCC | C | SDIO |
| 30 | SDIO_D2 | C5 IO14 | 51 kΩ pull-up | C | SDIO |
| 31 | SDIO_CMD | C5 IO10 | 51 kΩ pull-up | C | SDIO |
| 32 | SDIO_CLK | C5 IO9 | 51 kΩ pull-up | C | SDIO |
| 33 | SDIO_D0 | C5 IO8 | 51 kΩ pull-up | C | SDIO |
| 34 | SDIO_D1 | C5 IO7 | **Strapping pin**; its pull-up is on the switched C5 rail only | C (wiring); U (strap effect) | SDIO |
| 35 | BOOT | Boot strap; J1-6 and TP8 | Strapping pin; 10 kΩ pull-up, 1 kΩ series | C | Reserved |
| 36 | — | 10 kΩ strap pull-up only; not routed anywhere else | Strapping pin | C | Unavailable |
| 37 | TX0 | UART0 TX → J1-3, TP5 | Strapping pin | C | Console/recovery |
| 38 | RX0 | UART0 RX ← J1-4, TP6 | Strapping pin | C | Console/recovery |
| 39 | SD_D0 | microSD DAT0 | SDMMC slot 0 IO-MUX pin | C | Unused in M1 |
| 40 | SD_D1 | microSD DAT1 | | C | Unused in M1 |
| 41 | SD_D2 | microSD DAT2 | | C | Unused in M1 |
| 42 | SD_D3 | microSD DAT3/CD | | C | Unused in M1 |
| 43 | SD_CLK | microSD CLK | | C | Unused in M1 |
| 44 | SD_CMD | microSD CMD | | C | Unused in M1 |
| 45 | RGB_DE | Panel DE | | C | Display |
| 46 | RGB_VS | Panel VSYNC; also generates panel DISP | | C | Display |
| 47 | RGB_HS | Panel HSYNC | | C | Display |
| 48 | RGB_D17 | Panel B7 (also drives B1) | | C | Display |
| 49 | RGB_CLK | Panel PCLK through 220 Ω | | C | Display |
| 50 | RGB_D16 | Panel B6 (also drives B0) | | C | Display |
| 51 | RGB_D15 | Panel B5 | | C | Display |
| 52 | RGB_D14 | Panel B4 | | C | Display |
| 53 | RGB_D13 | Panel B3 | | C | Display |
| 54 | RGB_D12 | Panel B2 | Not on the 16-bit bus; vendor holds it high | C | Display (fixed level) |

**Notes:**

- **Dedicated non-GPIO pins:** USB 2.0 HS D± go to J5, MIPI-CSI lanes to J2, and MIPI-DSI lanes to J7 (not fitted).
- **ADC labels:** the ADC labels for GPIO16–23 come from the module symbol in the schematic. Confirm them against the ESP32-P4 datasheet before any analog use.

### Allocation summary

| Group | GPIOs | Count |
|---|---|---|
| Display (18 data lines + PCLK, HSYNC, VSYNC, DE) | 2–13, 45–54 | 22 |
| Touch and shared I2C | 0, 1, 14, 15 | 4 |
| Backlight | 16 | 1 |
| microSD | 17, 18, 39–44 | 8 |
| C5 power and SDIO | 28–34 | 7 |
| USB-Serial/JTAG | 24, 25 | 2 |
| Boot straps and UART0 console | 35–38 | 4 |
| **Free, on J9 only** | **19–23, 26, 27** | **7** |

## Vendor RGB565 bus order

These are ESP-IDF `data_gpio_nums` indices; index 0 is the least significant bit.

| Bus index | 0–4 | 5–10 | 11–15 |
|---|---|---|---|
| GPIO | 53, 52, 51, 50, 48 | 4, 11, 12, 13, 3, 2 | 9, 8, 7, 6, 5 |
| Panel bits | B3–B7 | G2–G7 | R3–R7 |

ESP-IDF's RGB565 layout puts blue in the low five bits and red in the high five. This order therefore lands each channel on the matching panel pins, even though the `custom_board.h` comments label the groups the other way round. Status: PHY — verify with the colour-bar slice.

## Connectors

### J1 — P4 debug, 7-pin 1.25 mm

| Pin | Schematic V1.3 | Guide | Status |
|---|---|---|---|
| 1 | +5 V (TP4) | +5 V | C |
| 2 | Not connected | +3.3 V | **Conflict → PHY** |
| 3 | TX0, GPIO37 (TP5) | ESP_TXD | C |
| 4 | RX0, GPIO38 (TP6) | ESP_RXD | C |
| 5 | ESP_EN (TP7) | EN | C |
| 6 | BOOT, GPIO35 (TP8) | BOOT ("GPIO 9") | C (GPIO35 per schematic) |
| 7 | GND (TP9) | GND | C |

### J6 — C5 debug, 7-pin 1.25 mm

| Pin | Schematic V1.3 | Guide | Status |
|---|---|---|---|
| 1 | Not connected | +5 V | **Conflict → PHY** |
| 2 | ESP_VCC, switched 3.3 V (TP10) | +3.3 V | **Conflict (switched rail) → PHY** |
| 3 | C5 TXD0 (TP11) | ESP_TXD | C |
| 4 | C5 RXD0 (TP12) | ESP_RXD | C |
| 5 | C5 EN (TP13) | EN | C |
| 6 | C5 IO28 BOOT (TP14) | BOOT ("GPIO 9") | C (IO28 per schematic) |
| 7 | GND (TP15) | GND | C |

TX/RX are named from the module's side: connect the adapter's RX to pin 3. **Until step 2 of the [bring-up checklist](bringup-checklist.md) is done, connect only GND, TX, RX, and BOOT/EN if needed. Never connect an adapter's power lead.**

### J9 — expansion, 2×7, 2.54 mm

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | GPIO27 (OUT0_P, USB1 D+) | 2 | GPIO26 (OUT0_N, USB1 D−) |
| 3 | GPIO23 (OUT1) | 4 | GPIO22 (OUT2) |
| 5 | GPIO21 (OUT3) | 6 | GPIO20 (OUT4) |
| 7 | GPIO19 (OUT5) | 8 | Not connected (OUT6, reserved) |
| 9 | +3V3 | 10 | +3V3 |
| 11 | GND | 12 | GND |
| 13 | +5 V | 14 | +5 V |

Per the guide, the +5 V pins can be a power input or output (5 V ± 5 %), and +3V3 is intended for low-power modules. The current budget is UNKNOWN.

### Other connectors (all C)

- **J2 camera, 15-pin 1.0 mm:**

  | Pin | Signal | Pin | Signal |
  |---|---|---|---|
  | 1, 4, 7, 10 | GND | 11 | CSI_EN (10 kΩ to 3V3) |
  | 2 / 3 | Data lane 0 − / + | 12 | Not connected |
  | 5 / 6 | Data lane 1 − / + | 13 | SCL (GPIO15) |
  | 8 / 9 | Clock − / + | 14 | SDA (GPIO14) |
  | | | 15 | +3V3 |

- **J3 touch, 6-pin 0.5 mm:** 1 GND, 2 SDA, 3 SCL, 4 +3V3, 5 INT, 6 RST.
- **J4 panel, 40-pin 0.5 mm:**

  | Pins | Signal | Pins | Signal |
  |---|---|---|---|
  | 1 | LEDK | 30 | CLK |
  | 2 | LEDA | 31 | DISP |
  | 3 | GND | 32 | HS |
  | 4 | VCC 3.3 V | 33 | VS |
  | 5–12 | R0–R7 | 34 | DE |
  | 13–20 | G0–G7 | 35 | Not connected |
  | 21–28 | B0–B7 | 36 | GND |
  | 29 | GND | 37–40 | Resistive-touch pins, not connected |

## ESP32-C5 module pins (U3, WT0132C5-S5)

| Module pin | C5 signal | Net |
|---|---|---|
| 3 | EN | EN: 10 kΩ / 1 µF to ESP_VCC; J6-5 |
| 8 | VCC | ESP_VCC (MT9700 output) |
| 9 | IO13 | SDIO_D3 |
| 10 | IO14 | SDIO_D2 |
| 11 | IO10 | SDIO_CMD |
| 12 | IO9 | SDIO_CLK |
| 13 | IO8 | SDIO_D0 |
| 14 | IO7 | SDIO_D1 |
| 17 | IO27 | ESP_HI (10 kΩ pull-up) |
| 18 | IO28 | BOOT (10 kΩ pull-up; J6-6) |
| 21 / 22 | RXD0 / TXD0 | J6-4 / J6-3 |
| Others | IO0–2, IO4–6, IO24–26 | Not connected |

The strap functions of C5 IO27/IO28 are PROVISIONAL until checked against the ESP32-C5 datasheet.

## I2C address map

| Address | Device | Status |
|---|---|---|
| 0x5A | CST3240 touch | C (vendor firmware) |
| Varies | Camera module on J2, if fitted | U |

## Expansion feasibility (provisional)

Prefer GPIO19–23 for optional sensors: they are plain GPIOs and ADC1-capable. Use GPIO26/27 last, because they are USB PHY pins and need explicit GPIO configuration.

| Need | Option | Pins | Notes |
|---|---|---|---|
| Ambient light (analog) | Phototransistor or LDR divider | One of GPIO19–23 (ADC1) | Simplest option; calibrate in place |
| Ambient light / proximity (digital) | I2C sensor on its own bus | Two of GPIO19–23, with their own pull-ups | Keep it off the touch/camera bus |
| Presence | PIR or radar module, digital output | One of GPIO19–23 (interrupt) | Check module supply against the J9 3V3/5 V budget |
| Backlight | — | GPIO16 only | No external dimming input |
| Extra UART/SPI | GPIO matrix | GPIO19–23 | |

**Allocation is not final.** Before any sensor code: confirm the J9 pin map by continuity, measure the 3V3/5 V budget, and review the ESP32-P4 datasheet (strap functions, ADC channels, power domains).
