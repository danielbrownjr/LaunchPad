"""Cheap source-level invariants for the first, deliberately disconnected slice."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = (ROOT / "main/board.c").read_text(encoding="utf-8")
UI = (ROOT / "main/dashboard.c").read_text(encoding="utf-8")
NETWORK = (ROOT / "main/network.c").read_text(encoding="utf-8")
NET_H = (ROOT / "main/network.h").read_text(encoding="utf-8")
MAIN = (ROOT / "main/main.c").read_text(encoding="utf-8")
TOUCH = (ROOT / "main/touch.c").read_text(encoding="utf-8")
CONFIG = (ROOT / "sdkconfig.defaults").read_text(encoding="utf-8")
MANIFEST = (ROOT / "main/idf_component.yml").read_text(encoding="utf-8")


class BringupGuards(unittest.TestCase):
    def test_expected_rgb565_gpio_order(self):
        block = BOARD.split(".data_gpio_nums = {", 1)[1].split("}", 1)[0]
        pins = [int(x) for x in re.findall(r"GPIO_NUM_(\d+)", block)]
        self.assertEqual(
            pins, [53, 52, 51, 50, 48, 4, 11, 12, 13, 3, 2, 9, 8, 7, 6, 5]
        )

    def test_rgb_pins_and_c5_gate(self):
        self.assertIn("GPIO_NUM_10", BOARD)
        self.assertIn("GPIO_NUM_54", BOARD)
        self.assertIn("GPIO_NUM_28", BOARD)
        self.assertIn("gpio_set_level(LP_C5_ENABLE_GPIO, 0)", BOARD)
        self.assertIn("gpio_set_level(LP_LCD_BL_GPIO, 0)", BOARD)

    def test_not_misrepresenting_data_or_clock(self):
        self.assertIn("CLOCK NOT SET", UI)
        self.assertIn("SYNTHETIC DATA", UI)
        self.assertIn("NOT A REAL APPOINTMENT", UI)
        self.assertIn("C5 OFF", UI)
        self.assertIn("C5 STARTING", UI)
        self.assertIn("WIFI NOT CONFIGURED", UI)
        self.assertIn("C5 LINK ERROR", UI)
        self.assertIn("TOUCH I2C READY", UI)
        self.assertIn("TOUCH OFFLINE", UI)
        self.assertIn("TAP TO TEST", UI)

    def test_exact_software_baseline(self):
        self.assertIn('version: "==6.0.3"', MANIFEST)
        self.assertIn('version: "==9.4.0"', MANIFEST)
        self.assertIn('CONFIG_IDF_TARGET="esp32p4"', CONFIG)
        self.assertIn("CONFIG_SPIRAM_MODE_HEX=y", CONFIG)

    def test_main_stack_covers_touch_and_dashboard(self):
        self.assertIn("CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192", CONFIG)
        self.assertIn("#if CONFIG_ESP_MAIN_TASK_STACK_SIZE < 8192", MAIN)
        self.assertIn("uxTaskGetStackHighWaterMark(NULL)", MAIN)

    def test_touch_mapping_matches_physical_screen_orientation(self):
        # A touch at (629,196) wrongly activated the button near (170,283).
        # The old mirror produced (799-629, 479-196) == (170,283).
        self.assertEqual((799 - 629, 479 - 196), (170, 283))
        self.assertIn("data->point.x = (int32_t)raw_x;", TOUCH)
        self.assertIn("data->point.y = (int32_t)raw_y;", TOUCH)
        self.assertNotIn("LP_LCD_WIDTH - 1U - raw_x", TOUCH)
        self.assertNotIn("LP_LCD_HEIGHT - 1U - raw_y", TOUCH)
        self.assertIn("raw_x < LP_LCD_WIDTH && raw_y < LP_LCD_HEIGHT", TOUCH)

    def test_ldo4_candidate_precedes_board_and_panel_setup(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("sdmmc", cmake)
        self.assertIn("static sd_pwr_ctrl_handle_t s_ldo4_handle;", MAIN)
        self.assertIn(".ldo_chan_id = 4", MAIN)
        self.assertIn("sd_pwr_ctrl_set_io_voltage(s_ldo4_handle, 3300)", MAIN)
        self.assertLess(MAIN.index("sd_pwr_ctrl_new_on_chip_ldo("),
                        MAIN.index("sd_pwr_ctrl_set_io_voltage("))
        self.assertLess(MAIN.index("sd_pwr_ctrl_set_io_voltage("),
                        MAIN.index("lp_board_prepare()"))

    def test_touch_is_optional_and_wired_to_v13(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('"touch.c"', cmake)
        self.assertIn("esp_driver_i2c", cmake)
        for literal in ["GPIO_NUM_14", "GPIO_NUM_15", "GPIO_NUM_0", "GPIO_NUM_1",
                        "0x5A", "0xD000", "0xD200", "0xAB", "400000"]:
            self.assertIn(literal, TOUCH)
        self.assertIn("i2c_master_transmit_receive", TOUCH)
        self.assertIn("LV_INDEV_STATE_RELEASED", TOUCH)
        self.assertIn("raw_x < LP_LCD_WIDTH && raw_y < LP_LCD_HEIGHT", TOUCH)
        self.assertIn("if (err != ESP_OK) goto failed;", TOUCH)
        self.assertIn("return false;", TOUCH)
        self.assertIn("lv_indev_set_read_cb", TOUCH)
        self.assertIn("lv_indev_set_display", TOUCH)
        self.assertIn("touch_ready = lp_touch_init(display)", MAIN)
        self.assertIn("touch_test_clicked", UI)

    def test_c5_transport_is_board_specific_and_pinned(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("==2.12.3", MANIFEST)
        self.assertIn("==1.6.1", MANIFEST)
        self.assertIn("CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y", CONFIG)
        self.assertNotIn("CONFIG_ESP_HOSTED_CP_TARGET_ESP32C61=y", CONFIG)
        self.assertIn("CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y", CONFIG)
        self.assertIn("CONFIG_ESP_HOSTED_SDIO_CHECKSUM=y", CONFIG)
        pins = {"GPIO_RESET_SLAVE": 28, "PIN_CMD": 31, "PIN_CLK": 32,
                "PIN_D0": 33, "PIN_D1": 34, "PIN_D2": 30, "PIN_D3": 29}
        for key, gpio in pins.items():
            self.assertIn(f"CONFIG_ESP_HOSTED_SDIO_{key}={gpio}", CONFIG.splitlines())
        self.assertIn("network.c", cmake)
        self.assertIn("esp_wifi", cmake)
        self.assertIn("lp_network_start()", MAIN)
        self.assertLess(MAIN.index("lp_board_backlight_on()"), MAIN.index("lp_network_start()"))
        self.assertIn("xTaskCreate(", NETWORK)
        self.assertIn("esp_hosted_init()", NETWORK)
        self.assertIn("esp_wifi_start()", NETWORK)
        self.assertNotIn("esp_wifi_connect()", NETWORK)
        self.assertNotIn("WIFI_SSID", NETWORK)
        self.assertIn("LP_NET_UNCONFIGURED", NET_H)
        self.assertIn("lp_dashboard_set_network_state(lp_network_state())", MAIN)

    def test_no_simpro_data_or_credential_integration(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotRegex(cmake, r"esp_http_client|mqtt")
        self.assertNotIn("simpro", UI.lower())


if __name__ == "__main__":
    unittest.main()
