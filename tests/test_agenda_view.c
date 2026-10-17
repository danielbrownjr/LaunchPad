#include "agenda_view.h"
#include "agenda_demo.h"
#include "clock_util.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef NDEBUG
#error "Agenda view tests require assertions"
#endif

static int64_t eastern(int year, int month, int day, int hour, int minute, int dst)
{
    struct tm local = {
        .tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day,
        .tm_hour = hour, .tm_min = minute, .tm_sec = 0, .tm_isdst = dst,
    };
    time_t t = mktime(&local);
    assert(t != (time_t)-1);
    return (int64_t)t;
}

static lp_agenda_store_state_t demo_state(int64_t now)
{
    lp_agenda_store_state_t store = {
        .revision = 44, .provider = LP_AGENDA_PROVIDER_OK,
        .has_snapshot = true,
    };
    assert(lp_agenda_demo_build(now, true, &store.snapshot));
    return store;
}

static lp_agenda_store_state_t bridge_state(int64_t now)
{
    lp_agenda_store_state_t store = demo_state(now);
    store.snapshot.source = LP_AGENDA_SOURCE_BRIDGE;
    store.snapshot.generated_at_utc = now;
    store.snapshot.source_checked_at_utc = now - 60;
    store.snapshot.ttl_seconds = 3600;
    return store;
}

static void assert_banner(lp_agenda_vm_t *vm, lp_agenda_screen_state_t state,
                          const char *text)
{
    assert(vm->state == state);
    assert(strcmp(vm->banner, text) == 0);
}

static void test_missing_bad_and_error(void)
{
    const int64_t now = eastern(2026, 10, 17, 10, 0, -1);
    lp_agenda_store_state_t store = { .revision = 101 };
    lp_agenda_vm_t vm;
    assert(!lp_agenda_vm_build(NULL, now, true, false, &vm));
    assert(!lp_agenda_vm_build(&store, now, true, false, NULL));

    assert(lp_agenda_vm_build(&store, now, false, false, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_NO_DATA, "NO SCHEDULE DATA");
    assert(!vm.has_hero && !vm.has_then && vm.row_count == 0);
    assert(!vm.network_online && vm.revision == 101);
    store.provider = LP_AGENDA_PROVIDER_FETCH_ERROR;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_ERROR, "SCHEDULE SOURCE ERROR");
    assert(vm.row_count == 0 && vm.network_online);
    store.provider = LP_AGENDA_PROVIDER_OK;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_INVALID, "INVALID SCHEDULE DATA");

    store = demo_state(now);
    store.snapshot.schema_version = 17;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_INVALID, "INVALID SCHEDULE DATA");
    assert(!vm.has_hero && vm.row_count == 0 && !vm.has_then);

    store = demo_state(now);
    store.provider = (lp_agenda_provider_state_t)-1;
    assert(lp_agenda_vm_build(&store, now, true, false, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_INVALID, "INVALID SCHEDULE DATA");

    store = demo_state(now);
    store.provider = LP_AGENDA_PROVIDER_NO_DATA;
    assert(lp_agenda_vm_build(&store, now, true, false, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_INVALID, "INVALID SCHEDULE DATA");
}

static void test_demo_active_and_then(void)
{
    const int64_t now = eastern(2026, 10, 17, 10, 0, -1);
    lp_agenda_store_state_t store = demo_state(now);
    lp_agenda_vm_t vm;
    assert(lp_agenda_vm_build(&store, now, true, false, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_DEMO, "FICTIONAL DEMO SCHEDULE");
    assert(!vm.dim_schedule && vm.has_hero && vm.has_then);
    assert(!vm.network_online && vm.revision == store.revision);
    assert(strcmp(vm.source, "FICTIONAL DEMO") == 0);
    assert(strcmp(vm.freshness, "FICTIONAL - NO LIVE SOURCE") == 0);
    assert(strcmp(vm.hero_label, "Fictional instrument check") == 0);
    assert(strcmp(vm.hero_time, "Oct 17 09:30 AM EDT") == 0);
    assert(strcmp(vm.hero_hint, "NOW UNTIL 10:45 AM EDT") == 0);
    assert(strcmp(vm.then_label, "Fictional calibration visit") == 0);
    assert(strcmp(vm.then_time, "Oct 17 12:30 PM EDT") == 0);
    assert(vm.row_count == 4);
    assert(strcmp(vm.rows[0].tag, "NOW") == 0);
    assert(strcmp(vm.rows[1].tag, "THEN") == 0);
    assert(strcmp(vm.rows[2].tag, "LATER") == 0);
    assert(strcmp(vm.rows[3].tag, "LATER") == 0);
    for (size_t i = 0; i < vm.row_count; ++i) {
        assert(strstr(vm.rows[i].label, "Fictional") != NULL);
    }

    /* A future job is NEXT and the second future job is THEN. */
    const int64_t early = eastern(2026, 10, 17, 8, 0, -1);
    assert(lp_agenda_vm_build(&store, early, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_DEMO, "FICTIONAL DEMO SCHEDULE");
    assert(strcmp(vm.hero_hint, "IN 90 MIN") == 0);
    assert(strcmp(vm.rows[0].tag, "NEXT") == 0);
    assert(strcmp(vm.rows[1].tag, "THEN") == 0);
    assert(vm.network_online);
    assert(strcmp(vm.source, "FICTIONAL DEMO") == 0);

    /* The final slot for tomorrow remains UPCOMING near local midnight. */
    const int64_t late = eastern(2026, 10, 17, 23, 59, -1);
    assert(lp_agenda_vm_build(&store, late, true, false, &vm));
    assert(vm.has_hero && !vm.has_then);
    assert(strcmp(vm.hero_label, "Fictional morning visit") == 0);
    assert(strcmp(vm.rows[0].tag, "DONE") == 0);
    assert(strcmp(vm.rows[3].tag, "NEXT") == 0);
}

static void test_bridge_stale_error_clock_and_no_jobs(void)
{
    const int64_t now = eastern(2026, 10, 17, 10, 0, -1);
    lp_agenda_store_state_t store = bridge_state(now);
    lp_agenda_vm_t vm;
    assert(lp_agenda_vm_build(&store, now, true, false, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_CURRENT, "SCHEDULE CURRENT");
    assert(vm.has_hero && !vm.dim_schedule);
    assert(strcmp(vm.source, "BRIDGE DATA") == 0);
    assert(strcmp(vm.freshness, "CHECKED 1 MIN AGO") == 0);
    assert(!vm.network_online); /* Offline does not mean data is stale. */

    store.provider = LP_AGENDA_PROVIDER_FETCH_ERROR;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_ERROR, "SCHEDULE SOURCE ERROR");
    assert(vm.dim_schedule && !vm.has_hero && !vm.has_then);
    assert(vm.row_count == store.snapshot.count);
    assert(strcmp(vm.rows[0].tag, "LAST KNOWN") == 0);
    assert(strcmp(vm.freshness, "CHECKED 1 MIN AGO") == 0);

    store.provider = LP_AGENDA_PROVIDER_OK;
    store.snapshot.source_checked_at_utc = now - 7200;
    store.snapshot.generated_at_utc = now - 3600;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_STALE, "SCHEDULE STALE");
    assert(vm.dim_schedule && !vm.has_hero && !vm.has_then);
    assert(strcmp(vm.rows[0].tag, "LAST KNOWN") == 0);
    assert(strcmp(vm.freshness, "CHECKED 2 H AGO") == 0);

    store = bridge_state(now);
    assert(lp_agenda_vm_build(&store, now, false, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_CLOCK_UNSYNCED, "CLOCK NOT SET");
    assert(vm.row_count == 4 && vm.dim_schedule);
    assert(!vm.has_hero && !vm.has_then);
    assert(strcmp(vm.freshness, "LAST CHECK AGE UNKNOWN") == 0);
    assert(strcmp(vm.rows[0].time, "Oct 17 09:30 AM EDT") == 0);

    store = bridge_state(now);
    store.snapshot.count = 0;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_NO_JOBS, "NO UPCOMING JOBS");
    assert(!vm.has_hero && vm.row_count == 0);

    store = bridge_state(now);
    store.snapshot.count = 1;
    store.snapshot.items[0].ends_at_utc = now - 10;
    store.snapshot.items[0].starts_at_utc = now - 3600;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_NO_JOBS, "NO UPCOMING JOBS");
    assert(!vm.has_hero && vm.row_count == 1);
}

static void test_dst_fallback_demo_vs_bridge_ttl(void)
{
    /* 2026 fall-back: 09:00 EDT yesterday to 10:00 EST today is 26h.
     * DEMO must remain explicitly DEMO rather than incorrectly STALE. */
    const int64_t old = eastern(2026, 10, 31, 9, 0, -1);
    const int64_t now = eastern(2026, 11, 1, 10, 0, -1);
    assert(now - old == 26LL * 3600);
    lp_agenda_store_state_t store = demo_state(old);
    lp_agenda_vm_t vm;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_DEMO, "FICTIONAL DEMO SCHEDULE");
    assert(!vm.dim_schedule);
    assert(vm.has_hero); /* Yesterday's slot for today. */
    assert(strcmp(vm.hero_time, "Nov 01 09:30 AM EST") == 0);
    /* At 10 AM, yesterday's last slot is ongoing until 10:45 AM EST. */
    assert(strcmp(vm.hero_hint, "NOW UNTIL 10:45 AM EST") == 0);

    store.snapshot.source = LP_AGENDA_SOURCE_BRIDGE;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_STALE, "SCHEDULE STALE");
    assert(vm.dim_schedule && !vm.has_hero);
}

static void test_unscheduled_cancelled_unicode_sort_and_ties(void)
{
    const int64_t now = eastern(2026, 10, 17, 10, 0, -1);
    lp_agenda_store_state_t store = bridge_state(now);
    store.snapshot.count = 4;

    store.snapshot.items[0].status = LP_AGENDA_CANCELLED;
    strcpy(store.snapshot.items[0].id, "cancelled");
    strcpy(store.snapshot.items[0].label, "Cancelled visit");

    store.snapshot.items[1] = store.snapshot.items[0];
    store.snapshot.items[1].status = LP_AGENDA_SCHEDULED;
    strcpy(store.snapshot.items[1].id, "b");
    strcpy(store.snapshot.items[1].label, "Caf\xc3\xa9 visit");
    store.snapshot.items[1].starts_at_utc = now + 1800;
    store.snapshot.items[1].ends_at_utc = now + 3600;

    store.snapshot.items[2] = store.snapshot.items[1];
    strcpy(store.snapshot.items[2].id, "a");

    store.snapshot.items[3] = store.snapshot.items[1];
    store.snapshot.items[3].status = LP_AGENDA_UNSCHEDULED;
    store.snapshot.items[3].starts_at_utc = 0;
    store.snapshot.items[3].ends_at_utc = 0;
    strcpy(store.snapshot.items[3].id, "unscheduled");

    lp_agenda_vm_t vm;
    assert(lp_agenda_vm_build(&store, now, true, true, &vm));
    assert_banner(&vm, LP_AGENDA_SCREEN_CURRENT, "SCHEDULE CURRENT");
    assert(vm.row_count == 4);
    assert(strcmp(vm.rows[0].tag, "CANCELLED") == 0);
    assert(strcmp(vm.rows[1].tag, "NEXT") == 0);
    assert(strcmp(vm.rows[1].label, "Caf\xc3\xa9 visit") == 0);
    assert(strcmp(vm.rows[2].tag, "THEN") == 0);
    assert(strcmp(vm.rows[3].tag, "UNSCHEDULED") == 0);
    assert(strcmp(vm.rows[3].time, "TIME UNSCHEDULED") == 0);
    assert(strcmp(vm.hero_label, "Caf\xc3\xa9 visit") == 0);
    assert(strcmp(vm.hero_hint, "IN 30 MIN") == 0);
}

int main(void)
{
    lp_clock_timezone_init();
    test_missing_bad_and_error();
    test_demo_active_and_then();
    test_bridge_stale_error_clock_and_no_jobs();
    test_dst_fallback_demo_vs_bridge_ttl();
    test_unscheduled_cancelled_unicode_sort_and_ties();
    puts("agenda view model: all native cases passed");
    return 0;
}
