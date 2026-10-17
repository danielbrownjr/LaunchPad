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
        self.assertIn("NETWORK ERROR", UI)
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
        self.assertIn("CONFIG_SLAVE_IDF_TARGET_ESP32C5=y", CONFIG)
        self.assertIn("CONFIG_ESP_HOSTED_P4_DEV_BOARD_NONE=y", CONFIG)
        self.assertIn("CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CMD_SLOT_1=31", CONFIG)
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
        self.assertIn("esp_wifi_connect()", NETWORK)
        self.assertNotIn("WIFI_SSID", NETWORK)
        self.assertIn("LP_NET_UNCONFIGURED", NET_H)
        self.assertIn("lp_dashboard_set_network_state(lp_network_state())", MAIN)

    def test_native_ci_validates_resolved_c5_configuration(self):
        workflow = (ROOT / ".github/workflows/p4-build.yml").read_text(encoding="utf-8")
        checker = (ROOT / "tools/check_c5_build.py").read_text(encoding="utf-8")
        self.assertIn("python3 tools/check_c5_build.py", workflow)
        self.assertIn("CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5", checker)
        self.assertIn("CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE", checker)
        self.assertIn("build/launchpad.bin", checker)

    def test_clock_sync_age_label_is_explicit_and_separate_from_validity(self):
        # Actual runtime SNTP interval is independent from the dashboard
        # label. The time remains displayed after a confirmed sync.
        self.assertIn('lv_label_set_text(s_clock_status, "LAST SYNC >24H")', UI)
        self.assertNotIn('lv_label_set_text(s_clock_status, "SYNC STALE")', UI)
        self.assertIn('lv_label_set_text(s_clock_status, "TIME SYNCED")', UI)
        self.assertIn('lv_label_set_text(s_clock_status, "OFFLINE TIME")', UI)
        self.assertIn('lv_label_set_text(s_clock_status, "CLOCK NOT SET")', UI)
        self.assertIn('elapsed > pdMS_TO_TICKS(24U * 60U * 60U * 1000U)', NETWORK)
        self.assertLess(UI.index('if (!lp_network_clock_valid())'),
                        UI.index('if (lp_network_clock_stale())'))

    def test_hourly_metrics_and_credential_free_wifi_events(self):
        # The LVGL task is app_main; no fabricated separate LVGL task.
        self.assertIn("#define LP_SOAK_DIAG_INTERVAL_S 3600", MAIN)
        self.assertIn("#ifndef LP_SOAK_TELEMETRY_ENABLED", MAIN)
        self.assertIn("heap_caps_get_free_size(MALLOC_CAP_INTERNAL", MAIN)
        self.assertIn("heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL", MAIN)
        self.assertIn("heap_caps_get_free_size(MALLOC_CAP_SPIRAM", MAIN)
        self.assertIn("heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM", MAIN)
        self.assertIn('main_stack_hwm_b=%u network_stack_hwm_b=%u', MAIN)
        self.assertIn("network_sample_age_s=%u network_valid=%u", MAIN)
        self.assertIn("lp_network_diag_snapshot()", MAIN)
        self.assertIn("uxTaskGetStackHighWaterMark(NULL)", NETWORK)
        self.assertIn("sample_worker_stack();", NETWORK)
        self.assertIn("static atomic_bool s_worker_stack_valid", NETWORK)
        for event in ("Wi-Fi save request queued",
                      "Wi-Fi forget request queued",
                      "Wi-Fi save committed to P4 NVS",
                      "Wi-Fi forget persisted in P4 NVS"):
            self.assertIn(f'ESP_LOGI(TAG, "{event}")', NETWORK)
        # All diagnostic event markers have literal, fixed formats only.
        for line in NETWORK.splitlines():
            if 'ESP_LOGI(TAG, "Wi-Fi save' in line or 'ESP_LOGI(TAG, "Wi-Fi forget' in line:
                self.assertNotIn("%", line)
                self.assertNotIn("ssid", line.lower())
                self.assertNotIn("password", line.lower())
        self.assertNotIn("esp_restart()", MAIN)
        self.assertNotIn("esp_restart()", NETWORK)

    def test_credentials_dhcp_and_trustworthy_clock(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        clock = (ROOT / "main/clock_util.c").read_text(encoding="utf-8")
        self.assertIn("credentials.c", cmake)
        self.assertIn("clock_util.c", cmake)
        self.assertIn("nvs_set_blob", NETWORK)
        self.assertIn("lp_secure_wipe", NETWORK)
        self.assertIn("WIFI_STORAGE_RAM", NETWORK)
        self.assertIn("IP_EVENT_STA_GOT_IP", NETWORK)
        self.assertIn("IP_EVENT_STA_LOST_IP", NETWORK)
        self.assertIn("WIFI_EVENT_STA_DISCONNECTED", NETWORK)
        self.assertIn("esp_netif_sntp_init", NETWORK)
        self.assertIn("time_sync_cb", NETWORK)
        self.assertIn("lp_network_clock_valid()", UI)
        self.assertIn("lp_dashboard_update_clock()", MAIN)
        self.assertIn("localtime_r", clock)
        self.assertIn("CLOCK NOT SET", UI)
        self.assertIn("SYNTHETIC DATA", UI)
        self.assertIn("CONFIG_COMPILER_OPTIMIZATION_SIZE=y", CONFIG)


    def test_wifi_touch_only_through_network_worker(self):
        ui = (ROOT / "main/wifi_ui.c").read_text(encoding="utf-8")
        head = (ROOT / "main/network.h").read_text(encoding="utf-8")
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('"wifi_ui.c"', cmake)
        self.assertIn('"wifi_keyboard.c"', cmake)
        self.assertIn('"wifi_text.c"', cmake)
        keyboard = (ROOT / "main/wifi_keyboard.c").read_text(encoding="utf-8")
        self.assertIn("lp_wifi_keyboard_apply_layout(s_ui.keyboard)", ui)
        self.assertIn("lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER", keyboard)
        self.assertIn("lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER", keyboard)
        self.assertIn("lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_SPECIAL", keyboard)
        self.assertIn('"ABC", "-", "_", " ", ":", LV_SYMBOL_OK', keyboard)
        self.assertIn('"abc", "-", "_", " ", ":", LV_SYMBOL_OK', keyboard)
        self.assertIn("lp_network_ipv4(ipv4, sizeof(ipv4))", ui)
        self.assertIn("lp_network_ipv4(char *out, size_t len)", NETWORK)
        self.assertIn("IPSTR, IP2STR(&got_ip->ip_info.ip)", NETWORK)
        self.assertIn("set_station_ipv4(NULL)", NETWORK)
        self.assertIn("lp_wifi_display_ssid(shown_ssid", ui)
        self.assertIn('lv_label_set_text(s_ui.message, "Saved.")', ui)
        self.assertIn("lp_wifi_ui_init(root, touch_ready)", UI)
        self.assertIn("lp_wifi_ui_tick();", MAIN)
        self.assertIn("lv_textarea_set_password_mode(s_ui.password, true)", ui)
        self.assertIn("lv_textarea_set_password_show_time(s_ui.password, 0)", ui)
        self.assertIn("lv_keyboard_set_textarea(", ui)
        # PR #36 hardware: hidden-form clipping, then bottom-middle default
        # alignment shifted the screen-level keyboard off-screen. Require an
        # explicit top-left screen anchor, not a relative position offset.
        self.assertIn("s_ui.keyboard = lv_keyboard_create(root);", ui)
        self.assertNotIn("lv_keyboard_create(s_ui.form_page)", ui)
        self.assertIn("lv_obj_set_size(s_ui.keyboard, 724, 205)", ui)
        self.assertIn("lv_obj_align(s_ui.keyboard, LV_ALIGN_TOP_LEFT, 38, 239)", ui)
        self.assertNotIn("lv_obj_set_pos(s_ui.keyboard", ui)
        self.assertLess(
            ui.index("lv_obj_set_size(s_ui.keyboard, 724, 205)"),
            ui.index("lv_obj_align(s_ui.keyboard, LV_ALIGN_TOP_LEFT, 38, 239)")
        )
        self.assertIn("lv_obj_remove_flag(s_ui.keyboard, LV_OBJ_FLAG_HIDDEN)", ui)
        self.assertIn("lv_obj_move_foreground(s_ui.keyboard)", ui)
        self.assertIn("lv_obj_update_layout(s_ui.keyboard)", ui)
        self.assertIn("keyboard bounds invalid, refusing credential entry", ui)
        self.assertIn("LV_SYMBOL_SETTINGS", ui)
        self.assertIn("LV_SYMBOL_WIFI", ui)
        self.assertIn("0xFFFFFF", ui)
        self.assertIn("connected ? 0x389AFF : 0x87919F", ui)
        self.assertIn("network == LP_NET_IP_READY", ui)
        self.assertIn("network == LP_NET_SYNCING", ui)
        self.assertIn("network == LP_NET_SYNCED", ui)
        self.assertIn("lp_network_request_scan()", ui)
        self.assertIn("lp_network_submit_credentials(&credentials)", ui)
        self.assertIn("lp_network_forget_saved()", ui)
        self.assertIn("lp_secure_wipe(&credentials, sizeof(credentials))", ui)
        self.assertIn("lv_textarea_set_text(s_ui.password, \"\")", ui)
        self.assertIn("LP_NET_SCAN_MAX 12", head)
        self.assertIn("esp_wifi_scan_start(&config, true)", NETWORK)
        self.assertIn("esp_wifi_scan_get_ap_records", NETWORK)
        self.assertIn("nvs_erase_key", NETWORK)
        self.assertIn("portENTER_CRITICAL", NETWORK)
        self.assertIn("NET_ACTION", NETWORK)
        # No network driver or flash access from the LVGL task.
        for banned in ("esp_wifi_", "nvs_open(", "nvs_set_blob(", "nvs_erase_key("):
            self.assertNotIn(banned, ui)

    def test_no_simpro_data_or_credential_integration(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotRegex(cmake, r"esp_http_client|mqtt")
        self.assertNotIn("simpro", UI.lower())


if __name__ == "__main__":
    unittest.main()
