#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
#include "network.h"

/* Only the LVGL-owning task calls this screen controller. */
void lp_dashboard_create(lv_display_t *display, size_t psram_bytes, bool touch_ready);
/* Call only from the single owning LVGL task. */
void lp_dashboard_set_network_state(lp_net_state_t state);

/* The existing single LVGL owner calls this once per second. */
void lp_dashboard_update_clock(void);
/* Read one atomic store copy, build truthful view state, then render it. */
void lp_dashboard_update_agenda(void);
/* Open the engineering fixture from Settings without cluttering home. */
void lp_dashboard_open_diagnostics(void);
