# LaunchPad

A standalone, wall-mounted **ESP32-P4 + ESP32-C5 touchscreen departure dashboard** for field-service work. It is designed to sit beside the keys and answer: *Where am I going next, when, and how current is this schedule?*

**Status:** planning stage; no firmware, hardware validation or company integration completed.

## Planned releases

- **M0 Foundation:** verify exact board, architecture and test fixtures.
- **M1 v0.1:** ESP-IDF/LVGL offline clock and appointments from synthetic data.
- **M2 v0.2:** Wi-Fi/time sync, freshness, optional weather/personal calendar, night dim.
- **M3 v0.3:** departure/leave-by UX, optional sensors, 3D-printed wall enclosure and measured power validation.
- **M4 v1.0:** employer-approved Simpro bridge and read-only assigned schedule.

See the [documentation index](docs/wiki/Home.md) and [roadmap](docs/wiki/Roadmap.md).

## Ground rules

No Dragon dependencies. No Simpro/customer data or secrets in this public repository. Do not connect to the employer's Simpro environment without approval. Appointments remain editable in the official Simpro mobile app; this is a glanceable companion. If the network or upstream data is stale, say so explicitly.

Development starts with fictional fixtures. Exact PCB revision, controllers, pinout, bus, power requirements and API fields are not yet confirmed. Use feature branches/PRs and validate real ESP32-P4 hardware before claiming functionality.

Licensed under GPL-3.0 (existing LICENSE file).
