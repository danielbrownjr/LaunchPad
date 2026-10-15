# Physical bring-up checklist — first test session

**Status: procedure only. Nothing in this document has been executed.** Record results in issue #2. Photos are welcome, but include no secrets, Wi-Fi passwords or employer data. Background: [board reconnaissance](board-reconnaissance.md), [pin/resource matrix](pin-resource-matrix.md).

## Ground rules

- **WRITE steps** change flash contents and need the maintainer's explicit authorization before they run. Every other step is read-only.
- **Use J10 only.** Plug one USB-C cable into J10 unless a step says otherwise. Both USB-C VBUS pins are tied to the same 5 V rail.
- **No adapter power leads.** Never connect a USB-UART adapter's power lead to J1 or J6 until step 2 resolves their power pins.
- **Gates stop the session.** If a GATE fails, stop and update the reconnaissance pages before continuing.

## Equipment

- 5 V USB supply rated at least 1 A.
- A short USB-C *data* cable and the intended ~10 ft cable.
- Inline USB power meter and a multimeter.
- 3.3 V USB-UART adapter with a 1.25 mm (Molex PicoBlade) lead, or clips for TP5–TP15.
- ESD precautions and a phone camera.
- Host computer with ESP-IDF v6.0.x and esptool. esptool v5 uses hyphenated commands (`chip-id`, `read-flash`); older versions use underscores.

### 1. Visual identification — GATE

1. Photograph both sides of the board.
2. Read the PCB silkscreen, the P4 module label, the wireless module label and any panel FPC markings.
3. Count the J9 pins and check whether the J7 footprint is populated.
4. Look for SW1 and for any BOOT button, and locate TP1–TP15.

**Expect:** `ZX4D30CE40S`, `V1.3`/`V13`; P4 module `WT0132P4-A1` (N16R32X); wireless module **`WT0132C5-S5`**; 14 J9 pins; J7 empty; SW1 is the only button.

**If different:**
- A C61 module means the C61 variant: change the coprocessor target and slave image.
- A different revision makes every CONFIRMED item in the reconnaissance PROVISIONAL.

### 2. Unpowered continuity — GATE

1. With the board unplugged, measure J1-1, J1-2, J6-1 and J6-2 against GND, J9-13 (+5 V) and J9-9 (+3V3).
2. Check the +5 V and +3V3 rails for a dead short to GND.

**Expect per schematic:**
- J1-1 = +5 V and J1-2 open.
- J6-1 open, and J6-2 = ESP_VCC (not continuous with J9-9, because the MT9700 switch sits between them).

The guide instead says pin 1 = +5 V and pin 2 = +3.3 V on both headers. Record which document matches the board.

### 3. First power-up

1. Connect the USB meter to J10, with no microSD card and nothing else attached.
2. Note what the factory firmware shows.
3. Measure J9-13/14 (+5 V), J9-9/10 or TP3 (+3V3), and the idle supply current.

**Expect:** both rails present. The factory firmware may or may not turn the backlight on.

**Failure:**
- No 3.3 V → unplug and stop (buck or short).
- Any component hot to the touch → unplug.

### 4. USB enumeration

List USB devices on the host (`lsusb`, macOS System Information, or Windows Device Manager).

**Expect:** Espressif VID `0x303A`, typically "USB JTAG/serial debug unit" (PID `0x1001`), appearing as `/dev/ttyACM*`, `/dev/cu.usbmodem*` or `COMx`. A second CDC device appears only on J5, and only if the factory firmware runs TinyUSB.

**Failure:**
- Try another cable; charge-only USB-C cables are common.
- The factory firmware may have repurposed GPIO24/25; use the manual download mode in step 7.

### 5. Serial console and chip identification (read-only)

1. Open a 115200-baud monitor on the J10 port and press SW1. Capture the full boot log.
2. Run `esptool --chip esp32p4 -p PORT chip-id` and `esptool --chip esp32p4 -p PORT flash-id`.

**Expect:**
- Chip revision **v3.x** (vendor builds need ≥ v3.1).
- 16 MB flash, and 32 MB PSRAM reported by the application.
- A normal SPI boot-mode line and no brownout messages.

**Record:** chip revision, flash ID and size, factory application name/version, the boot-mode/strap line (relevant to GPIO34), and any warnings. Redact MAC addresses before publishing if preferred.

**Failure:** a revision below v3.0 means the vendor images will not run. Stop and reassess the ESP-IDF configuration.

### 6. Full flash backup (read-only) — GATE before any WRITE

1. Run `esptool --chip esp32p4 -p PORT -b 921600 read-flash 0 0x1000000 p4-factory.bin`.
2. Record `sha256sum p4-factory.bin`.
3. Read a second time and compare the hashes.
4. Store the file outside the public repository.

**Expect:** a 16 MiB file, with identical hashes on both reads.

**Failure:** on read errors, retry at 115200 baud and check the cable.

### 7. Download and recovery mode

1. **Automatic:** repeat `chip-id` and confirm esptool enters download mode over USB-Serial/JTAG on its own.
2. **Manual:** hold BOOT (J1-6 or TP8) to GND, tap SW1, then release BOOT. The ROM should report it is waiting for download.
3. Tap SW1 to return to normal boot.

**Expect:** both paths work. Record which one is actually needed.

**Failure:** check BOOT continuity to GPIO35. As a fallback, use UART0 on J1 with only GND, TX and RX connected.

### 8. Minimal P4 boot — WRITE

1. Build ESP-IDF v6.0.x `hello_world` for `esp32p4`, with "<3.0 revisions" left unselected.
2. Flash it over J10 and open the monitor.

**Expect:** boot banner, chip revision, heap sizes including PSRAM, and periodic output. The display stays dark, because the backlight pull-down holds it off and the C5 is unpowered.

**Failure:**
- A chip-revision error from the bootloader → wrong revision configuration.
- A brownout boot loop → see step 15.
- To restore: `esptool --chip esp32p4 -p PORT write-flash 0 p4-factory.bin`.

### 9. Display initialization — WRITE

Flash either the vendor baseline `P4C5/hosted_p4c5_20260910_0x0.bin` at 0x0 (check its sha256 against the [source inventory](vendor-source-inventory.md)), or, when authorized, the first LaunchPad slice.

**Expect:** a stable image with no rolling or flicker, the backlight on, and colour bars in the right order (LaunchPad slice).

**Failure:**

| Symptom | Likely cause |
|---|---|
| Backlight on, but white or black screen | Panel timing or PSRAM |
| Screen completely dark | GPIO16 PWM; check the voltage across TP1–TP2 |
| Red and blue swapped | Bus order |
| Shimmer or noise | PCLK edge or rate, or bounce buffer |

### 10. Touch input

With the baseline image running, touch the four corners and the centre. Record the reported coordinates or the UI response.

**Expect:** the CST3240 answers at 0x5A, and coordinates span 0–799 × 0–479 in the display orientation.

**Failure:**
- No response → reset timing (the vendor waits 1.3 s), FPC seating, INT/RST wiring.
- Mirrored or swapped axes → swap/mirror flags.

### 11. C5 power and transport

1. With the baseline host image running, measure ESP_VCC (J6-2 / TP10).
2. Capture the ESP-Hosted log: transport up, coprocessor firmware version, chip type.
3. Watch ESP_VCC during a host reboot; the vendor config power-cycles the C5 on every host boot.

**Expect:**
- ESP_VCC ≈ 3.3 V while running.
- A successful handshake. The factory slave version is unknown; the vendor host image pairs with 2.12.3.
- During the reboot, ESP_VCC falls close to 0 V. Record the minimum: a substantial residual suggests back-powering through the SDIO lines.

**Failure:**
- Handshake timeout → slave version, target or checksum mismatch, or slave not flashed.
- ESP_VCC at 0 V while running → GPIO28 is not being driven.

### 12. C5 backup and recovery path

1. Run a host image that holds GPIO28 high, so the C5 is powered.
2. Connect the USB-UART to J6 with GND, TX→RX and RX→TX only.
3. Hold J6-6 (BOOT) low and pulse J6-5 (EN) low.
4. Back up the C5 (read-only): `esptool --chip esp32c5 -p PORT read-flash 0 0x400000 c5-factory.bin`.
5. **WRITE, only if needed:** `esptool --chip esp32c5 -p PORT write-flash 0x0 slave_p4c5_20260910_0x0.bin`. Do **not** use the vendor README's `load-ram` command; it does not persist.

**Expect:** an ESP32-C5 at revision v1.x with 4 MB flash.

### 13. Wi-Fi connectivity

1. With the baseline image, use its SoftAP (`QM-<MAC>`) provisioning to join the home network.
2. Try both 2.4 GHz and 5 GHz if available.
3. Note the RSSI at the wall position and let SNTP sync.

**Expect:** a DHCP lease, a time sync, and reconnection after an access-point reboot.

**Notes:**
- The vendor documents an interrupt-watchdog panic near its HTTP probe. If it appears, record it as a vendor-firmware issue.
- Credentials stay in device NVS. The vendor console command `e` erases them.

### 14. Backlight control — WRITE (test build)

Step the duty through 0, 20, 37, 38, 40, 50, 75 and 100 %. At each step, measure V(TP2) and V(TP1) − V(TP2), and note any visible flicker (also through a phone camera).

**Expect:**
- LED current ≈ V(TP2) / 3.3 Ω, reaching about 60 mA at full duty.
- Dropout at low duty; the vendor sets its knee at 38 %.

**Record:** the dimmest stable level, and whether it is acceptable at night.

### 15. Power and thermal measurement

Fill in this table for each load case:

| Case | Supply current (meter) | 5 V at board (J9-13) | 3V3 (TP3) | Notes |
|---|---|---|---|---|
| A — idle, backlight off, C5 off | | | | |
| B — backlight 100 %, C5 off | | | | |
| C — backlight 100 %, Wi-Fi connected and idle | | | | |
| D — backlight 100 %, Wi-Fi transfer | | | | |
| E — boot and C5-enable transients | | | | Oscilloscope if available |

- **Cable drop:** repeat cases B–D with the short cable and with the ~10 ft cable. Cable drop is the board voltage on the short cable minus the board voltage on the long cable.
- **Brownouts:** watch the logs for brownout resets.
- **Thermal:** after 60 minutes in case C, record the temperatures of the P4 module, U2, U6 with D10, the C5 module, and ambient.

### 16. Reset and recovery behaviour

Exercise each path:

1. SW1 reset.
2. Unplug and replug.
3. Host reboot with Wi-Fi up; the C5 should power-cycle and reconnect.
4. Access-point power cycle.
5. Download mode entered from a running application.
6. **WRITE:** restore `p4-factory.bin` once, to prove the backup works.

**Expect:** every path returns to a working state.

### 17. Record

Post the results to #2:

- Photos and module/PCB markings.
- Chip revision, flash and PSRAM.
- Enumeration result.
- J1/J6 pin findings.
- Measurements.
- Every deviation from these pages.

Then update the hardware pages. Issue #2 should close only after the identity gate and the measurements are recorded.
