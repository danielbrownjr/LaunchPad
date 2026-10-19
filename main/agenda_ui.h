#pragma once

#include "lvgl.h"
#include "agenda_view.h"

/* S4 UI: call ONLY on the app_main LVGL-owning task. Static object tree,
 * no data/source policy. Create once, then apply screen-ready snapshots. */
void lp_agenda_ui_create(lv_obj_t *root);
void lp_agenda_ui_apply(const lp_agenda_vm_t *view);
