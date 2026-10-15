#include "board.h"

#include "driver/gpio.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"

#define LP_LCD_BL_GPIO      GPIO_NUM_16
#define LP_C5_ENABLE_GPIO   GPIO_NUM_28
#define LP_RGB_FIXED_LOW    GPIO_NUM_10
#define LP_RGB_FIXED_HIGH   GPIO_NUM_54

static const char *TAG = "lp_board";

esp_err_t lp_board_prepare(void)
{
    /* Do not power the C5: no ESP-Hosted, SDIO or Wi-Fi in this slice. */
    const gpio_config_t control = {
        .pin_bit_mask = (1ULL << LP_LCD_BL_GPIO) |
                        (1ULL << LP_C5_ENABLE_GPIO) |
                        (1ULL << LP_RGB_FIXED_LOW) |
                        (1ULL << LP_RGB_FIXED_HIGH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&control);
    if (err != ESP_OK) return err;

    /* Pins 10 and 54 drive the non-bus R/B LSB duplicates; vendor fixes them high. */
    if ((err = gpio_set_level(LP_LCD_BL_GPIO, 0)) != ESP_OK) return err;
    if ((err = gpio_set_level(LP_C5_ENABLE_GPIO, 0)) != ESP_OK) return err;
    if ((err = gpio_set_level(LP_RGB_FIXED_LOW, 1)) != ESP_OK) return err;
    if ((err = gpio_set_level(LP_RGB_FIXED_HIGH, 1)) != ESP_OK) return err;
    ESP_LOGI(TAG, "Backlight off; C5 power gate disabled");
    return ESP_OK;
}

esp_err_t lp_board_panel_init(esp_lcd_panel_handle_t *out_panel)
{
    if (out_panel == NULL) return ESP_ERR_INVALID_ARG;

    /* RGB565 bus bits 0..15, mapped from the V1.3 schematic.
     * PCLK = 25 MHz, falling-edge sample, sync porches per vendor reference.
     * First hardware run MUST visually verify color order and panel stability. */
    const esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .dma_burst_size = 64,
        .num_fbs = 1,
        .bounce_buffer_size_px = LP_LCD_WIDTH * 20,
        .disp_gpio_num = GPIO_NUM_NC,
        .pclk_gpio_num = GPIO_NUM_49,
        .vsync_gpio_num = GPIO_NUM_46,
        .hsync_gpio_num = GPIO_NUM_47,
        .de_gpio_num = GPIO_NUM_45,
        .data_gpio_nums = {
            GPIO_NUM_53, GPIO_NUM_52, GPIO_NUM_51, GPIO_NUM_50,
            GPIO_NUM_48, GPIO_NUM_4,  GPIO_NUM_11, GPIO_NUM_12,
            GPIO_NUM_13, GPIO_NUM_3,  GPIO_NUM_2,  GPIO_NUM_9,
            GPIO_NUM_8,  GPIO_NUM_7,  GPIO_NUM_6,  GPIO_NUM_5,
        },
        .timings = {
            .pclk_hz = 25 * 1000 * 1000,
            .h_res = LP_LCD_WIDTH,
            .v_res = LP_LCD_HEIGHT,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true,
        .flags.bb_invalidate_cache = true,
    };
    esp_err_t err = esp_lcd_new_rgb_panel(&cfg, out_panel);
    if (err != ESP_OK) return err;
    err = esp_lcd_panel_init(*out_panel);
    if (err != ESP_OK) {
        esp_lcd_panel_del(*out_panel);
        *out_panel = NULL;
    }
    return err;
}

esp_err_t lp_board_backlight_on(void)
{
    /* Fixed full output for the electrical/display slice.
     * Proper PWM dimming with measured knee belongs to a later change. */
    return gpio_set_level(LP_LCD_BL_GPIO, 1);
}
