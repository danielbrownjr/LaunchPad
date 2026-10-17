#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "agenda_store.h"

/* Pure C presentation data for the eventual 800x480 LVGL screen.
 * No LVGL, FreeRTOS, network, allocation, or provider-specific APIs.
 * Caller initializes LaunchPad's Eastern timezone before building. */
typedef enum {
    LP_AGENDA_SCREEN_NO_DATA = 0,
    LP_AGENDA_SCREEN_ERROR,
    LP_AGENDA_SCREEN_INVALID,
    LP_AGENDA_SCREEN_CLOCK_UNSYNCED,
    LP_AGENDA_SCREEN_DEMO,
    LP_AGENDA_SCREEN_CURRENT,
    LP_AGENDA_SCREEN_STALE,
    LP_AGENDA_SCREEN_NO_JOBS,
} lp_agenda_screen_state_t;

typedef struct {
    char time[32]; /* Explicit local date, clock, EDT/EST designation. */
    char label[LP_AGENDA_LABEL_LEN + 1];
    char location[LP_AGENDA_LOCATION_LEN + 1];
    char tag[16]; /* NOW / NEXT / LATER / DONE / CANCELLED / ... */
} lp_agenda_vm_row_t;

typedef struct {
    uint64_t revision;
    lp_agenda_screen_state_t state;
    bool network_online;  /* Transport fact only; never freshness evidence. */
    bool dim_schedule;    /* Render old/unverified records as last known. */
    bool has_hero;
    bool has_then;
    char banner[32];
    char source[32];
    char freshness[48];
    char hero_time[32];
    char hero_label[LP_AGENDA_LABEL_LEN + 1];
    char hero_location[LP_AGENDA_LOCATION_LEN + 1];
    char hero_hint[48];  /* NOW UNTIL ... or IN N MIN. Never for stale/error. */
    char then_time[32];
    char then_label[LP_AGENDA_LABEL_LEN + 1];
    size_t row_count;
    lp_agenda_vm_row_t rows[LP_AGENDA_MAX_ITEMS];
} lp_agenda_vm_t;

/* All input data comes from ONE lp_agenda_store_read() copy. Snapshot data
 * is never mutated. Results have only fixed-capacity NUL-terminated fields.
 * On invalid/missing data, the screen state is truthful with no false NEXT.
 * Returns false on NULL arguments; on time-conversion failure, reports an
 * INVALID state rather than partially displaying suspect appointments.
 * Active relative labels require a *verified* clock and fresh or DEMO data.
 * DEMO bypasses bridge freshness TTL, including 25-hour fall-back days. */
bool lp_agenda_vm_build(const lp_agenda_store_state_t *store,
                        int64_t now_utc, bool clock_valid,
                        bool network_online, lp_agenda_vm_t *out);
