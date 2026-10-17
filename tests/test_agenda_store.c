#include "agenda_store.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define EPOCH 1800000000LL
#define ROUNDS 600

static lp_agenda_snapshot_t sample(const char *id)
{
    lp_agenda_snapshot_t snapshot = {
        .schema_version = LP_AGENDA_SCHEMA_VERSION,
        .source = LP_AGENDA_SOURCE_DEMO,
        .generated_at_utc = EPOCH,
        .source_checked_at_utc = EPOCH - 60,
        .ttl_seconds = 3600,
        .count = 1,
    };
    lp_agenda_item_t *item = &snapshot.items[0];
    strcpy(item->id, id);
    strcpy(item->label, id);
    strcpy(item->location_label, "Fictional site");
    item->status = LP_AGENDA_SCHEDULED;
    item->starts_at_utc = EPOCH + 100;
    item->ends_at_utc = EPOCH + 200;
    return snapshot;
}

static void test_pre_init_and_empty(void)
{
    lp_agenda_store_state_t out = { .revision = 777 };
    lp_agenda_snapshot_t data = sample("initial");

    assert(!lp_agenda_store_read(&out));
    assert(out.revision == 777);  /* Failed read must not clobber output. */
    assert(!lp_agenda_store_publish(&data));
    assert(!lp_agenda_store_mark_fetch_error());
    assert(lp_agenda_store_init());
    assert(lp_agenda_store_init());  /* Idempotent; does not discard state. */
    assert(lp_agenda_store_read(&out));
    assert(out.provider == LP_AGENDA_PROVIDER_NO_DATA);
    assert(!out.has_snapshot);
    assert(out.revision == 0);
    assert(!lp_agenda_store_read(NULL));
    assert(!lp_agenda_store_publish(NULL));
}

static void test_errors_and_recovery(void)
{
    lp_agenda_store_state_t out;
    lp_agenda_snapshot_t data = sample("good-1");

    assert(lp_agenda_store_mark_fetch_error());
    assert(lp_agenda_store_read(&out));
    assert(out.provider == LP_AGENDA_PROVIDER_FETCH_ERROR);
    assert(!out.has_snapshot);
    assert(out.revision == 1);
    assert(lp_agenda_store_mark_fetch_error());
    assert(lp_agenda_store_read(&out));
    assert(out.revision == 1);  /* Repeated error is not another update. */

    assert(lp_agenda_store_publish(&data));
    assert(lp_agenda_store_read(&out));
    assert(out.revision == 2);
    assert(out.provider == LP_AGENDA_PROVIDER_OK && out.has_snapshot);
    assert(strcmp(out.snapshot.items[0].id, "good-1") == 0);

    /* Copy ownership: mutating caller storage after publish has no effect. */
    strcpy(data.items[0].id, "changed");
    assert(lp_agenda_store_read(&out));
    assert(strcmp(out.snapshot.items[0].id, "good-1") == 0);

    /* Invalid publication is a no-op, including when already in error. */
    data.schema_version = 999;
    assert(!lp_agenda_store_publish(&data));
    assert(lp_agenda_store_read(&out));
    assert(out.provider == LP_AGENDA_PROVIDER_OK && out.revision == 2);
    assert(strcmp(out.snapshot.items[0].id, "good-1") == 0);

    assert(lp_agenda_store_mark_fetch_error());
    assert(lp_agenda_store_read(&out));
    assert(out.provider == LP_AGENDA_PROVIDER_FETCH_ERROR);
    assert(out.revision == 3 && out.has_snapshot);
    assert(strcmp(out.snapshot.items[0].id, "good-1") == 0);
    assert(out.snapshot.source_checked_at_utc == EPOCH - 60);

    data = sample("recovered");
    assert(lp_agenda_store_publish(&data));
    assert(lp_agenda_store_read(&out));
    assert(out.provider == LP_AGENDA_PROVIDER_OK);
    assert(out.revision == 4);
    assert(strcmp(out.snapshot.items[0].id, "recovered") == 0);

    assert(lp_agenda_store_publish(&data)); /* An accepted refresh increments. */
    assert(lp_agenda_store_read(&out));
    assert(out.revision == 5);

    /* Strict bounds/field validation still applies before publication. */
    data = sample("bad");
    memset(data.items[0].label, 'x', sizeof(data.items[0].label));
    assert(!lp_agenda_store_publish(&data));
    data = sample("bad");
    data.count = LP_AGENDA_MAX_ITEMS + 1;
    assert(!lp_agenda_store_publish(&data));
    assert(lp_agenda_store_read(&out));
    assert(out.revision == 5);
}

typedef struct {
    unsigned writer;
} writer_arg_t;

static atomic_uint s_done;

static void *writer_thread(void *arg)
{
    const writer_arg_t *w = arg;
    for (unsigned i = 0; i < ROUNDS; ++i) {
        char id[LP_AGENDA_ID_LEN + 1];
        (void)snprintf(id, sizeof(id), "w%u-%04u", w->writer, i);
        lp_agenda_snapshot_t data = sample(id);
        assert(lp_agenda_store_publish(&data));
    }
    atomic_fetch_add(&s_done, 1U);
    return NULL;
}

static void *reader_thread(void *unused)
{
    (void)unused;
    uint64_t previous = 0;
    unsigned reads = 0;
    while (atomic_load(&s_done) < 2U || reads < 100U) {
        lp_agenda_store_state_t state;
        assert(lp_agenda_store_read(&state));
        assert(state.revision >= previous);
        previous = state.revision;
        assert(state.has_snapshot);
        assert(state.provider == LP_AGENDA_PROVIDER_OK);
        assert(state.snapshot.count == 1);
        const lp_agenda_item_t *item = &state.snapshot.items[0];
        assert(strcmp(item->id, item->label) == 0);
        assert(item->starts_at_utc == EPOCH + 100);
        assert(item->ends_at_utc == EPOCH + 200);
        assert(lp_agenda_validate(&state.snapshot));
        ++reads;
    }
    return NULL;
}

static void test_concurrent_atomic_copies(void)
{
    lp_agenda_snapshot_t data = sample("baseline");
    assert(lp_agenda_store_publish(&data));

    lp_agenda_store_state_t before;
    assert(lp_agenda_store_read(&before));
    atomic_init(&s_done, 0U);
    pthread_t a, b, reader;
    writer_arg_t first = { .writer = 0 }, second = { .writer = 1 };
    assert(pthread_create(&a, NULL, writer_thread, &first) == 0);
    assert(pthread_create(&b, NULL, writer_thread, &second) == 0);
    assert(pthread_create(&reader, NULL, reader_thread, NULL) == 0);
    assert(pthread_join(a, NULL) == 0);
    assert(pthread_join(b, NULL) == 0);
    assert(pthread_join(reader, NULL) == 0);

    lp_agenda_store_state_t after;
    assert(lp_agenda_store_read(&after));
    assert(after.revision == before.revision + 2U * ROUNDS);
    assert(after.provider == LP_AGENDA_PROVIDER_OK);
}

int main(void)
{
    test_pre_init_and_empty();
    test_errors_and_recovery();
    test_concurrent_atomic_copies();
    puts("agenda store: all native cases passed");
    return 0;
}
