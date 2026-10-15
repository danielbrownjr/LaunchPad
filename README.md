# LaunchPad

A standalone, wall-mounted **ESP32-P4 + ESP32-C5 touchscreen departure dashboard** for field-service work. It is designed to sit beside the keys and answer: *Where am I going next, when, and how current is this schedule?*

**Status:** hardware identity, initial power rails, factory RGB/touch operation and 16 MiB flash backup have been validated. A first offline ESP32-P4/LVGL firmware slice exists in [draft PR #31](https://github.com/danielbrownjr/LaunchPad/pull/31), but **has not been flashed or physically tested**. No company integration exists.

## Planned releases

- **M0 Foundation:** verify exact board, architecture and test fixtures.
- **M1 v0.1:** ESP-IDF/LVGL offline clock and appointments from synthetic data.
- **M2 v0.2:** Wi-Fi/time sync, freshness, optional weather/personal calendar, night dim.
- **M3 v0.3:** departure/leave-by UX, optional sensors, 3D-printed wall enclosure and measured power validation.
- **M4 v1.0:** employer-approved Simpro bridge and read-only assigned schedule.

See the [documentation index](docs/wiki/Home.md) and [roadmap](docs/wiki/Roadmap.md).

## Ground rules

No Dragon dependencies. No Simpro/customer data or secrets in this public repository. Do not connect to the employer's Simpro environment without approval. Appointments remain editable in the official Simpro mobile app; this is a glanceable companion. If the network or upstream data is stale, say so explicitly.

Development uses fictional fixtures. The PCB is physically identified as ZX4D30CE40S-V13 with ESP32-P4 and ESP32-C5; PSRAM capacity, our firmware's panel timing, full-load electrical behavior and API fields are not yet confirmed. Use feature branches/PRs and validate real ESP32-P4 hardware before claiming functionality.

Licensed under GPL-3.0 (existing LICENSE file).
