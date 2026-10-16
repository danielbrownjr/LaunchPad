idf_component_register(
    SRCS "main.c" "board.c" "dashboard.c" "touch.c" "network.c" "wifi_ui.c" "wifi_keyboard.c" "wifi_text.c" "credentials.c" "clock_util.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd esp_driver_gpio esp_timer esp_psram sdmmc esp_driver_i2c esp_wifi esp_netif esp_event nvs_flash
)
