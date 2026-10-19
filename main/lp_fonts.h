#pragma once

#include "lvgl.h"

/* Small, pinned bitmap fallback collections for Latin-1 and common
 * punctuation. ASCII, LVGL symbols and the 40-px clock remain Montserrat.
 * Generated glyphs are Noto Sans (Google, SIL Open Font License 1.1);
 * see docs/architecture/latin1-fonts.md.
 *
 * These are const and require no runtime heap allocations.
 */
LV_FONT_DECLARE(lp_font_latin1_14)
LV_FONT_DECLARE(lp_font_latin1_20)
