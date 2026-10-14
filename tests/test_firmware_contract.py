"""Cheap source-level invariants for the first, deliberately disconnected slice."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = (ROOT / "main/board.c").read_text(encoding="utf-8")
UI = (ROOT / "main/dashboard.c").read_text(encoding="utf-8")
MAIN = (ROOT / "main/main.c").read_text(encoding="utf-8")
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
        self.assertIn("TOUCH NOT ENABLED", UI)

    def test_exact_software_baseline(self):
        self.assertIn('version: "==6.0.3"', MANIFEST)
        self.assertIn('version: "==9.4.0"', MANIFEST)
        self.assertIn('CONFIG_IDF_TARGET="esp32p4"', CONFIG)
        self.assertIn("CONFIG_SPIRAM_MODE_HEX=y", CONFIG)

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

    def test_no_network_or_simpro_stack_linked(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotRegex(cmake, r"esp_wifi|esp_hosted|esp_http_client|mqtt")
        self.assertNotIn("simpro", UI.lower())


if __name__ == "__main__":
    unittest.main()
