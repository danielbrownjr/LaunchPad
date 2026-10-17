#include "agenda_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FIXED_NOW 1800000000LL

static lp_agenda_snapshot_t fixture(void)
{
    lp_agenda_snapshot_t data = {
        .schema_version = LP_AGENDA_SCHEMA_VERSION,
        .source = LP_AGENDA_SOURCE_DEMO,
        .generated_at_utc = FIXED_NOW,
        .source_updated_at_utc = FIXED_NOW - 60,
        .ttl_seconds = 3600,
        .count = 1,
    };
    lp_agenda_item_t *first = &data.items[0];
    strcpy(first->id, "example-a");
    strcpy(first->label, "Example instrument visit");
    strcpy(first->location_label, "Sampletown, MA");
    first->starts_at_utc = FIXED_NOW + 300;
    first->ends_at_utc = FIXED_NOW + 3600;
    first->status = LP_AGENDA_SCHEDULED;
    return data;
}

static void test_one_and_empty(void)
{
    lp_agenda_snapshot_t data = fixture();
    size_t index = 99;
    assert(lp_agenda_validate(&data));
    assert(lp_agenda_next(&data, FIXED_NOW, &index) && index == 0);
    assert(lp_agenda_view(&data, FIXED_NOW, true) == LP_AGENDA_VIEW_DEMO);
    assert(lp_agenda_view(&data, FIXED_NOW, false) == LP_AGENDA_VIEW_CLOCK_UNKNOWN);
    assert(!lp_agenda_next(&data, FIXED_NOW + 3600, &index)); /* exclusive end */

    data.count = 0;
    index = 77;
    assert(lp_agenda_validate(&data));
    assert(!lp_agenda_next(&data, FIXED_NOW, &index) && index == 77);
    data.source = LP_AGENDA_SOURCE_BRIDGE;
    assert(lp_agenda_view(&data, FIXED_NOW, true) == LP_AGENDA_VIEW_NO_JOBS);
}

static void test_selection_order_and_status(void)
{
    lp_agenda_snapshot_t data = fixture();
    data.count = 5;

    /* Out of order on purpose. Cancelled and unscheduled are ignored. */
    strcpy(data.items[0].id, "late");
    data.items[0].starts_at_utc = FIXED_NOW + 600;
    data.items[0].ends_at_utc = FIXED_NOW + 1200;

    data.items[1] = data.items[0];
    strcpy(data.items[1].id, "cancelled");
    data.items[1].status = LP_AGENDA_CANCELLED;
    data.items[1].starts_at_utc = FIXED_NOW + 40;
    data.items[1].ends_at_utc = FIXED_NOW + 50;

    data.items[2] = data.items[0];
    strcpy(data.items[2].id, "tie-b");
    data.items[2].starts_at_utc = FIXED_NOW + 100;
    data.items[2].ends_at_utc = FIXED_NOW + 250;

    data.items[3] = data.items[2];
    strcpy(data.items[3].id, "tie-a");

    data.items[4] = data.items[2];
    strcpy(data.items[4].id, "unscheduled");
    data.items[4].status = LP_AGENDA_UNSCHEDULED;
    data.items[4].starts_at_utc = 0;
    data.items[4].ends_at_utc = 0;

    size_t index = 99;
    assert(lp_agenda_validate(&data));
    assert(lp_agenda_next(&data, FIXED_NOW, &index) && index == 3);
    /* An ongoing appointment remains relevant until its exclusive end. */
    assert(lp_agenda_next(&data, FIXED_NOW + 150, &index) && index == 3);
    assert(lp_agenda_next(&data, FIXED_NOW + 250, &index) && index == 0);
    assert(!lp_agenda_next(&data, FIXED_NOW + 1200, &index));

    data.source = LP_AGENDA_SOURCE_BRIDGE;
    assert(lp_agenda_view(&data, FIXED_NOW + 1200, true) ==
           LP_AGENDA_VIEW_NO_JOBS);
}

static void test_midnight_and_expiry(void)
{
    lp_agenda_snapshot_t data = fixture();
    /* UTC selection works without assuming 24 h wall days. The timezone
     * conversion/DST behavior is tested separately in clock_util. */
    const int64_t next_utc_midnight =
        FIXED_NOW - FIXED_NOW % 86400 + 86400;
    data.items[0].starts_at_utc = next_utc_midnight - 60;
    data.items[0].ends_at_utc = next_utc_midnight + 240;
    size_t index = 99;
    assert(lp_agenda_next(&data, next_utc_midnight + 60, &index) && index == 0);
    assert(!lp_agenda_next(&data, next_utc_midnight + 240, &index));

    data.source = LP_AGENDA_SOURCE_BRIDGE;
    data.items[0].starts_at_utc = FIXED_NOW + 100;
    data.items[0].ends_at_utc = FIXED_NOW + 10000;
    assert(lp_agenda_view(&data, FIXED_NOW + 100, true) ==
           LP_AGENDA_VIEW_CURRENT);
    assert(lp_agenda_view(&data, FIXED_NOW + 3600, true) ==
           LP_AGENDA_VIEW_STALE);
    assert(lp_agenda_view(&data, FIXED_NOW - 400, true) ==
           LP_AGENDA_VIEW_INVALID);
    assert(lp_agenda_view(&data, FIXED_NOW + 100, false) ==
           LP_AGENDA_VIEW_CLOCK_UNKNOWN);
}

static void test_invalid_input(void)
{
    lp_agenda_snapshot_t data = fixture();
    assert(!lp_agenda_validate(NULL));
    assert(!lp_agenda_next(&data, FIXED_NOW, NULL));

    data.schema_version = 2;
    assert(!lp_agenda_validate(&data));
    data = fixture();

    data.count = LP_AGENDA_MAX_ITEMS + 1;
    assert(!lp_agenda_validate(&data));
    data = fixture();

    memset(data.items[0].label, 'x', sizeof(data.items[0].label));
    assert(!lp_agenda_validate(&data)); /* missing NUL */
    data = fixture();

    strcpy(data.items[0].label, "Line\nbreak");
    assert(!lp_agenda_validate(&data));
    data = fixture();

    /* C0/C1 control, overlong and malformed UTF-8 are rejected. */
    strcpy(data.items[0].label, "\xc0\xaf");
    assert(!lp_agenda_validate(&data));
    data = fixture();

    strcpy(data.items[0].label, "Caf\xc3\xa9 visit");
    assert(lp_agenda_validate(&data));
    data = fixture();

    data.items[0].ends_at_utc = data.items[0].starts_at_utc;
    assert(!lp_agenda_validate(&data));
    data = fixture();

    data.items[0].status = LP_AGENDA_UNSCHEDULED;
    assert(!lp_agenda_validate(&data)); /* dates must be zero */
    data = fixture();

    data.count = 2;
    data.items[1] = data.items[0];
    assert(!lp_agenda_validate(&data)); /* duplicate opaque ID */
    data = fixture();

    data.source_updated_at_utc = data.generated_at_utc + 1;
    assert(!lp_agenda_validate(&data));
    data = fixture();

    data.ttl_seconds = 0;
    assert(!lp_agenda_validate(&data));
    data = fixture();

    data.items[0].ends_at_utc = data.items[0].starts_at_utc + 8LL * 86400;
    assert(!lp_agenda_validate(&data));
}

int main(void)
{
    test_one_and_empty();
    test_selection_order_and_status();
    test_midnight_and_expiry();
    test_invalid_input();
    puts("agenda model: all native cases passed");
    return 0;
}
