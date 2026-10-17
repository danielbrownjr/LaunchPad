# LaunchPad S3: bounded agenda presentation model

Status: draft review candidate, after merged #44 (bounded DTO), #45 (store)
and #46 (fictional Eastern-day provider). **No LVGL integration is included
here.** The screen still displays its existing hard-coded fictional cards
until S4 implements rendering.

## Contract

`lp_agenda_vm_build(store_copy, now_utc, clock_valid, network_online, out)`
is a **pure C** transformation of a single atomic store read into a fixed-
capacity screen model. It depends on the already documented Eastern POSIX
timezone being initialized, but never initializes it, reads FreeRTOS state,
calls a provider, alters the store, or touches LVGL. The caller supplies
real clock validity from the trusted SNTP callback and passes Wi-Fi/link
state separately. A successful Wi-Fi connection is never proof of fresh
schedule data.

All text is bounded and NUL terminated. Original UTF-8 appointment strings
are preserved byte for byte, never silently ASCII-converted or truncated.
The planned S4 font must support the relevant subset of Latin glyphs; text
rendering in picolibc/LVGL on physical P4 is **not** yet validated.

The future UI should use a statically allocated view-model output on the
8 KiB app_main/LVGL task stack, not allocate its several-kilobyte result
as a large local variable every second. S3 compilation introduces no new
FreeRTOS task, timer, provider or network operation.

## Visual truth table

| Store/provider input | Screen state | Hero/THEN | Rows |
|---|---|---|---|
| No snapshot, NO_DATA | NO_DATA | None | None |
| No snapshot, FETCH_ERROR | ERROR | None | None |
| Retained snapshot, FETCH_ERROR | ERROR | None | Last-known, dimmed |
| Invalid data/provider combination | INVALID | None | None |
| Valid snapshot, never-sync clock | CLOCK_UNSYNCED | None | Absolute local date/time, dimmed, no relative labels |
| Valid DEMO with sync | DEMO | Upcoming/ongoing fictional hero, then next future | Full list; clear FICTIONAL provenance |
| Fresh BRIDGE with upcoming work | CURRENT | Ongoing job first, upcoming job as THEN; otherwise NEXT | Chronological list |
| Fresh BRIDGE, no unfinished scheduled work | NO_JOBS | None | Completed/cancelled items retained, not dimmed |
| Expired BRIDGE TTL | STALE | None | Last-known, dimmed |

**Important:** the model's DEMO classification intentionally bypasses
the real-bridge TTL. A fictional 25-hour fall-back local day is not
incorrectly STALE. Conversely a FETCH_ERROR must show an error even with
an otherwise unexpired last-good snapshot. A cached snapshot never renews
`source_checked_at`. With no valid clock, absolute appointment timestamps
may be formatted as local times, but nothing is called NOW/NEXT or treated
as genuinely active.

### Rows, selection and ages

Dated items sort by absolute UTC start, with ID tie-break; unscheduled items
follow dated items. Cancelled rows remain visible with CANCELLED. Completed
items are DONE; active appointments are NOW; the current hero's upcoming
item is NEXT; the next distinct future appointment is THEN; subsequent
future items are LATER. Row strings include *local date* plus clock and
zone abbreviation (EDT or EST) to distinguish repeated DST hours. The hero
includes a bounded relative label (e.g. IN 25 MIN, NOW UNTIL 10:45 AM EDT).
No fabricated traffic or departure estimate is emitted.

Bridge source-check ages are derived from the supplied verified clock and
stored `source_checked_at_utc`. Ages remain unknown without clock sync,
and future source check times are labelled ahead, not negative durations.
Demo shows FICTIONAL - NO LIVE SOURCE instead of a misleading upstream
check timestamp. The network-online boolean is separate from the state
and never changes how freshness is classified. S4 can display this as a
dedicated network indicator.

## Limits / review gates

- The `BRIDGE` source in tests is synthetic, only to check future error and
  freshness handling. No bridge or real employer records are used.
- `main/agenda_view.c` builds a result from an **already copied** store
  snapshot; S4 must call `lp_agenda_store_read` on app_main and guard
  against store-unavailable results.
- Store revisions signal *possible* changes; S4 should also refresh the
  relative time and clock validity once per minute/second as appropriate.
- Source strings fit the capped DTO exactly. The view model does not
  implement a hypothetical future JSON/Simpro parser.
- Local time `strftime` behavior is host-tested; ESP-IDF 6.0.3 links
  picolibc. Target CI checks compilation/size, but **hardware typography,
  glyph support and visual placement are S4 acceptance gates**.
- The existing `lp_clock_timezone_init` runs in the network worker.
  Before S4 uses the VM, ensure the timezone is initialized **before the
  first possible conversion** on the LVGL task, even if networking fails.
- No new task, firmware flash/reset, C5/partition changes or soak test.

## Verification

Native `tests/test_agenda_view.c` runs the *production* model, demo, store,
clock utility and view builder via the existing pthread FreeRTOS mutex shim.
Tests cover no data, errors before first data, preserved last-known data,
invalid snapshots, unverified clock, future/current/stale source checks,
offline-versus-fresh separation, DEMO attribution, ongoing hero + THEN,
deterministic rows including Unicode, cancelled/unscheduled and end times,
a 25-hour DST fall-back demo, explicit EDT/EST and source age formats.

The pinned ESP-IDF 6.0.3 ESP32-P4 build and partition-fit check are
required on the exact PR head before approval. Host CI treats warnings as
errors. S4 will consume this contract without duplicating policy in LVGL.
