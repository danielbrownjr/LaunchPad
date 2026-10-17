#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Versioned, bounded *display* data. Upstream providers and authentication
 * are deliberately absent from this module; no Simpro fields are assumed. */
#define LP_AGENDA_SCHEMA_VERSION 1U
#define LP_AGENDA_MAX_ITEMS 8U
#define LP_AGENDA_ID_LEN 24U
#define LP_AGENDA_LABEL_LEN 48U
#define LP_AGENDA_LOCATION_LEN 64U
#define LP_AGENDA_TTL_MIN_S 60U
#define LP_AGENDA_TTL_MAX_S 86400U

typedef enum {
    LP_AGENDA_SOURCE_DEMO = 1,
    LP_AGENDA_SOURCE_BRIDGE = 2, /* Future, only after authorization. */
} lp_agenda_source_t;

typedef enum {
    LP_AGENDA_SCHEDULED = 1,
    LP_AGENDA_CANCELLED = 2,
    LP_AGENDA_UNSCHEDULED = 3,
} lp_agenda_item_status_t;

typedef struct {
    char id[LP_AGENDA_ID_LEN + 1];
    char label[LP_AGENDA_LABEL_LEN + 1];
    char location_label[LP_AGENDA_LOCATION_LEN + 1];
    int64_t starts_at_utc; /* Unix seconds. 0 only for UNSCHEDULED. */
    int64_t ends_at_utc;   /* Exclusive; 0 only for UNSCHEDULED. */
    lp_agenda_item_status_t status;
} lp_agenda_item_t;

typedef struct {
    uint32_t schema_version;
    lp_agenda_source_t source;
    int64_t generated_at_utc;      /* When bridge generated this view. */
    int64_t source_updated_at_utc; /* Last upstream-confirmed data update. */
    uint32_t ttl_seconds;          /* 60..86400, freshness is source-specific. */
    size_t count;
    lp_agenda_item_t items[LP_AGENDA_MAX_ITEMS];
} lp_agenda_snapshot_t;

typedef enum {
    LP_AGENDA_VIEW_INVALID = 0,
    LP_AGENDA_VIEW_CLOCK_UNKNOWN,
    LP_AGENDA_VIEW_DEMO,
    LP_AGENDA_VIEW_STALE,
    LP_AGENDA_VIEW_NO_JOBS,
    LP_AGENDA_VIEW_CURRENT,
} lp_agenda_view_t;

/* Reject nonterminated, invalid UTF-8 and control strings, duplicate IDs,
 * invalid dates/status, and over-limit lists. No allocation or logging. */
bool lp_agenda_validate(const lp_agenda_snapshot_t *snapshot);

/* Calculate data provenance/freshness from the *data* timestamps. An active
 * Wi-Fi connection is NOT evidence of a fresh schedule. DEMO is never CURRENT.
 * Callers render offline/network status separately. */
lp_agenda_view_t lp_agenda_view(const lp_agenda_snapshot_t *snapshot,
                                int64_t now_utc, bool clock_valid);

/* Pick earliest still-active or future scheduled job, inclusive of an
 * ongoing appointment (start <= now < end). Ignore cancelled/unscheduled and
 * expired appointments. out_index must be valid. Never mutate the snapshot. */
bool lp_agenda_next(const lp_agenda_snapshot_t *snapshot,
                    int64_t now_utc, size_t *out_index);
