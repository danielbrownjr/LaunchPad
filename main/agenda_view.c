#include "agenda_view.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "clock_util.h"

/* Source ID and UTF-8 display strings are already bounded and validated by
 * agenda_model. No truncation, heap, user-controlled format strings or LVGL. */

static bool local_stamp(int64_t utc, char out[32])
{
    if (utc <= 0) return false;
    const time_t t = (time_t)utc;
    if ((int64_t)t != utc) return false;
    struct tm local;
    if (localtime_r(&t, &local) == NULL) return false;
    return strftime(out, 32, "%b %d %I:%M %p %Z", &local) > 0;
}

static bool local_clock(int64_t utc, char out[24])
{
    if (utc <= 0) return false;
    const time_t t = (time_t)utc;
    if ((int64_t)t != utc) return false;
    struct tm local;
    if (localtime_r(&t, &local) == NULL) return false;
    return strftime(out, 24, "%I:%M %p %Z", &local) > 0;
}

/* Unscheduled items sort after dated items. Equal dates are stable by ID.
 * Cancelled items retain their dates for the day list. */
static bool earlier(const lp_agenda_item_t *a, const lp_agenda_item_t *b)
{
    bool a_unscheduled = a->status == LP_AGENDA_UNSCHEDULED;
    bool b_unscheduled = b->status == LP_AGENDA_UNSCHEDULED;
    if (a_unscheduled != b_unscheduled) return !a_unscheduled;
    if (!a_unscheduled && a->starts_at_utc != b->starts_at_utc)
        return a->starts_at_utc < b->starts_at_utc;
    return strcmp(a->id, b->id) < 0;
}

static void set_banner(lp_agenda_vm_t *out)
{
    const char *text;
    switch (out->state) {
        case LP_AGENDA_SCREEN_NO_DATA: text = "NO SCHEDULE DATA"; break;
        case LP_AGENDA_SCREEN_ERROR: text = "SCHEDULE SOURCE ERROR"; break;
        case LP_AGENDA_SCREEN_INVALID: text = "INVALID SCHEDULE DATA"; break;
        case LP_AGENDA_SCREEN_CLOCK_UNSYNCED: text = "CLOCK NOT SET"; break;
        case LP_AGENDA_SCREEN_DEMO: text = "FICTIONAL DEMO SCHEDULE"; break;
        case LP_AGENDA_SCREEN_CURRENT: text = "SCHEDULE CURRENT"; break;
        case LP_AGENDA_SCREEN_STALE: text = "SCHEDULE STALE"; break;
        case LP_AGENDA_SCREEN_NO_JOBS: text = "NO UPCOMING JOBS"; break;
        default: text = "INVALID SCHEDULE DATA"; break;
    }
    (void)strcpy(out->banner, text);
}

static void set_freshness(const lp_agenda_snapshot_t *snapshot,
                          int64_t now, bool clock_valid,
                          lp_agenda_vm_t *out)
{
    if (snapshot->source == LP_AGENDA_SOURCE_DEMO) {
        (void)strcpy(out->freshness, "FICTIONAL - NO LIVE SOURCE");
        return;
    }
    if (!clock_valid || now <= 0) {
        (void)strcpy(out->freshness, "LAST CHECK AGE UNKNOWN");
        return;
    }
    if (snapshot->source_checked_at_utc > now) {
        (void)strcpy(out->freshness, "UPSTREAM CHECK TIME AHEAD");
        return;
    }
    int64_t elapsed_s = now - snapshot->source_checked_at_utc;
    if (elapsed_s < 60) {
        (void)strcpy(out->freshness, "CHECKED LESS THAN 1 MIN AGO");
    } else if (elapsed_s < 3600) {
        (void)snprintf(out->freshness, sizeof(out->freshness),
                       "CHECKED %" PRId64 " MIN AGO", elapsed_s / 60);
    } else if (elapsed_s < 86400) {
        (void)snprintf(out->freshness, sizeof(out->freshness),
                       "CHECKED %" PRId64 " H AGO", elapsed_s / 3600);
    } else {
        (void)snprintf(out->freshness, sizeof(out->freshness),
                       "CHECKED %" PRId64 " D AGO", elapsed_s / 86400);
    }
}

static void invalidate(lp_agenda_vm_t *out)
{
    uint64_t revision = out->revision;
    bool online = out->network_online;
    memset(out, 0, sizeof(*out));
    out->revision = revision;
    out->network_online = online;
    out->state = LP_AGENDA_SCREEN_INVALID;
    set_banner(out);
}

bool lp_agenda_vm_build(const lp_agenda_store_state_t *store,
                        int64_t now_utc, bool clock_valid,
                        bool network_online, lp_agenda_vm_t *out)
{
    if (store == NULL || out == NULL) return false;

    /* Callers should keep this output static on the 8 KiB LVGL task:
     * the view is ~2 KiB. Never build it on a timer/ISR callback. */
    memset(out, 0, sizeof(*out));
    out->revision = store->revision;
    out->network_online = network_online;
    clock_valid = clock_valid && now_utc >= (int64_t)LP_CLOCK_MIN_EPOCH;

    if (store->provider < LP_AGENDA_PROVIDER_NO_DATA ||
        store->provider > LP_AGENDA_PROVIDER_FETCH_ERROR ||
        (store->provider == LP_AGENDA_PROVIDER_OK && !store->has_snapshot) ||
        (store->provider == LP_AGENDA_PROVIDER_NO_DATA && store->has_snapshot)) {
        invalidate(out);
        return true;
    }
    if (!store->has_snapshot) {
        out->state = store->provider == LP_AGENDA_PROVIDER_FETCH_ERROR
                   ? LP_AGENDA_SCREEN_ERROR : LP_AGENDA_SCREEN_NO_DATA;
        set_banner(out);
        return true;
    }

    const lp_agenda_snapshot_t *s = &store->snapshot;
    if (!lp_agenda_validate(s)) {
        invalidate(out);
        return true;
    }
    (void)strcpy(out->source, s->source == LP_AGENDA_SOURCE_DEMO
                               ? "FICTIONAL DEMO" : "BRIDGE DATA");
    set_freshness(s, now_utc, clock_valid, out);

    /* An upstream failure is NOT hidden by a still-unexpired cached record.
     * Likewise, a 25-hour fall-back day cannot stale fictional DEMO data. */
    lp_agenda_view_t freshness = lp_agenda_view(s, now_utc, clock_valid);
    if (store->provider == LP_AGENDA_PROVIDER_FETCH_ERROR) {
        out->state = LP_AGENDA_SCREEN_ERROR;
    } else {
        switch (freshness) {
            case LP_AGENDA_VIEW_DEMO: out->state = LP_AGENDA_SCREEN_DEMO; break;
            case LP_AGENDA_VIEW_CURRENT: out->state = LP_AGENDA_SCREEN_CURRENT; break;
            case LP_AGENDA_VIEW_STALE: out->state = LP_AGENDA_SCREEN_STALE; break;
            case LP_AGENDA_VIEW_NO_JOBS: out->state = LP_AGENDA_SCREEN_NO_JOBS; break;
            case LP_AGENDA_VIEW_CLOCK_UNKNOWN:
                out->state = LP_AGENDA_SCREEN_CLOCK_UNSYNCED; break;
            default: invalidate(out); return true;
        }
    }
    set_banner(out);

    bool active = (out->state == LP_AGENDA_SCREEN_DEMO ||
                   out->state == LP_AGENDA_SCREEN_CURRENT) && clock_valid;
    out->dim_schedule = !active;
    size_t order[LP_AGENDA_MAX_ITEMS];
    for (size_t i = 0; i < s->count; ++i) {
        size_t j = i;
        while (j > 0 && earlier(&s->items[i], &s->items[order[j - 1]])) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = i;
    }

    size_t hero = s->count; /* Missing sentinel. */
    size_t then = s->count;
    if (active) {
        /* Existing selector uses earliest start among all non-ended jobs.
         * If currently in a job, the next future job belongs in THEN. */
        (void)lp_agenda_next(s, now_utc, &hero);
        for (size_t n = 0; n < s->count; ++n) {
            size_t i = order[n];
            const lp_agenda_item_t *item = &s->items[i];
            if (i != hero && item->status == LP_AGENDA_SCHEDULED &&
                item->starts_at_utc > now_utc &&
                item->ends_at_utc > now_utc) {
                then = i;
                break;
            }
        }
    }

    for (size_t n = 0; n < s->count; ++n) {
        size_t i = order[n];
        const lp_agenda_item_t *item = &s->items[i];
        lp_agenda_vm_row_t *row = &out->rows[n];
        if (item->status == LP_AGENDA_UNSCHEDULED) {
            (void)strcpy(row->time, "TIME UNSCHEDULED");
        } else if (!local_stamp(item->starts_at_utc, row->time)) {
            invalidate(out);
            return true;
        }
        (void)strcpy(row->label, item->label);
        (void)strcpy(row->location, item->location_label);

        const char *tag;
        if (item->status == LP_AGENDA_CANCELLED) tag = "CANCELLED";
        else if (item->status == LP_AGENDA_UNSCHEDULED) tag = "UNSCHEDULED";
        else if (!active) tag = "LAST KNOWN";
        else if (item->ends_at_utc <= now_utc) tag = "DONE";
        else if (item->starts_at_utc <= now_utc) tag = "NOW";
        else if (i == hero) tag = "NEXT";
        else if (i == then) tag = "THEN";
        else tag = "LATER";
        (void)strcpy(row->tag, tag);
        ++out->row_count;
    }

    if (hero < s->count) {
        const lp_agenda_item_t *item = &s->items[hero];
        out->has_hero = true;
        if (!local_stamp(item->starts_at_utc, out->hero_time)) {
            invalidate(out);
            return true;
        }
        (void)strcpy(out->hero_label, item->label);
        (void)strcpy(out->hero_location, item->location_label);
        if (item->starts_at_utc <= now_utc) {
            char end_clock[24];
            if (!local_clock(item->ends_at_utc, end_clock)) {
                invalidate(out);
                return true;
            }
            (void)snprintf(out->hero_hint, sizeof(out->hero_hint),
                           "NOW UNTIL %s", end_clock);
        } else {
            const int64_t delta = item->starts_at_utc - now_utc;
            int64_t minutes = delta / 60 + (delta % 60 != 0);
            if (minutes <= 1) (void)strcpy(out->hero_hint, "STARTS SOON");
            else if (minutes <= 9999)
                (void)snprintf(out->hero_hint, sizeof(out->hero_hint),
                               "IN %" PRId64 " MIN", minutes);
            else (void)strcpy(out->hero_hint, "UPCOMING");
        }
    }
    if (then < s->count) {
        const lp_agenda_item_t *item = &s->items[then];
        out->has_then = true;
        if (!local_stamp(item->starts_at_utc, out->then_time)) {
            invalidate(out);
            return true;
        }
        (void)strcpy(out->then_label, item->label);
    }
    return true;
}
