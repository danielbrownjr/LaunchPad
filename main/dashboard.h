#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"

/* Fictional dashboard; touch is optional, network/RTC remain disabled. */
void lp_dashboard_create(lv_display_t *display, size_t psram_bytes, bool touch_ready);
