# Development and testing

Follow the canonical **General Standing Orders (GSO) — Repository & Coding Workflows** stored in Mem. Use feature branches and review PRs. Do not merge/release/flash/deploy as a side effect of completing a code task.

## Technical direction (not yet verified)
ESP-IDF targeting ESP32-P4, LVGL for UI, vendor display/touch drivers only after source and revision inspection, and supported P4↔C5 networking. Keep rendering independent of data sources with a mock provider and versioned bounded DTO.

## Automated tests
Fixture loading; bad/oversized JSON; missing appointments; out-of-order schedules; next appointment selection; event times crossing midnight; DST and unsynchronized clock; stale/timeout transitions; reconnect/retry/backoff; malicious input rejected; optional provider outages; no unintended mutations.

## Physical checks
ESP32-P4 build and boot, display and touch, C5 networking, backlight behavior, reset/brownout, UI at wall distance, adapter/cable voltage and thermal soak.

CI green ≠ physical validation. Do not call data Simpro-supported until an authorized integration test proves it.

## Codex session boundary
Implement only the specifically authorized milestone slice. Read current repo/PR/issue state, implement/tests/docs, produce a review PR, confirm exact-head CI if relevant, and **stop**. Never silently merge, release, publish real company data, flash, or start the next milestone.
