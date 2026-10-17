# LaunchPad agenda v1: provider-neutral display contract

Status: **proposed, synthetic-only M0 slice** for issues #4 and #13. This is
NOT an observed Simpro schema or an authorized API integration. Source data
remains fictional. The bridge endpoint, authentication and JSON parser are
future work, with live access gated by employer/IT approval (#21–#24).

## Boundaries

The wire/display DTO uses version `1` and UTC timestamps in full RFC 3339
notation with an explicit offset (`Z` preferred). Wall rendering converts
through the device's already-verified Eastern POSIX timezone and does not
manually subtract offsets or assume 24-hour days around DST boundaries.
`time_zone` is fixed to `America/New_York` for this prototype. Other zones
are **not** implicitly supported; a future bridge must validate and normalize
them instead of silently displaying the wrong time.

```json
{
  "schema_version": 1,
  "source": "demo",
  "generated_at": "2026-10-17T12:00:00Z",
  "source_updated_at": "2026-10-17T11:59:00Z",
  "time_zone": "America/New_York",
  "ttl_seconds": 3600,
  "appointments": [
    {
      "id": "demo-a",
      "status": "scheduled",
      "starts_at": "2026-10-17T14:30:00Z",
      "ends_at": "2026-10-17T16:00:00Z",
      "label": "Example instrument visit",
      "location_label": "Sampletown, MA"
    }
  ]
}
```

The example above is **synthetic**, not a real job. No field name assumes a
particular Simpro tenant or job API. `source="bridge"` is reserved for a
future authorized, authenticated, minimal read-only bridge. The firmware does
not contact one today.

### Normalization and limits

| Property | Contract |
|---|---|
| `schema_version` | Exactly integer `1`. Unknown versions rejected |
| `source` | `demo` or reserved `bridge`; no current live provider |
| `time_zone` | Explicit `America/New_York` for this prototype |
| `generated_at`, `source_updated_at` | Explicit-offset timestamps, normalized to signed 64-bit UTC Unix seconds; source update must not follow generation |
| `ttl_seconds` | Integer 60–86400 inclusive; bounded freshness horizon |
| `appointments` | Array of 0–8; no unbounded allocations |
| `id` | 1–24 ASCII bytes, letters/digits/`._-`; unique within snapshot; opaque and never shown |
| `label` | 1–48 UTF-8 bytes, well-formed, no control characters |
| `location_label` | 0–64 UTF-8 bytes; empty means unavailable, not a fabricated location |
| `status` | `scheduled`, `cancelled`, `unscheduled` |
| `starts_at`, `ends_at` | UTC with explicit offset; for scheduled/cancelled: both present, end greater than start, duration at most seven days; for unscheduled: both JSON null |
| Unknown keys | Reject at trust boundary until schema extension is versioned |

`main/agenda_model.{h,c}` implements the allocation-free, provider-neutral
in-memory form of this contract. The proposed wire keys map to UTC second
integers; **no JSON parsing or network bridge is implemented by this slice**.
A future parser must reject invalid/malformed JSON, duplicate keys, non-finite
numbers, unsigned overflow, bad UTF-8, unrecognized enum values, and
unexpected optional/required fields *before* populating the in-memory DTO.
Do not claim source-level fixture checks replace runtime parser tests.

### Truthfulness and freshness

`DEMO` always remains `DEMO`, even with a functioning Wi-Fi connection.
`CURRENT` requires a clock established by real time sync, a valid, recent
bridge snapshot and its upstream `source_updated_at` no older than the TTL.
A delayed bridge response alone cannot magically freshen old source data.
`STALE` preserves provenance and allows a future UI to show older records
with explicit age; it must **not** display them as live jobs. `NO_JOBS`
requires an otherwise current authorized schedule with no upcoming relevant
appointments. A cold boot without time sync is `CLOCK_UNKNOWN`, not
`CURRENT`. `OFFLINE` is a separate transport state: Wi-Fi status never
substitutes for schedule freshness.

`lp_agenda_next()` chooses the earliest non-ended **scheduled** appointment,
including an in-progress appointment, with a stable ID tie-breaker for
overlapping/equal timestamps. Cancelled and unscheduled appointments are
never chosen. End times are exclusive.

### Deliberate limitations

- No real work/customer records or job numbers, tokens, or Simpro API calls.
- No on-device persistence or bridge credentials.
- No new screens in this M0 model change. The current on-screen job card is
  still hard-coded and explicitly marked fictional; #8/#9 are the M1 UI step.
- Appointment list and display strings are deliberately bounded; longer
  upstream text needs a documented truncation policy at the future bridge,
  not silent overflow on the MCU.
- `source_updated_at` means a *confirmed* upstream change timestamp, not
  the time a client simply fetched cached information. If a future source
  cannot provide it, the bridge must expose that limitation, not fabricate it.

## Tests

`tests/fixtures/agenda_cases.json` includes zero, one, many, overlaps,
in-progress, cross-midnight, a DST fall-back repeated hour, stale, no-jobs,
future-dated, cancelled/unscheduled, and intentionally invalid data. Every
case states its expected validity, next appointment and view state.

- `test_agenda_schema.py` checks the proposed wire contract with a Python
  reference validator, selector and view assessment.
- `test_agenda_native_fixtures.py` maps the same fixtures into the C DTO,
  compiles `main/agenda_model.c` with CI's warning flags and checks that the C
  model reaches the same validity, next appointment and view. Cases only a
  JSON parser can reject (offsetless timestamps, unknown keys) are listed
  explicitly. It skips locally without a C compiler but fails in CI.
- `test_agenda_model.c` covers native boundaries: UTF-8 edges, field caps,
  exact TTL and five-minute skew limits.

The existing DST tests cover the device timezone formatter; no time-zone
conversions occur in the in-memory selection routine.
