# LaunchPad roadmap

These are **proposed** GitHub milestones. No milestone or release exists yet; dates are deliberately unset.

## M0: Foundation and board verification
Confirm actual PCB revision, display/touch controller, P4↔C5 connection, supported ESP-IDF toolchain, usable GPIO and power. Define UI states, mock fixtures, schema and architecture. Exit: verified assumptions and documented build plan.

## M1: v0.1 Offline Launchpad
ESP-IDF + LVGL clock and next appointment screen using fictional schedules. Day detail, touch, empty/offline/stale states. Exit: board boots, target build passes, actual display/touch smoke tests and mock data tests pass.

## M2: v0.2 Connected dashboard
C5 Wi-Fi networking, NTP/time zone, bridge connection/cache/freshness, optional personal calendar/weather, configurable night display. Exit: connectivity recovery, stale indicators, DST, and source isolation verified.

## M3: v0.3 Departure experience and enclosure
Optional traffic-based leave-by calculation, optional ambient/proximity wake, low-profile printable wall mount, certified USB adapter and cable route. Exit: validated supply voltage under peak load, enclosure temperature, strain relief and legibility.

## M4: v1.0 Authorized Simpro
Employer/IT authorization and security review before real access. Separate local bridge holds API credentials; expose filtered read-only assigned appointments. Exit: approved access, tested schema/mapping, authenticated device access, documented privacy, revocation and offline behavior.

M4 API integration is independently blocked by authorization even if mock-backed development proceeds. The mobile app remains authoritative for editing jobs.

## Non-goals
Dragon-core compatibility, completing jobs/timesheets, changing service records, and direct company tokens on ESP32.
