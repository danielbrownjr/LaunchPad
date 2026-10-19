"""Static bitmap and text-path contracts for the S4 Latin-1 fallback PR.

These are structural/source checks, not a substitute for visual LVGL 9.4
rendering on the physical LCD. ESP-IDF CI compiles both bitmap assets.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CM = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
UI = {p: (ROOT / f"main/{p}").read_text(encoding="utf-8")
      for p in ("dashboard.c", "agenda_ui.c", "wifi_ui.c")}
H = (ROOT / "main/lp_fonts.h").read_text(encoding="utf-8")
PUNCT = (0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x20AC, 0x2122)


def parse_font(px):
    src = (ROOT / f"main/lp_font_latin1_{px}.c").read_text(encoding="utf-8")
    raw = re.search(r"static const uint8_t glyph_bitmap\[\] = \{(.*?)\};", src, re.S)
    desc = re.search(r"static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc\[\] = \{(.*?)\};", src, re.S)
    assert raw and desc
    bpp = [int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]{2}\b", raw.group(1))]
    entries = [
        tuple(int(x) for x in m)
        for m in re.findall(
            r"\.bitmap_index=(\d+), \.adv_w=(\d+), \.box_w=(\d+), "
            r"\.box_h=(\d+), \.ofs_x=(-?\d+), \.ofs_y=(-?\d+)",
            desc.group(1),
        )
    ]
    return src, bpp, entries


class LatinFontTests(unittest.TestCase):
    def test_font_descriptors_cover_complete_latin1_and_selected_punctuation(self):
        for px in (14, 20):
            with self.subTest(px=px):
                src, bitmap, entries = parse_font(px)
                self.assertEqual(len(entries), 96 + len(PUNCT))
                self.assertRegex(src, r"glyph_dsc\[\] = \{\s*\{0\},")
                self.assertIn("range_start=0x00A0, .range_length=0x60", src)
                self.assertIn("glyph_id_start=97", src)
                self.assertIn("LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY", src)
                self.assertIn("LV_FONT_FMT_TXT_CMAP_SPARSE_TINY", src)
                self.assertIn("punctuation_offsets[] = { 0, 1, 5, 6, 9, 10, 19, 153, 271 }", src)
                self.assertIn(".fallback=&lv_font_montserrat_" + str(px), src)
                self.assertIn(".bpp=2", src)
                self.assertIn("lp_font_latin1_" + str(px) + " = {", src)
                self.assertIn(".line_height=" + ("16" if px == 14 else "22"), src)
                self.assertIn(".base_line=" + ("3" if px == 14 else "4"), src)

                for i, (offset, adv, w, h, x, y) in enumerate(entries):
                    self.assertGreaterEqual(adv, 0)
                    self.assertLessEqual(w, 32)
                    self.assertLessEqual(h, 24)
                    self.assertGreaterEqual(x, -32)
                    self.assertLessEqual(y, 32)
                    next_offset = entries[i + 1][0] if i < len(entries) - 1 else len(bitmap)
                    self.assertEqual(next_offset - offset, (w * h + 3) // 4)
                    cp = 0xA0 + i if i < 96 else PUNCT[i - 96]
                    self.assertIn(f"U+{cp:04X}", src)

    def test_extended_glyphs_have_real_pixels_and_fit_existing_lines(self):
        for px, line_height, base_line in ((14, 16, 3), (20, 22, 4)):
            _, bitmap, entries = parse_font(px)
            for cp in (0x00C1, 0x00E9, 0x00FC, 0x00E3, 0x00F1,
                       0x00E7, 0x2014, 0x201C, 0x201D, 0x2026):
                idx = cp - 0xA0 if cp < 0x100 else PUNCT.index(cp) + 96
                offset, adv, w, h, x, y = entries[idx]
                pixels = [((bitmap[offset + (i // 4)] >> (6 - 2 * (i % 4))) & 3)
                          for i in range(w * h)]
                self.assertGreater(adv, 0, (px, hex(cp)))
                self.assertTrue(any(pixels), (px, hex(cp), "missing actual glyph pixels"))
                self.assertLessEqual(h + y, line_height - base_line,
                                     (px, hex(cp), "accent clipped at top"))
                self.assertGreaterEqual(y, -base_line,
                                        (px, hex(cp), "descender clipped at bottom"))

    def test_widget_text_uses_extended_fonts_but_clock_stays_montserrat(self):
        self.assertIn('"lp_font_latin1_14.c"', CM)
        self.assertIn('"lp_font_latin1_20.c"', CM)
        self.assertIn("LV_FONT_DECLARE(lp_font_latin1_14)", H)
        self.assertIn("LV_FONT_DECLARE(lp_font_latin1_20)", H)
        for path, code in UI.items():
            with self.subTest(path=path):
                self.assertIn('#include "lp_fonts.h"', code)
                self.assertNotIn("&lv_font_montserrat_14", code)
                self.assertNotIn("&lv_font_montserrat_20", code)
        self.assertIn("&lv_font_montserrat_40", UI["dashboard.c"])
        self.assertIn("lv_obj_set_style_text_font(s_ui.ap_list, &lp_font_latin1_14, 0)", UI["wifi_ui.c"])
        self.assertIn("lv_obj_set_style_text_font(s_ui.ssid, &lp_font_latin1_14, 0)", UI["wifi_ui.c"])

    def test_diagnostics_exposes_fictional_glyph_fixtures_not_live_records(self):
        self.assertIn("Café Müller — São Paulo | Zoë, garçon", UI["dashboard.c"])
        self.assertIn("Résumé “façade” – mañana, crème brûlée", UI["dashboard.c"])
        self.assertIn("SIL OPEN FONT LICENSE", (ROOT / "main/fonts-Noto-OFL.txt").read_text())
        config = (ROOT / "sdkconfig.defaults").read_text(encoding="utf-8")
        self.assertIn("CONFIG_LV_MEM_SIZE_KILOBYTES=128", config)
        self.assertNotIn("CONFIG_PARTITION_TABLE_CUSTOM=y", config)


if __name__ == "__main__":
    unittest.main()
