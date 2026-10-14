/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * ZX4D30CE40S-V13 CST3240 single-point input.
 * Wiring and register protocol from the manufacturer's V1.3 reference:
 * https://github.com/smartpanle/ZX4D30CE40S_V13/tree/91a80aa4
 * This file is independently implemented using ESP-IDF 6.0.3 I2C APIs.
 */
#include "touch.h"

#include <stdint.h>
#include "board.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TP_SDA GPIO_NUM_14
#define TP_SCL GPIO_NUM_15
#define TP_INT GPIO_NUM_0
#define TP_RST GPIO_NUM_1
#define TP_ADDR 0x5A
#define TP_REG_POINT 0xD000
#define TP_REG_ID 0xD200
#define TP_ACK 0xAB
#define TP_TIMEOUT_MS 20

static const char *TAG = "lp_touch";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_device;
static int64_t s_last_error_log_us;

static esp_err_t touch_read_reg(uint16_t reg, uint8_t *out, size_t count, int timeout_ms)
{
    const uint8_t address[] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    return i2c_master_transmit_receive(s_device, address, sizeof(address),
                                       out, count, timeout_ms);
}

static esp_err_t touch_ack(void)
{
    const uint8_t data[] = {0xD0, 0x00, TP_ACK};
    return i2c_master_transmit(s_device, data, sizeof(data), TP_TIMEOUT_MS);
}

static void log_io_error(esp_err_t error)
{
    const int64_t now = esp_timer_get_time();
    if (s_last_error_log_us == 0 || now - s_last_error_log_us >= 5000000) {
        ESP_LOGW(TAG, "Touch I2C error: %s; input released", esp_err_to_name(error));
        s_last_error_log_us = now;
    }
}

/* Called only by the single owning LVGL task; never from an interrupt. */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;

    uint8_t report[7] = {0};
    const esp_err_t err = touch_read_reg(TP_REG_POINT, report, sizeof(report),
                                         TP_TIMEOUT_MS);
    if (err != ESP_OK) {
        log_io_error(err);
        return;
    }
    if ((report[0] & 0x04U) == 0) {
        return;
    }

    const uint16_t raw_x = ((uint16_t)report[1] << 4) | ((report[3] >> 4) & 0x0F);
    const uint16_t raw_y = ((uint16_t)report[2] << 4) | (report[3] & 0x0F);
    /* Vendor's landscape orientation mirrors both coordinates.
     * The right/bottom boundary is exclusive in LVGL. */
    if (raw_x < LP_LCD_WIDTH && raw_y < LP_LCD_HEIGHT) {
        data->point.x = (int32_t)(LP_LCD_WIDTH - 1U - raw_x);
        data->point.y = (int32_t)(LP_LCD_HEIGHT - 1U - raw_y);
        data->state = LV_INDEV_STATE_PRESSED;
    }

    const esp_err_t ack_err = touch_ack();
    if (ack_err != ESP_OK) {
        log_io_error(ack_err);
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

bool lp_touch_init(lv_display_t *display)
{
    if (display == NULL) {
        return false;
    }

    /* On this controller INT low during reset selects the vendor's 0x5A address. */
    const gpio_config_t pins = {
        .pin_bit_mask = (1ULL << TP_RST) | (1ULL << TP_INT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&pins);
    if (err != ESP_OK) goto failed;
    err = gpio_set_level(TP_INT, 0);
    if (err != ESP_OK) goto failed;
    err = gpio_set_level(TP_RST, 0);
    if (err != ESP_OK) goto failed;
    vTaskDelay(pdMS_TO_TICKS(10));
    err = gpio_set_level(TP_RST, 1);
    if (err != ESP_OK) goto failed;
    vTaskDelay(pdMS_TO_TICKS(10));
    err = gpio_set_direction(TP_INT, GPIO_MODE_INPUT);
    if (err != ESP_OK) goto failed;
    /* The original panel bring-up waits 1.3s after touch reset.
     * Keep that known-good settling window until hardware validation. */
    vTaskDelay(pdMS_TO_TICKS(1300));

    const i2c_master_bus_config_t bus = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TP_SDA,
        .scl_io_num = TP_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false, /* Board has external pullups. */
    };
    err = i2c_new_master_bus(&bus, &s_bus);
    if (err != ESP_OK) goto failed;

    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TP_ADDR,
        .scl_speed_hz = 400000,
    };
    err = i2c_master_bus_add_device(s_bus, &device, &s_device);
    if (err != ESP_OK) goto failed;

    uint8_t id[3] = {0};
    err = touch_read_reg(TP_REG_ID, id, sizeof(id), 100);
    if (err != ESP_OK) goto failed;

    lv_indev_t *input = lv_indev_create();
    if (input == NULL) {
        err = ESP_ERR_NO_MEM;
        goto failed;
    }
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, touch_read_cb);
    lv_indev_set_display(input, display);
    ESP_LOGI(TAG, "CST3240 at 0x5A: ID %02X %02X %02X, touch enabled",
             id[0], id[1], id[2]);
    return true;

failed:
    ESP_LOGW(TAG, "CST3240 unavailable (%s); display continues without touch",
             esp_err_to_name(err));
    if (s_device != NULL) {
        (void)i2c_master_bus_rm_device(s_device);
        s_device = NULL;
    }
    if (s_bus != NULL) {
        (void)i2c_del_master_bus(s_bus);
        s_bus = NULL;
    }
    (void)gpio_set_level(TP_RST, 1);
    return false;
}
