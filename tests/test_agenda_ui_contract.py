"""S4 guardrails for the display-only LVGL wiring.

Static checks complement (not replace) the pinned ESP-IDF compile and physical
800x480 visual/touch verification. They guard the highest-value boundaries.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "main/agenda_ui.c").read_text(encoding="utf-8")
DASH = (ROOT / "main/dashboard.c").read_text(encoding="utf-8")
MAIN = (ROOT / "main/main.c").read_text(encoding="utf-8")
WIFI = (ROOT / "main/wifi_ui.c").read_text(encoding="utf-8")


class AgendaUiContract(unittest.TestCase):
    def test_ui_is_presentation_only(self):
        for forbidden in ("lp_agenda_store_publish(",
                          "lp_agenda_store_mark_fetch_error(",
                          "lv_timer_create(", "xTaskCreate(",
                          "lp_network_start(", "lp_network_request_scan(",
                          "lp_network_submit_credentials(",
                          "lv_keyboard_create("):
            self.assertNotIn(forbidden, UI)
        self.assertIn('void lp_agenda_ui_apply(const lp_agenda_vm_t *view)', UI)
        self.assertIn('void lp_agenda_ui_create(lv_obj_t *root)', UI)

    def test_no_widget_allocation_during_refresh(self):
        match = re.search(
            r"void lp_agenda_ui_apply\(const lp_agenda_vm_t \*view\)\s*\{(.+)\}\s*$",
            UI, flags=re.S)
        self.assertIsNotNone(match)
        refresh = match.group(1)
        self.assertNotIn("_create(", refresh)
        self.assertNotIn("malloc(", refresh)
        self.assertIn("strcmp(lv_label_get_text(label), text)", UI)

    def test_model_controls_hero_and_detail(self):
        for field in ("view->has_hero", "view->dim_schedule", "view->has_then",
                      "view->row_count", "view->banner", "view->freshness",
                      "view->source", "view->rows[i]"):
            self.assertIn(field, UI)
        self.assertIn("LV_OBJ_FLAG_HIDDEN", UI)
        self.assertIn("LV_DIR_VER", UI)
        self.assertIn("lv_obj_move_foreground(s_overlay)", UI)
        self.assertIn("LP_AGENDA_MAX_ITEMS", UI)

    def test_display_uses_store_copy_and_trusted_clock(self):
        self.assertIn("static lp_agenda_store_state_t s_store;", DASH)
        self.assertIn("static lp_agenda_vm_t s_view;", DASH)
        self.assertIn("lp_agenda_store_read(&s_store)", DASH)
        self.assertIn("lp_agenda_vm_build(&s_store", DASH)
        self.assertIn("lp_network_clock_valid()", DASH)
        self.assertIn("lp_agenda_ui_apply(&s_view)", DASH)
        self.assertNotIn("lp_agenda_store_publish(", DASH)

        init = MAIN.index("lp_clock_timezone_init();")
        create = MAIN.index("lp_dashboard_create(")
        self.assertLess(init, create)
        poll = MAIN.index("lp_agenda_demo_poll(")
        refresh = MAIN.index("lp_dashboard_update_agenda();")
        self.assertLess(poll, refresh)

    def test_engineering_fixture_is_not_home(self):
        self.assertNotIn("BOARD BRING-UP", DASH)
        self.assertIn("lp_dashboard_open_diagnostics(void)", DASH)
        self.assertIn('button(s_ui.list_page, "Diagnostics"', WIFI)
        self.assertIn("lp_dashboard_open_diagnostics();", WIFI)
        self.assertIn("TOUCH OK:", DASH)
        self.assertIn("const uint32_t swatches[]", DASH)
        self.assertIn("LV_SYMBOL_SETTINGS", WIFI)

    def test_long_text_truncates_on_one_line(self):
        # LV_LABEL_LONG_MODE_DOTS only truncates a fixed-height label; with
        # content height a 48-byte label wraps over the next line (seen in a
        # headless LVGL 9.4 render of the agenda rows and home preview).
        text_at = re.search(r"static lv_obj_t \*text_at\(.*?\n\}", UI, flags=re.S).group(0)
        self.assertIn("LV_LABEL_LONG_MODE_DOTS", text_at)
        self.assertIn("lv_obj_set_size(label, width, lv_font_get_line_height(font))", text_at)

    def test_agenda_list_never_keeps_a_stale_scroll_offset(self):
        # Hiding rows does not clamp LVGL's scroll offset: with 8 rows
        # scrolled to the bottom, a 2-row update showed an empty list.
        opener = re.search(r"static void open_agenda\(.*?\n\}", UI, flags=re.S).group(0)
        self.assertIn("lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);", opener)
        apply = UI[UI.index("void lp_agenda_ui_apply("):]
        self.assertIn("if (view->row_count != s_last_row_count)", apply)
        self.assertIn("lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);", apply)

    def test_fixed_geometry_inside_screen(self):
        # All top-level cards/overlays remain in the 800x480 viewport.
        for x, y, width, height in re.findall(
            r"card_at\(root,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),", UI + DASH
        ):
            x, y, width, height = map(int, (x, y, width, height))
            self.assertGreater(width, 0)
            self.assertGreater(height, 0)
            self.assertLessEqual(x + width, 800)
            self.assertLessEqual(y + height, 480)


if __name__ == "__main__":
    unittest.main()
