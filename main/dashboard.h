#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
#include "network.h"

/* Fictional schedule and clock remain unchanged. Only C5 link status is live. */
void lp_dashboard_create(lv_display_t *display, size_t psram_bytes, bool touch_ready);
/* Call only from the single owning LVGL task. */
void lp_dashboard_set_network_state(lp_net_state_t state);
