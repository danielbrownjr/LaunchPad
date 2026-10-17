# LaunchPad S1: bounded agenda store

Status: **review candidate**, after the agenda v1 DTO and tests in merged PR #44.
This slice stores state only. It does not introduce a demo provider, JSON
parser, Simpro access, UI rendering, background polling or persistence.

## Data flow and ownership

Future producer task → `lp_agenda_store_publish()` → bounded snapshot copy
under FreeRTOS mutex → `lp_agenda_store_read()` → future view model on the
**single** LVGL-owning task.

The store is initialized once on `app_main` before network tasks start.
Subsequent `init` calls do nothing. Concurrent init is unsupported, by
design. Every operation is task-context only, never ISR-context.

The entire source DTO stays bounded to 8 records (static compile-time
assertion: ≤ 2 KiB). One copy of its data is held in static memory. Creating
the FreeRTOS mutex allocates a small OS synchronization object once at boot;
publication and read make **no heap allocations**. Validation runs on a
temporary caller-task stack copy before acquiring the lock, so malformed
records never replace the last-good snapshot. Inputs must remain stable
until `publish` returns; this cannot make racing writes to a caller-owned
input buffer safe.

Caller stack cost is bounded but not small. On ESP32-P4 (RV32 ILP32,
ESP-IDF 6.0.3 GCC `-Os`) a snapshot is 1,376 bytes and a store state is
1,392 bytes; `publish` uses a 1,408-byte frame plus validation, and `read`
needs a caller-supplied 1,392-byte output. Size provider task stacks for
that, and prefer a static output buffer on the LVGL task.

No network I/O, time conversion, LVGL calls, log formatting, or error
handling occurs while holding the mutex. Snapshot copying and provider
status updates are the only lock-protected operations. A store lock waits
indefinitely in task context; callers must never hold another lock that
re-enters the store or invoke these APIs from an ISR.

## Truthful provider status

| Status | Snapshot | Meaning |
|---|---|---|
| NO_DATA | absent | Nothing has been successfully published yet |
| OK | present | A validated snapshot was published; **not proof of fresh/current schedule** |
| FETCH_ERROR | absent or preserved | Upstream/provider read failed. Keep the last successful data and timestamps unchanged for an explicitly degraded display |

A successful `publish` switches to OK and bumps a saturating 64-bit
revision. A new transition into FETCH_ERROR also bumps revision. Repeated
error reports do not bump it. Invalid input never changes the revision,
status or stored bytes. Readers receive a *single consistent copy* of the
snapshot, status, `has_snapshot`, and revision under the same mutex. The
snapshot is meaningless when `has_snapshot == false`.

Even status OK is not `CURRENT`: call the existing `lp_agenda_view` logic
on a valid clock and the **stored** `generated_at_utc` and
`source_checked_at_utc`. Fetch failure must be visible even when a retained
snapshot's TTL has not yet expired. A future view model handles offline
network status *independently* of the provider and data freshness.

## Failure and recovery policy

- Mutex initialization failure is logged; LaunchPad's existing clock/Wi-Fi
  bring-up continues. Publish/read/error calls then return `false`.
- No source is active in S1. Boot state is NO_DATA, not an invented schedule.
- If a future provider fails, it calls `mark_fetch_error`. Last-good data
  remains available but must display a degraded/error banner.
- Successful next publish recovers to OK, without overwriting immutable
  timestamps with the time a cached copy was reread.
- Store state is volatile, not NVS. Cold reboot returns to NO_DATA.
- No real appointments, credentials, tokens, or company data are committed.

## Host and target verification

Native CI compiles the exact production `agenda_store.c`, not a reimplemented
Python store. A small test-only pthread shim supplies the FreeRTOS semaphore
API on Linux. Tests cover initialization/empty reads, invalid rejection (including while in
FETCH_ERROR), publication/copy isolation, retaining data during fetch failure,
recovery, revision semantics, simultaneous publishers/readers, and publishers
racing an error reporter and two readers. CI also runs the store tests under
ThreadSanitizer. The actual pinned
ESP-IDF 6.0.3 ESP32-P4 build compiles the real FreeRTOS version.

**Limits:** host pthread tests prove the store's data handling under a
functionally equivalent lock, not the timing or behavior of the actual
FreeRTOS scheduler. S2 will add the deterministic fictional local-day
producer, S3 view model, and S4 LVGL agenda UI. No board flashing or long
soak is authorized by this PR.
