#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "agenda_model.h"

/* The store holds one bounded copy of the most recently accepted agenda.
 * Provider state and schedule freshness are different concepts:
 * FETCH_ERROR preserves the last snapshot, but it does NOT make it CURRENT.
 * Never call this module from an ISR. No LVGL or network dependencies. */
typedef enum {
    LP_AGENDA_PROVIDER_NO_DATA = 0,
    LP_AGENDA_PROVIDER_OK,
    LP_AGENDA_PROVIDER_FETCH_ERROR,
} lp_agenda_provider_state_t;

typedef struct {
    uint64_t revision; /* Monotonic until saturation; 0 on initialization. */
    lp_agenda_provider_state_t provider;
    bool has_snapshot;
    lp_agenda_snapshot_t snapshot; /* Meaningful only if has_snapshot is true. */
} lp_agenda_store_state_t;

/* Call once on the app's startup task, before other tasks use this store.
 * Idempotent on subsequent calls, but concurrent initializers are forbidden.
 * Returns false if FreeRTOS cannot allocate the mutex. */
bool lp_agenda_store_init(void);

/* Publish one validated, *copied* snapshot atomically. Caller must not
 * mutate input concurrently with this operation. Invalid input is rejected
 * without changing status, last-good data or revision. May block on mutex. */
bool lp_agenda_store_publish(const lp_agenda_snapshot_t *snapshot);

/* A failed provider fetch keeps the last-good snapshot for explicitly stale/
 * error display. Repeated errors do not artificially bump the revision. */
bool lp_agenda_store_mark_fetch_error(void);

/* An atomic full snapshot+provider+revision copy. Caller owns the result.
 * Returns false and leaves *out unchanged if not initialized or unavailable.
 * Copy cost is bounded (8 entries), no heap allocation per operation. */
bool lp_agenda_store_read(lp_agenda_store_state_t *out);
