# LaunchPad S2: fictional Eastern-day agenda provider

Status: review candidate. S1's bounded store is merged, and S2 is a
**synthetic-only** producer. It does not read a service API, read company
records, add a network client, alter the LVGL screen, write NVS, or control
the ESP32-C5.

## Behavior

`main/agenda_demo.c` generates four deterministic appointments, all visibly
fictional and tagged `LP_AGENDA_SOURCE_DEMO`:

| Local Eastern day | Appointment | Start | Duration |
|---|---|---|---|
| Today | Fictional instrument check | 09:30 | 75 minutes |
| Today | Fictional calibration visit | 12:30 | 60 minutes |
| Today | Fictional service follow-up | 16:00 | 90 minutes |
| Tomorrow | Fictional morning visit | 09:30 | 75 minutes |

Names and locations are made up; no customer names, addresses, job IDs,
tokens or company identifiers are used. The tomorrow entry gives the
demonstration a legitimate *future fictional* appointment when the local
time is after the final same-day job. The next-job selector still ignores
jobs that have ended and never fabricates a real schedule.

The generator uses `localtime_r` and `mktime` with `tm_isdst=-1`,
combined with the same verified Eastern timezone used by the clock. It
increments a **calendar date**, not a fixed 86,400 seconds. When DST
starts, the difference between corresponding local appointments can be
23 hours; when DST ends, 25 hours. Two occurrences of the repeated
01:30 local time produce the same fictional appointments.

The data is validated using the existing bounded agenda model before
publication through the mutex-protected store. The `DEMO` provenance
never becomes CURRENT even when Wi-Fi is connected. The synthetic
`generated_at` and `source_checked_at` describe the demo generation
only and do not claim any verified upstream schedule check.

## Lifecycle

`app_main` retains sole ownership of LVGL. Its existing once-per-second
clock pass calls `lp_agenda_demo_poll(time(NULL), lp_network_clock_valid())`.
No new task or timer is created. The clock-valid flag comes only after a
real SNTP synchronization callback, not merely a plausible epoch or DHCP
association. Before clock synchronization, the store remains NO_DATA.

The provider publishes at most once per **Eastern local calendar day**.
It never repositions demo appointments during the same day. After midnight,
it publishes the next day's deterministic fictional schedule. If
publication fails, it can retry later without marking the day published.
For now, provider scratch and read buffers are static to avoid adding
multiple 1.4 KiB snapshot frames to the 8 KiB LVGL task stack. The
poll function must therefore be called only on `app_main`, not from
another task or an ISR.

S2 will **not overwrite** a snapshot marked as source BRIDGE, even when
the bridge provider reports FETCH_ERROR and its last-good records remain
stored. A future production/bridge mode should disable the demo source
explicitly; this guard is defense in depth, not authorization for
Simpro access. Only future approved provider code may create bridge data.

S2 is not a screen change: the on-screen appointment card remains the
explicitly synthetic placeholder until S3/S4 consume the store. Do not
claim visual acceptance, hardware stability, or UI integration yet.

## Verification

The native tests compile **real production C** for the model, store,
clock utility and demo provider, using the same test-only pthread
semaphore shim as S1. They cover invalid clocks, no initial data,
stable same-day generation, local midnight/year rollover, always having
an upcoming fictional appointment, spring DST 23-hour and fall DST
25-hour days, repeated-hour stability, store revision behavior, and a
synthetic BRIDGE record not being overwritten by the demo provider.
Warnings are errors. The existing ESP-IDF 6.0.3 ESP32-P4 CI and partition
fit check remain mandatory.

No flashing, reset, or new soak is authorized by this PR.
