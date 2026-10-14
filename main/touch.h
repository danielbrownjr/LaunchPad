#pragma once

#include <stdbool.h>
#include "lvgl.h"

/* Bring up optional CST3240 touch on the V1.3 board.
 * False means the display may still operate without input. */
bool lp_touch_init(lv_display_t *display);
