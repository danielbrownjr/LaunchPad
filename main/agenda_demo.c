#include "agenda_demo.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "agenda_store.h"
#include "clock_util.h"

typedef struct {
    const char *id;
    const char *label;
    const char *place;
    int day_offset;
    int hour;
    int minute;
    int duration_minutes;
} demo_slot_t;

/* Deterministic fictional records, never customer data. The next-day entry
 * ensures the demo has an upcoming appointment even late at night. */
static const demo_slot_t SLOTS[] = {
    {"demo-morning", "Fictional instrument check", "Sampletown, MA",
     0, 9, 30, 75},
    {"demo-midday", "Fictional calibration visit", "Example City, MA",
     0, 12, 30, 60},
    {"demo-afternoon", "Fictional service follow-up", "Mocksville, MA",
     0, 16, 0, 90},
    {"demo-tomorrow", "Fictional morning visit", "Sampletown, MA",
     1, 9, 30, 75},
};

_Static_assert(sizeof(SLOTS) / sizeof(SLOTS[0]) <= LP_AGENDA_MAX_ITEMS,
               "Demo exceeds bounded model capacity");

/* mktime is deliberately applied to local calendar fields. Adding 86400
 * seconds is wrong when an Eastern calendar day lasts 23 or 25 hours. */
static bool local_slot_epoch(const struct tm *today, const demo_slot_t *slot,
                             int64_t *start_utc)
{
    struct tm local = {
        .tm_year = today->tm_year,
        .tm_mon = today->tm_mon,
        .tm_mday = today->tm_mday + slot->day_offset,
        .tm_hour = slot->hour,
        .tm_min = slot->minute,
        .tm_sec = 0,
        .tm_isdst = -1,
    };
    const time_t epoch = mktime(&local);
    if (epoch == (time_t)-1 || local.tm_hour != slot->hour ||
        local.tm_min != slot->minute || local.tm_sec != 0 ||
        (int64_t)epoch <= 0)
        return false;
    *start_utc = (int64_t)epoch;
    return true;
}

bool lp_agenda_demo_build(int64_t now_utc, bool clock_valid,
                          lp_agenda_snapshot_t *out)
{
    if (!out || !clock_valid || now_utc < (int64_t)LP_CLOCK_MIN_EPOCH)
        return false;

    const time_t now = (time_t)now_utc;
    if ((int64_t)now != now_utc) return false;

    struct tm today;
    if (localtime_r(&now, &today) == NULL) return false;

    int64_t starts[sizeof(SLOTS) / sizeof(SLOTS[0])];
    for (size_t i = 0; i < sizeof(SLOTS) / sizeof(SLOTS[0]); ++i) {
        if (!local_slot_epoch(&today, &SLOTS[i], &starts[i]) ||
            starts[i] > INT64_MAX - (int64_t)SLOTS[i].duration_minutes * 60)
            return false;
    }

    memset(out, 0, sizeof(*out));
    out->schema_version = LP_AGENDA_SCHEMA_VERSION;
    out->source = LP_AGENDA_SOURCE_DEMO;
    out->generated_at_utc = now_utc;
    /* Synthetic generation time, NOT evidence of a successful upstream
     * check: DEMO always remains DEMO irrespective of these timestamps. */
    out->source_checked_at_utc = now_utc;
    out->ttl_seconds = LP_AGENDA_TTL_MAX_S;
    out->count = sizeof(SLOTS) / sizeof(SLOTS[0]);
    for (size_t i = 0; i < out->count; ++i) {
        lp_agenda_item_t *item = &out->items[i];
        const demo_slot_t *slot = &SLOTS[i];
        /* Strings are literal, shorter than the fixed field capacities. */
        (void)strcpy(item->id, slot->id);
        (void)strcpy(item->label, slot->label);
        (void)strcpy(item->location_label, slot->place);
        item->status = LP_AGENDA_SCHEDULED;
        item->starts_at_utc = starts[i];
        item->ends_at_utc = starts[i] + (int64_t)slot->duration_minutes * 60;
    }
    return lp_agenda_validate(out);
}

bool lp_agenda_demo_poll(int64_t now_utc, bool clock_valid)
{
    /* Called only by app_main, once per second. Static storage avoids adding
     * a ~1.4 KiB DTO plus a store state to app_main's 8 KiB stack. */
    static lp_agenda_snapshot_t scratch;
    static lp_agenda_store_state_t stored;
    static bool published;
    static int last_year;
    static int last_yday;

    if (!clock_valid || now_utc < (int64_t)LP_CLOCK_MIN_EPOCH) return false;
    const time_t now = (time_t)now_utc;
    if ((int64_t)now != now_utc) return false;

    struct tm local;
    if (localtime_r(&now, &local) == NULL) return false;
    if (published && local.tm_year == last_year && local.tm_yday == last_yday)
        return false;

    if (!lp_agenda_store_read(&stored)) return false;
    /* Future bridge-owned data must never be silently replaced by DEMO,
     * including retained data in FETCH_ERROR. No bridge exists in S2. */
    if (stored.has_snapshot && stored.snapshot.source == LP_AGENDA_SOURCE_BRIDGE)
        return false;

    if (!lp_agenda_demo_build(now_utc, true, &scratch) ||
        !lp_agenda_store_publish(&scratch))
        return false;
    published = true;
    last_year = local.tm_year;
    last_yday = local.tm_yday;
    return true;
}
