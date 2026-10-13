#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#define LP_LCD_WIDTH 800
#define LP_LCD_HEIGHT 480

/* ZX4D30CE40S-V13 with WT0132P4-A1 and WT0132C5-S5 only.
 * Pin assignment comes from docs/hardware/pin-resource-matrix.md. */
esp_err_t lp_board_prepare(void);
esp_err_t lp_board_panel_init(esp_lcd_panel_handle_t *out_panel);
esp_err_t lp_board_backlight_on(void);
