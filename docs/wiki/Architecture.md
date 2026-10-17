# Architecture and data contracts (draft)

```text
Simpro API (only when authorized)         Optional personal sources
               \                                  /
                  Local bridge service
           auth • filter • TTL • bounded cache
                            |
                authenticated LAN transport
                            |
                ESP32-C5 Wi-Fi coprocessor
                            |
              verified P4↔C5 transport
                            |
                    ESP32-P4 + LVGL
            clock • next call • day • sync status
```

The first firmware runs from **synthetic fixtures**, independently of a bridge. The bridge might be Python/FastAPI, but runtime host, transport and credentials method are not selected.

## Main screen
Large current time, next appointment/time, location label, what comes next, and a clear last-successful-data timestamp. Touch shows the day. Optional leave-by estimate has source and freshness and must not invent travel time.

## State model
DEMO / CURRENT / STALE / OFFLINE / NO_JOBS / CLOCK_UNSYNCED / ERROR. Wi-Fi connected is not proof of current schedule data.

## Draft JSON fields
`schema_version`, `generated_at`, `source`, `source_checked_at`, `status`, `time_zone`, and bounded `appointments[]` with `id`, `starts_at`, `label`, `location_label`, `job_ref`. Treat these as a proposal and verify actual Simpro schema after approval.

Reject invalid timestamps, oversized strings/lists and malformed JSON. Transport timestamps in UTC or with an explicit offset; render with verified local timezone/DST handling.

## Safety / privacy boundary
Firmware contains no company-wide credentials. Local bridge validates its clients and minimizes data exposed. Public repo contains no customer PII, production snapshots, tokens or logs.
