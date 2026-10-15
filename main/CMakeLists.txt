idf_component_register(
    SRCS "main.c" "board.c" "dashboard.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd esp_driver_gpio esp_timer esp_psram
)
