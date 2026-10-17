#include "agenda_demo.h"
#include "agenda_store.h"
#include "clock_util.h"

#include <assert.h>
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
    test_clock_gate_and_bounds();
    test_stable_local_day_and_late_night();
    test_eastern_dst_days_and_repeated_hour();
    test_store_integration_and_rollover();
    puts("agenda demo provider: all native cases passed");
    return 0;
}
