"""Offline configuration and telemetry regression guards for S4 watchdog #51.

These tests do not simulate actual LVGL rendering. The evidence-backed LVGL
9.4 host renderer reproduction (64 KiB hangs, 128 KiB passes at 2/6/12 APs)
is retained privately. CI's native ESP32-P4 build and the resolved Kconfig
checker establish the deployed allocator settings.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
DEFAULTS = (ROOT / "sdkconfig.defaults").read_text(encoding="utf-8")
MAIN = (ROOT / "main/main.c").read_text(encoding="utf-8")
WIFI = (ROOT / "main/wifi_ui.c").read_text(encoding="utf-8")
BUILD_CHECK = (ROOT / "tools/check_c5_build.py").read_text(
    encoding="utf-8"
)


class S4LvglWatchdogGuards(unittest.TestCase):
    def test_builtin_pool_is_explicit_and_at_least_128k(self):
        self.assertRegex(DEFAULTS, r"(?m)^CONFIG_LV_CONF_SKIP=y$")
        self.assertRegex(DEFAULTS, r"(?m)^CONFIG_LV_USE_BUILTIN_MALLOC=y$")
        size = re.search(
            r"(?m)^CONFIG_LV_MEM_SIZE_KILOBYTES=(\d+)$", DEFAULTS
        )
        self.assertIsNotNone(size)
        self.assertGreaterEqual(int(size.group(1)), 128)
        self.assertRegex(
            DEFAULTS, r"(?m)^CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=0$"
        )
        # Guard in effective LVGL header AND post-ESP-IDF resolved sdkconfig.
        self.assertIn("#if LV_MEM_SIZE < (128U * 1024U)", MAIN)
        self.assertIn("#if LV_USE_STDLIB_MALLOC != LV_STDLIB_BUILTIN", MAIN)
        self.assertIn(
            'require("CONFIG_LV_MEM_SIZE_KILOBYTES", "128")', BUILD_CHECK
        )
        self.assertIn(
            'require("CONFIG_LV_USE_BUILTIN_MALLOC", "1")', BUILD_CHECK
        )

    def test_telemetry_on_lvgl_owning_task_only(self):
        self.assertIn("lv_mem_monitor_t lv_mem = {0};", MAIN)
        self.assertIn("lv_mem_monitor(&lv_mem);", MAIN)
        self.assertIn("lvgl_pool_b=%u", MAIN)
        self.assertIn("lvgl_free_b=%u", MAIN)
        self.assertIn("lvgl_biggest_free_b=%u", MAIN)
        self.assertIn("lvgl_frag_pct=%u", MAIN)
        self.assertIn("LVGL_AP_LIST ap_count=%u", WIFI)
        self.assertIn("lv_mem_monitor(&mem);", WIFI)
        self.assertNotIn("shown_ssid", WIFI.split("static void log_scan_ui_memory", 1)[1].split("static void render_scan", 1)[0])
        self.assertLess(WIFI.index("lv_list_add_button("),
                        WIFI.index("log_scan_ui_memory(s_ui.snapshot.count);"))

    def test_no_hardware_or_partition_changes_sneak_in(self):
        self.assertIn("capacity = 0x100000", BUILD_CHECK)
        self.assertNotIn("CONFIG_PARTITION_TABLE_CUSTOM=y", DEFAULTS)
        self.assertIn("CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192", DEFAULTS)
        self.assertIn("CONFIG_SLAVE_IDF_TARGET_ESP32C5=y", DEFAULTS)
        self.assertIn("CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE=28", DEFAULTS)

    def test_regression_guard_rejects_previous_64k_default(self):
        # Configuration contract negative case. Not a simulated LVGL run.
        previous = DEFAULTS.replace(
            "CONFIG_LV_MEM_SIZE_KILOBYTES=128",
            "CONFIG_LV_MEM_SIZE_KILOBYTES=64"
        )
        assert_size = lambda text: int(re.search(
            r"(?m)^CONFIG_LV_MEM_SIZE_KILOBYTES=(\d+)$", text
        ).group(1)) >= 128
        self.assertFalse(assert_size(previous))
        self.assertTrue(assert_size(DEFAULTS))


if __name__ == "__main__":
    unittest.main()
