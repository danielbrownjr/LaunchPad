#include "agenda_demo.h"
#include "agenda_store.h"
#include "clock_util.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef NDEBUG
#error "Tests must keep assertions enabled"
#endif

static int64_t eastern(int year, int month, int day, int hour, int minute,
                       int isdst)
{
    struct tm local = {
        .tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day,
        .tm_hour = hour, .tm_min = minute, .tm_sec = 0, .tm_isdst = isdst,
    };
    time_t result = mktime(&local);
    assert(result != (time_t)-1);
    return (int64_t)result;
}

static void assert_local_slot(int64_t epoch, int year, int month, int day,
                              int hour, int minute)
{
    const time_t t = (time_t)epoch;
    struct tm local;
    assert(localtime_r(&t, &local) != NULL);
    assert(local.tm_year + 1900 == year);
    assert(local.tm_mon + 1 == month);
    assert(local.tm_mday == day);
    assert(local.tm_hour == hour);
    assert(local.tm_min == minute);
}

static void test_clock_gate_and_bounds(void)
{
    lp_agenda_snapshot_t out;
    memset(&out, 0xa5, sizeof(out));
    lp_agenda_snapshot_t original = out;
    int64_t now = eastern(2026, 10, 17, 10, 0, -1);
    assert(!lp_agenda_demo_build(now, false, &out));
    assert(memcmp(&out, &original, sizeof(out)) == 0);
    assert(!lp_agenda_demo_build(0, true, &out));
    assert(memcmp(&out, &original, sizeof(out)) == 0);
    assert(!lp_agenda_demo_build(now, true, NULL));
    assert(lp_agenda_demo_build(now, true, &out));
    assert(lp_agenda_validate(&out));
    assert(out.source == LP_AGENDA_SOURCE_DEMO);
    assert(out.count == 4);
    assert(out.ttl_seconds == LP_AGENDA_TTL_MAX_S);
    assert(out.source_checked_at_utc == now);
    assert(out.generated_at_utc == now);
    assert(lp_agenda_view(&out, now, true) == LP_AGENDA_VIEW_DEMO);
    assert(lp_agenda_view(&out, now, false) == LP_AGENDA_VIEW_CLOCK_UNKNOWN);
    for (size_t i = 0; i < out.count; ++i) {
        assert(out.items[i].status == LP_AGENDA_SCHEDULED);
        assert(strncmp(out.items[i].id, "demo-", 5) == 0);
        assert(strstr(out.items[i].label, "Fictional") != NULL);
        assert(out.items[i].ends_at_utc > out.items[i].starts_at_utc);
    }
}

static void test_stable_local_day_and_late_night(void)
{
    lp_agenda_snapshot_t morning, late;
    int64_t first = eastern(2026, 10, 17, 6, 15, -1);
    int64_t second = eastern(2026, 10, 17, 23, 59, -1);
    assert(lp_agenda_demo_build(first, true, &morning));
    assert(lp_agenda_demo_build(second, true, &late));
    for (size_t i = 0; i < morning.count; ++i) {
        assert(morning.items[i].starts_at_utc == late.items[i].starts_at_utc);
        assert(morning.items[i].ends_at_utc == late.items[i].ends_at_utc);
        assert(strcmp(morning.items[i].id, late.items[i].id) == 0);
    }
    assert_local_slot(morning.items[0].starts_at_utc, 2026, 10, 17, 9, 30);
    assert_local_slot(morning.items[1].starts_at_utc, 2026, 10, 17, 12, 30);
    assert_local_slot(morning.items[2].starts_at_utc, 2026, 10, 17, 16, 0);
    assert_local_slot(morning.items[3].starts_at_utc, 2026, 10, 18, 9, 30);

    size_t next = 99;
    assert(lp_agenda_next(&late, second, &next));
    assert(next == 3); /* Still an upcoming fictional call near midnight. */

    lp_agenda_snapshot_t next_day;
    assert(lp_agenda_demo_build(eastern(2026, 10, 18, 0, 1, -1),
                                true, &next_day));
    assert(next_day.items[0].starts_at_utc == morning.items[3].starts_at_utc);

    lp_agenda_snapshot_t new_year;
    assert(lp_agenda_demo_build(eastern(2026, 12, 31, 23, 59, -1),
                                true, &new_year));
    assert_local_slot(new_year.items[3].starts_at_utc, 2027, 1, 1, 9, 30);
}

static void test_eastern_dst_days_and_repeated_hour(void)
{
    lp_agenda_snapshot_t before_spring, before_fall, fold_one, fold_two;
    assert(lp_agenda_demo_build(eastern(2026, 3, 7, 12, 0, -1),
                                true, &before_spring));
    assert_local_slot(before_spring.items[3].starts_at_utc, 2026, 3, 8, 9, 30);
    /* Spring-forward local calendar day between the same hour has 23h. */
    assert(before_spring.items[3].starts_at_utc -
           before_spring.items[0].starts_at_utc == 23LL * 3600);

    assert(lp_agenda_demo_build(eastern(2026, 10, 31, 12, 0, -1),
                                true, &before_fall));
    assert_local_slot(before_fall.items[3].starts_at_utc, 2026, 11, 1, 9, 30);
    /* Fall-back local calendar day between the same hour has 25h. */
    assert(before_fall.items[3].starts_at_utc -
           before_fall.items[0].starts_at_utc == 25LL * 3600);

    int64_t first_130 = eastern(2026, 11, 1, 1, 30, 1);
    int64_t second_130 = eastern(2026, 11, 1, 1, 30, 0);
    assert(second_130 - first_130 == 3600);
    assert(lp_agenda_demo_build(first_130, true, &fold_one));
    assert(lp_agenda_demo_build(second_130, true, &fold_two));
    assert(fold_one.items[0].starts_at_utc == fold_two.items[0].starts_at_utc);
    assert(fold_one.items[3].starts_at_utc == fold_two.items[3].starts_at_utc);
}

/* Ground truth from the IANA America/New_York zone (Python zoneinfo), NOT
 * from this C library. Round-tripping mktime() through the same libc's
 * localtime_r() cannot detect a consistently wrong TZ implementation, and
 * the ESP32-P4 build links picolibc while host CI links glibc. */
static void assert_starts(const lp_agenda_snapshot_t *s, int64_t a, int64_t b,
                          int64_t c, int64_t d)
{
    assert(s->count == 4);
    assert(s->items[0].starts_at_utc == a);
    assert(s->items[1].starts_at_utc == b);
    assert(s->items[2].starts_at_utc == c);
    assert(s->items[3].starts_at_utc == d);
}

static void test_absolute_utc_and_calendar_edges(void)
{
    lp_agenda_snapshot_t s;
    /* Ordinary EDT day: 09:30 EDT = 13:30Z; 12:30, 16:00 same day. */
    assert(lp_agenda_demo_build(1792245600, true, &s));   /* 10-17 10:00 */
    assert_starts(&s, 1792243800, 1792243800 + 3 * 3600,
                  1792243800 + 6 * 3600 + 1800, 1792243800 + 86400);

    /* Day before spring-forward: tomorrow 09:30 EDT is only 23 h later. */
    assert(lp_agenda_demo_build(1772902800, true, &s));   /* 03-07 12:00 */
    assert(s.items[0].starts_at_utc == 1772893800);       /* 14:30Z EST */
    assert(s.items[3].starts_at_utc == 1772976600);       /* 13:30Z EDT */
    /* On the 23-hour day itself, after the 02:00 jump. */
    assert(lp_agenda_demo_build(1772955000, true, &s));   /* 03-08 03:30 */
    assert(s.items[0].starts_at_utc == 1772976600);
    assert(s.items[3].starts_at_utc == 1773063000);

    /* Day before fall-back: tomorrow is 25 h later. */
    assert(lp_agenda_demo_build(1793462400, true, &s));   /* 10-31 12:00 */
    assert(s.items[0].starts_at_utc == 1793453400);       /* 13:30Z EDT */
    assert(s.items[3].starts_at_utc == 1793543400);       /* 14:30Z EST */
    /* Both occurrences of the repeated 01:30 map to the same day. */
    lp_agenda_snapshot_t second;
    assert(lp_agenda_demo_build(1793511000, true, &s));   /* first 01:30 */
    assert(lp_agenda_demo_build(1793514600, true, &second));
    assert_starts(&s, 1793543400, 1793543400 + 3 * 3600,
                  1793543400 + 6 * 3600 + 1800, 1793629800);
    assert(memcmp(s.items, second.items, sizeof(s.items)) == 0);

    /* Leap years: 2028-02-28 -> 02-29, 02-29 -> 03-01; 2027 and the
     * non-leap century year 2100 go 02-28 -> 03-01. */
    assert(lp_agenda_demo_build(1835370000, true, &s));   /* 2028-02-28 */
    assert(s.items[0].starts_at_utc == 1835361000);
    assert(s.items[3].starts_at_utc == 1835447400);       /* 02-29 */
    assert(lp_agenda_demo_build(1835456400, true, &s));   /* 2028-02-29 */
    assert(s.items[0].starts_at_utc == 1835447400);
    assert(s.items[3].starts_at_utc == 1835533800);       /* 03-01 */
    assert(lp_agenda_demo_build(1803834000, true, &s));   /* 2027-02-28 */
    assert(s.items[3].starts_at_utc == 1803911400);       /* 03-01 */
    assert(lp_agenda_demo_build(4107517200, true, &s));   /* 2100-02-28 */
    assert(s.items[0].starts_at_utc == 4107508200);
    assert(s.items[3].starts_at_utc == 4107594600);       /* 03-01 */

    /* Year rollover, and past the signed 32-bit time_t limit. */
    assert(lp_agenda_demo_build(1798779540, true, &s));   /* 12-31 23:59 */
    assert(s.items[2].starts_at_utc == 1798750800);       /* 16:00 EST */
    assert(s.items[3].starts_at_utc == 1798813800);       /* 2027-01-01 */
    assert(lp_agenda_demo_build(2147500800, true, &s));   /* 2038-01-19 */
    assert(s.items[0].starts_at_utc == 2147524200);
    assert(s.items[3].starts_at_utc == 2147610600);
}

static void test_invalid_times_leave_output_untouched(void)
{
    lp_agenda_snapshot_t out, original;
    memset(&out, 0x5a, sizeof(out));
    original = out;
    const int64_t bad[] = {
        INT64_MIN, -1, 0, (int64_t)LP_CLOCK_MIN_EPOCH - 1, INT64_MAX,
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        assert(!lp_agenda_demo_build(bad[i], true, &out));
        assert(memcmp(&out, &original, sizeof(out)) == 0);
        assert(!lp_agenda_demo_poll(bad[i], true));
    }
}

/* Runs before the store is initialized: a failed publication must not
 * mark the day done, so the same day is published once the store works. */
static void test_publication_failure_does_not_mark_day(void)
{
    assert(!lp_agenda_demo_poll(1792252800, true));       /* 10-17 12:00 */
}

static void test_store_integration_and_rollover(void)
{
    assert(lp_agenda_store_init());
    lp_agenda_store_state_t state;
    assert(lp_agenda_store_read(&state));
    assert(!state.has_snapshot && state.provider == LP_AGENDA_PROVIDER_NO_DATA);

    const int64_t late = eastern(2026, 10, 17, 23, 59, -1);
    assert(!lp_agenda_demo_poll(late, false));
    assert(lp_agenda_store_read(&state) && !state.has_snapshot);
    assert(lp_agenda_demo_poll(late, true));
    assert(lp_agenda_store_read(&state) && state.has_snapshot);
    assert(state.provider == LP_AGENDA_PROVIDER_OK);
    assert(state.snapshot.source == LP_AGENDA_SOURCE_DEMO);
    uint64_t revision = state.revision;
    assert(!lp_agenda_demo_poll(late + 15, true));
    assert(lp_agenda_store_read(&state) && state.revision == revision);

    assert(lp_agenda_store_mark_fetch_error());
    assert(lp_agenda_store_read(&state) &&
           state.provider == LP_AGENDA_PROVIDER_FETCH_ERROR);
    revision = state.revision;
    assert(!lp_agenda_demo_poll(late + 15, true));
    assert(lp_agenda_store_read(&state) &&
           state.revision == revision &&
           state.provider == LP_AGENDA_PROVIDER_FETCH_ERROR);

    const int64_t new_day = eastern(2026, 10, 18, 0, 1, -1);
    assert(!lp_agenda_demo_poll(new_day, false));
    assert(lp_agenda_store_read(&state) && state.revision == revision);
    assert(lp_agenda_demo_poll(new_day, true));
    assert(lp_agenda_store_read(&state));
    assert(state.provider == LP_AGENDA_PROVIDER_OK);
    assert(state.revision == revision + 1);
    assert_local_slot(state.snapshot.items[0].starts_at_utc,
                      2026, 10, 18, 9, 30);

    /* Same-day polling, including a backward correction that stays on the
     * same local day, never republishes. */
    revision = state.revision;
    for (int i = 0; i < 60; ++i) assert(!lp_agenda_demo_poll(1792350000, true));
    assert(!lp_agenda_demo_poll(1792296030, true));     /* 10-18 00:00:30 */
    assert(lp_agenda_store_read(&state) && state.revision == revision);

    /* SNTP correction backwards across midnight follows the local date. */
    assert(lp_agenda_demo_poll(1792295880, true));      /* 10-17 23:58 */
    assert(lp_agenda_store_read(&state) && state.revision == revision + 1);
    assert(state.snapshot.items[0].starts_at_utc == 1792243800);
    /* And a forward jump of several days publishes that day's schedule. */
    assert(lp_agenda_demo_poll(1792936800, true));      /* 10-25 10:00 */
    assert(lp_agenda_store_read(&state) && state.revision == revision + 2);
    assert(state.snapshot.items[0].starts_at_utc == 1792935000);
    assert(!lp_agenda_demo_poll(1792936800 + 1, true));
    assert(lp_agenda_store_read(&state) && state.revision == revision + 2);
    /* Same tm_yday one year later is a different local day. */
    assert(lp_agenda_demo_poll(1824472800, true));      /* 2027-10-25 */
    assert(lp_agenda_store_read(&state) && state.revision == revision + 3);
    assert(state.snapshot.items[0].starts_at_utc == 1824471000);

    /* Synthetic BRIDGE input in this test exercises the non-overwrite gate.
     * No real bridge or employer data is used. */
    lp_agenda_snapshot_t bridge = state.snapshot;
    bridge.source = LP_AGENDA_SOURCE_BRIDGE;
    assert(lp_agenda_store_publish(&bridge));
    assert(!lp_agenda_demo_poll(eastern(2026, 10, 19, 0, 1, -1), true));
    assert(lp_agenda_store_read(&state));
    assert(state.snapshot.source == LP_AGENDA_SOURCE_BRIDGE);
    assert(state.provider == LP_AGENDA_PROVIDER_OK);

    assert(lp_agenda_store_mark_fetch_error());
    assert(!lp_agenda_demo_poll(eastern(2026, 10, 20, 0, 1, -1), true));
    assert(lp_agenda_store_read(&state));
    assert(state.provider == LP_AGENDA_PROVIDER_FETCH_ERROR);
    assert(state.snapshot.source == LP_AGENDA_SOURCE_BRIDGE);
}

int main(void)
{
    lp_clock_timezone_init();
    test_publication_failure_does_not_mark_day();
    test_clock_gate_and_bounds();
    test_stable_local_day_and_late_night();
    test_eastern_dst_days_and_repeated_hour();
    test_absolute_utc_and_calendar_edges();
    test_invalid_times_leave_output_untouched();
    test_store_integration_and_rollover();
    puts("agenda demo provider: all native cases passed");
    return 0;
}
