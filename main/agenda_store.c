#include "agenda_store.h"

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* One non-ISR shared store. Keep the lock held only for a bounded struct
 * copy/status mutation. Validate outside the mutex; never run providers,
 * formatting, networking or LVGL while holding it. */
static SemaphoreHandle_t s_mutex;
static lp_agenda_store_state_t s_state;

_Static_assert(sizeof(lp_agenda_snapshot_t) <= 2048,
               "Agenda snapshots must remain small, bounded and copyable");

static void bump_revision(void)
{
    if (s_state.revision != UINT64_MAX) ++s_state.revision;
}

bool lp_agenda_store_init(void)
{
    /* Startup-only contract. Do not call while another task initializes. */
    if (s_mutex != NULL) return true;
    s_mutex = xSemaphoreCreateMutex();
    return s_mutex != NULL;
}

bool lp_agenda_store_publish(const lp_agenda_snapshot_t *snapshot)
{
    if (snapshot == NULL || s_mutex == NULL) return false;

    /* Copy once, then validate the same bytes we will publish: callers own
     * their input and may not modify it until this operation returns. */
    const lp_agenda_snapshot_t candidate = *snapshot;
    if (!lp_agenda_validate(&candidate)) return false;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return false;
    s_state.snapshot = candidate;
    s_state.has_snapshot = true;
    s_state.provider = LP_AGENDA_PROVIDER_OK;
    bump_revision(); /* Every valid publication is an observable new update. */
    (void)xSemaphoreGive(s_mutex);
    return true;
}

bool lp_agenda_store_mark_fetch_error(void)
{
    if (s_mutex == NULL) return false;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return false;
    if (s_state.provider != LP_AGENDA_PROVIDER_FETCH_ERROR) {
        s_state.provider = LP_AGENDA_PROVIDER_FETCH_ERROR;
        bump_revision();
    }
    /* Last successful snapshot intentionally retained without refreshing
     * generated_at/source_checked_at. Never mask an error with old data. */
    (void)xSemaphoreGive(s_mutex);
    return true;
}

bool lp_agenda_store_read(lp_agenda_store_state_t *out)
{
    if (out == NULL || s_mutex == NULL) return false;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return false;
    *out = s_state;
    (void)xSemaphoreGive(s_mutex);
    return true;
}
