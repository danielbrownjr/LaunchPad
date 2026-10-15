# Hardware and wall mounting

## Planning inputs
- Existing ESP32-P4 + ESP32-C5 4.3-inch touch board; advertised 800 × 480. **Exact board revision not yet confirmed**.
- Intended mount: entryway wall beside the keys.
- Outlet roughly 5 ft below and 2 ft to the right. Straight-line distance √29 ≈ 5.39 ft, practical down-and-across route ≈ 7 ft plus slack.
- 10 ft USB cable is a starting layout estimate, **not yet electrically validated**.
- Printed low-profile, removable enclosure with concealed cable run.

## Before selecting parts
Verify PCB pinout, board revision, USB input, display/touch drivers, C5 firmware, exposed GPIO, PSRAM/flash, cable exit, mounting holes and vendor license. Measure idle/peak input current (maximum backlight and active Wi-Fi); measure voltage at board under load and inspect brownout/heat. Pick a correctly rated, certified USB supply. Do not put mains wiring into the 3D-printed housing.

## Enclosure
Cable strain relief, ventilation, wall clearance, non-glare viewing angle, removable/serviceable mount, access to USB and recovery controls. Ambient/proximity sensing is optional and must not block screen or require undocumented GPIO.

## Physical acceptance
Power cycle; extended max-brightness soak; radio connection and reconnect; night dim/wake; voltage drop test with actual cable; enclosure thermal check; touch usability when wall mounted.
