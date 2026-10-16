#pragma once
#include <stdbool.h>
#include "lvgl.h"

/* All functions must run on the single LVGL-owning P4 main task. */
void lp_wifi_ui_init(lv_obj_t *root, bool touch_ready);
void lp_wifi_ui_tick(void);
