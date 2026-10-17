#include "agenda_model.h"

#include <string.h>

/* A small strict UTF-8 check prevents arbitrary provider bytes or embedded
 * control sequences reaching LVGL. Fixed buffers are never scanned past cap. */
static bool valid_display_string(const char *s, size_t cap, bool may_be_empty)
{
    size_t i = 0;
    if (!s || !cap) return false;
    while (i < cap) {
        unsigned char b = (unsigned char)s[i];
        if (b == 0) return may_be_empty || i > 0;
        if (b < 0x20 || b == 0x7f) return false;
        if (b < 0x80) { ++i; continue; }

        size_t width;
        if (b >= 0xc2 && b <= 0xdf) width = 2;
        else if (b >= 0xe0 && b <= 0xef) width = 3;
        else if (b >= 0xf0 && b <= 0xf4) width = 4;
        else return false;
        /* Reserve at least one terminating NUL after the code point. */
        if (width >= cap - i) return false;
        for (size_t j = 1; j < width; ++j) {
            unsigned char next = (unsigned char)s[i + j];
            if ((next & 0xc0) != 0x80) return false;
        }
        unsigned char second = (unsigned char)s[i + 1];
        if ((b == 0xe0 && second < 0xa0) || /* overlong */
            (b == 0xed && second > 0x9f) || /* surrogate */
            (b == 0xf0 && second < 0x90) || /* overlong */
            (b == 0xf4 && second > 0x8f) || /* > U+10FFFF */
            (b == 0xc2 && second <= 0x9f)) /* C1 controls */
            return false;
        i += width;
    }
    return false; /* not NUL terminated */
}

static bool valid_id(const char *id, size_t cap)
{
    size_t i = 0;
    for (; i < cap && id[i]; ++i) {
        char c = id[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              c == '_' || c == '-' || c == '.'))
            return false;
    }
    return i > 0 && i < cap;
}

bool lp_agenda_validate(const lp_agenda_snapshot_t *snapshot)
{
    if (!snapshot || snapshot->schema_version != LP_AGENDA_SCHEMA_VERSION ||
        (snapshot->source != LP_AGENDA_SOURCE_DEMO &&
         snapshot->source != LP_AGENDA_SOURCE_BRIDGE) ||
        snapshot->count > LP_AGENDA_MAX_ITEMS ||
        snapshot->generated_at_utc <= 0 ||
        snapshot->source_updated_at_utc <= 0 ||
        snapshot->source_updated_at_utc > snapshot->generated_at_utc ||
        snapshot->ttl_seconds < LP_AGENDA_TTL_MIN_S ||
        snapshot->ttl_seconds > LP_AGENDA_TTL_MAX_S)
        return false;

    for (size_t i = 0; i < snapshot->count; ++i) {
        const lp_agenda_item_t *a = &snapshot->items[i];
        if (!valid_id(a->id, sizeof(a->id)) ||
            !valid_display_string(a->label, sizeof(a->label), false) ||
            !valid_display_string(a->location_label,
                                  sizeof(a->location_label), true))
            return false;
        if (a->status == LP_AGENDA_UNSCHEDULED) {
            if (a->starts_at_utc != 0 || a->ends_at_utc != 0) return false;
        } else if (a->status == LP_AGENDA_SCHEDULED ||
                   a->status == LP_AGENDA_CANCELLED) {
            if (a->starts_at_utc <= 0 ||
                a->ends_at_utc <= a->starts_at_utc ||
                a->ends_at_utc - a->starts_at_utc > 7LL * 86400)
                return false;
        } else {
            return false;
        }
        for (size_t j = 0; j < i; ++j) {
            if (strcmp(a->id, snapshot->items[j].id) == 0) return false;
        }
    }
    return true;
}

lp_agenda_view_t lp_agenda_view(const lp_agenda_snapshot_t *snapshot,
                                int64_t now_utc, bool clock_valid)
{
    if (!lp_agenda_validate(snapshot)) return LP_AGENDA_VIEW_INVALID;
    if (!clock_valid || now_utc <= 0) return LP_AGENDA_VIEW_CLOCK_UNKNOWN;
    if (snapshot->source == LP_AGENDA_SOURCE_DEMO) return LP_AGENDA_VIEW_DEMO;

    /* A future-dated response is not a trusted freshness signal. Allow
     * five minutes of clock/transport skew but never time-travel CURRENT. */
    if (snapshot->generated_at_utc > now_utc + 300 ||
        snapshot->source_updated_at_utc > now_utc + 300)
        return LP_AGENDA_VIEW_INVALID;

    if (now_utc - snapshot->generated_at_utc > snapshot->ttl_seconds ||
        now_utc - snapshot->source_updated_at_utc > snapshot->ttl_seconds)
        return LP_AGENDA_VIEW_STALE;
    return snapshot->count ? LP_AGENDA_VIEW_CURRENT
                           : LP_AGENDA_VIEW_NO_JOBS;
}

bool lp_agenda_next(const lp_agenda_snapshot_t *snapshot,
                    int64_t now_utc, size_t *out_index)
{
    if (!out_index || now_utc <= 0 || !lp_agenda_validate(snapshot))
        return false;

    bool found = false;
    size_t best = 0;
    for (size_t i = 0; i < snapshot->count; ++i) {
        const lp_agenda_item_t *a = &snapshot->items[i];
        if (a->status != LP_AGENDA_SCHEDULED ||
            a->ends_at_utc <= now_utc) continue;
        if (!found || a->starts_at_utc < snapshot->items[best].starts_at_utc ||
            (a->starts_at_utc == snapshot->items[best].starts_at_utc &&
             strcmp(a->id, snapshot->items[best].id) < 0)) {
            best = i;
            found = true;
        }
    }
    if (found) *out_index = best;
    return found;
}
