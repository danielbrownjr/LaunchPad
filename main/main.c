#include <stdint.h>

#include "board.h"
#include "dashboard.h"
#include "esp_err.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "touch.h"
#include "network.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sd_pwr_ctrl_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#if CONFIG_ESP_MAIN_TASK_STACK_SIZE < 8192
#error "LaunchPad UI main task requires at least 8 KiB stack"
#endif

#define LVGL_TICK_MS 2
#define LVGL_DRAW_LINES 48

static const char *TAG = "launchpad";
/* Retained for the whole application lifetime. Do not release while running. */
static sd_pwr_ctrl_handle_t s_ldo4_handle;

static void lvgl_tick(void *unused)
{
    (void)unused;
    lv_tick_inc(LVGL_TICK_MS);
}

static bool color_copy_complete(esp_lcd_panel_handle_t panel,
                                const esp_lcd_rgb_panel_event_data_t *event,
                                void *user_data)
{
    (void)panel;
    (void)event;
    lv_display_flush_ready((lv_display_t *)user_data);
    return false;
}

static void lvgl_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(display);
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                                               area->x2 + 1, area->y2 + 1, pixels);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD flush failed: %s", esp_err_to_name(err));
        lv_display_flush_ready(display);
    }
    /* Success is acknowledged asynchronously by on_color_trans_done.
       Never release the draw buffer while the LCD driver is copying it. */
}

void app_main(void)
{
    /* Vendor-parity experiment only. N16R32X VDDO_4/VDD_IO_5 wiring
     * and electrical safety MUST be confirmed before this is flashed. */
    const sd_pwr_ctrl_ldo_config_t ldo4_cfg = {
        .ldo_chan_id = 4,
    };
    ESP_ERROR_CHECK(sd_pwr_ctrl_new_on_chip_ldo(&ldo4_cfg, &s_ldo4_handle));
    ESP_ERROR_CHECK(sd_pwr_ctrl_set_io_voltage(s_ldo4_handle, 3300));

    ESP_ERROR_CHECK(lp_board_prepare());

    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM initialization failed; refusing to start display");
        ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
    }
    const size_t psram = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM detected: %u bytes", (unsigned)psram);
    if (psram != 32U * 1024U * 1024U) {
        ESP_LOGW(TAG, "Expected 32 MiB PSRAM from module marking; actual size differs");
    }

    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(lp_board_panel_init(&panel));

    lv_init();
    lv_display_t *display = lv_display_create(LP_LCD_WIDTH, LP_LCD_HEIGHT);
    if (!display) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    lv_display_set_user_data(display, panel);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    const size_t draw_bytes = LP_LCD_WIDTH * LVGL_DRAW_LINES * sizeof(uint16_t);
    void *draw = esp_lcd_rgb_alloc_draw_buffer(panel, draw_bytes, 0);
    if (!draw) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffer");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    lv_display_set_buffers(display, draw, NULL, draw_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, lvgl_flush);

    const esp_lcd_rgb_panel_event_callbacks_t callbacks = {
        .on_color_trans_done = color_copy_complete,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, display));

    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick, LVGL_TICK_MS * 1000));

    const bool touch_ready = lp_touch_init(display);
    lp_dashboard_create(display, psram, touch_ready);
    ESP_LOGI(TAG, "main stack minimum free after dashboard: %u bytes",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    (void)lv_timer_handler();  /* Paint the initial frame before enabling backlight. */
    ESP_ERROR_CHECK(lp_board_backlight_on());
    ESP_LOGI(TAG, "Dashboard running; touch %s; clock unsynchronized",
             touch_ready ? "ready" : "offline");

    /* Hosted transport and remote Wi-Fi can block if the C5 firmware differs.
     * Start only after the first frame is visible and never on the LVGL task. */
    const esp_err_t net_err = lp_network_start();
    if (net_err != ESP_OK) {
        ESP_LOGE(TAG, "C5 network worker creation failed: %s", esp_err_to_name(net_err));
    }
    lp_dashboard_set_network_state(lp_network_state());

    /* Single-owner LVGL loop; there is no competing UI task. */
    int64_t last_clock_second = -1;
    while (true) {
        lp_dashboard_set_network_state(lp_network_state());
        int64_t second = esp_timer_get_time() / 1000000;
        if (second != last_clock_second) {
            last_clock_second = second;
            lp_dashboard_update_clock();
        }
        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms < 10) wait_ms = 10;
        if (wait_ms > 100) wait_ms = 100;
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}
