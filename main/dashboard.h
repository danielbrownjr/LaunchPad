#pragma once

#include <stddef.h>
#include "lvgl.h"

/* Deliberately no network, RTC, data feed, or touch driver in this slice. */
void lp_dashboard_create(lv_display_t *display, size_t psram_bytes);
