# LaunchPad S4: real LVGL agenda, synthetic data only

Status: stacked draft candidate on S3 PR #47. S4 introduces no bridge,
Simpro integration, live jobs, credentials, cloud calls or new FreeRTOS task.
Dan authorized Claude to flash an **older, already-merged S2 image** separately.
S4 itself must not be flashed or merged as a side effect of code review.

## Data flow and single owner

`lp_agenda_demo_poll()` publishes validated, clock-gated fictional data to
the bounded S1 store. The existing app_main loop then calls
`lp_dashboard_update_agenda()` after each once-per-second producer poll.

The dashboard takes **one atomic store copy** into a static buffer, calls
S3's pure-C `lp_agenda_vm_build` using the real SNTP-confirmed clock and
separate network state, then invokes the S4 LVGL-only
`lp_agenda_ui_apply`. No provider task touches LVGL. No LVGL callback
updates the store or contacts the network. No new task or timer is introduced and the eight-row details tree is
created once. Label text changes can still allocate inside LVGL; callers
avoid resubmitting identical strings to reduce redraw and allocator churn.

Main-task stack safety: both the ~1.4 KiB store read buffer and the
view model live in static storage, **not** automatic locals on the 8 KiB
LVGL task. The provider's existing static scratch buffers remain unchanged.

`lp_clock_timezone_init()` is explicitly called before display creation.
The existing network worker also calls it at its startup, before SNTP
establishes clock validity. No UI appointment conversion should race a
network-worker timezone setup: there are no appointments before genuine
clock sync in current S2, and both calls set the same timezone. This also
establishes an explicit precondition for future no-sync last-known data.

## 800x480 home screen

- Left card: verified Eastern wall clock, SYNCED / OFFLINE TIME /
  LAST SYNC >24H / CLOCK NOT SET.
- Right card: source and status on independent labels; a hero time, bounded
  title/location, NOW UNTIL / IN N MIN when trustworthy, and the next
  future appointment as THEN. If not trusted, show a truthful no-confirmed-next
  message and the source age or error context, rather than an invented NEXT.
- Bottom card: two compact relevant agenda entries, tap anywhere on the
  card to open the full agenda.
- Footer: network transport separately from schedule source/freshness.
- Top-right existing gear + Wi-Fi indicator remain, with their previously
  tested networking and keyboard behavior unchanged.

The full agenda is a scrollable, within-viewport modal with reusable rows
and a 44-pixel-close button. It shows local date, EDT/EST, tags and
locations for all records, including DONE, CANCELLED and UNSCHEDULED.
The S3 model determines whether rows must be dimmed and whether NOW/NEXT
are permitted. S4 only renders that model.

## Settings diagnostics migration

The previous BOARD BRING-UP title, TAP TO TEST control and RGB swatches
are no longer on the home view. Settings (existing gear) now includes a
`Diagnostics` button, opening an on-demand overlay with board/PSRAM/touch
status, the RGB fixture, and touch counter. This change does not alter
Wi-Fi provisioning, password masking, NVS storage, scans or the C5 driver.

## LVGL 9.4 rendering rules found in review

These were reproduced with a headless LVGL 9.4.0 host render of the real
dashboard, agenda and Wi-Fi UI sources:

- **Single-line labels need a fixed height.** `LV_LABEL_LONG_MODE_DOTS` only
  truncates a fixed-height label. With content height, a 48-byte title
  wraps and overprints the location line. `text_at()` sizes each label to
  one font line.
- **DOTS rewrites the label's stored text.** `lv_label_get_text()` returns
  the truncated `...` form, so a text comparison never matches a truncated
  label. `lp_agenda_ui_apply()` therefore returns early when the view model
  is byte-identical to the last applied one. The S3 builder zero-fills the
  view first, which keeps that comparison exact. In the harness, an
  unchanged schedule then produced no flushes over 10 seconds, where it
  produced 50 before.
- **Hiding rows does not clamp the scroll offset.** A list scrolled to the
  bottom of 8 rows showed nothing after an update to 2 rows. The list
  returns to the top when the agenda opens and when the row count changes.
- **Empty wording follows the state.** "No appointments scheduled" appears
  only for an accepted CURRENT, NO_JOBS or DEMO schedule. Missing, rejected,
  unsynchronized or failed data says so instead.
- **Settings raises itself** when opened, because the agenda overlay does.

The LVGL built-in heap on the device is 64 KiB (`work_mem_int`). With every
screen created (home, agenda, Settings, keyboard and Diagnostics), the
64-bit host build peaked at 52.7 KB. Usage stayed flat across 50
open/close cycles. The 32-bit device uses less, but should report
`lv_mem_monitor()` before more UI is added.

Unicode: the built-in Montserrat 14/20/40 fonts contain only U+0020-U+007F,
`°` and `•`, so `é`, `ü`, `ã` and `—` render as placeholder boxes. Adding
U+00A0-U+00FF to subset fonts is estimated at about 5 KB at 14 px and 9 KB
at 20 px. The 40 px clock needs digits only. This fits the 62 KiB left in the
factory partition, but belongs in its own font change. Until then, real
non-ASCII labels are not acceptable for display.

## Tests / required acceptance

- Native S3 view-model tests remain the truth/policy gate.
- `test_agenda_ui_contract.py` guards static LVGL ownership, fixed-size
  objects, model-only rendering, once-per-second pipeline ordering,
  diagnostics relocation, and screen card bounds.
- Exact-head ESP-IDF 6.0.3 ESP32-P4 firmware CI must pass, including the
  partition-fit test. The physical factory app partition remains 1 MiB.
- **Hardware visual acceptance is still pending.** Check 800x480 layout,
  touch target, agenda scrolling, Settings and Wi-Fi keyboard overlay
  ordering, font/glyph legibility from wall distance, network and demo
  truthfulness, and recoverability on the actual P4 before S4 merges.
- **Unicode glyphs are an explicit unresolved display acceptance item.**
  S3 preserves UTF-8 correctly, but stock LVGL Montserrat may lack
  characters such as é. Do not claim international labels render correctly
  without subset-font proof and flash-size budgeting. The firmware must
  not silently strip or ASCII-transliterate source data.
- No unverified LDO4 voltage change, bootloader/partition/C5 flash, NVS
  erase, real employer data or another overnight soak.

S5 can later produce host 800x480 screenshot artifacts for each state.
Those are useful review evidence but do not replace the first short,
operator-approved screen/touch check.
