#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "agenda_model.h"

/* Fictional appointments only. This is not an upstream scheduling source.
 * Times are UTC seconds, constructed from Eastern local calendar days after
 * a genuine clock sync. Caller must initialize LP_TIMEZONE before use. */

/* Pure deterministic generator: 3 fictional calls on the current Eastern
 * local day plus one on the following day. Labels are always DEMO; no heap,
 * networking, LVGL or Simpro dependencies. No output is written if the
 * clock is invalid or input cannot be converted. */
bool lp_agenda_demo_build(int64_t now_utc, bool clock_valid,
                          lp_agenda_snapshot_t *out);

/* Call from the single app_main/LVGL-owning task once per second, after store
 * init. Publishes at most once per Eastern calendar day, and never without
 * a verified clock. No extra FreeRTOS task. Returns true only when a
 * validated snapshot is published. The bridge-source check is best effort,
 * not atomic against another publisher. A future bridge must disable DEMO
 * before activating a concurrent writer. Does not touch LVGL. */
bool lp_agenda_demo_poll(int64_t now_utc, bool clock_valid);
