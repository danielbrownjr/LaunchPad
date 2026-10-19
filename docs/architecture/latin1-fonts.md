# S4 font subsets (issue #53)

**Status: software candidate, requires exact-head ESP32-P4 CI and physical glyph review.**
No change to S4 demo data, PrusaLink, C5, power, NVS, flash partitions or OTA.

## Problem and approach

LVGL 9.4.0's built-in Montserrat 14 and 20 embed ASCII
(`U+0020..U+007F`) and a few symbols, but do not carry normal Latin-1
accents. S3 already preserves the UTF-8 bytes. The limitation was **glyph
coverage**, not UTF-8 parsing.

Two custom **read-only 2-bpp LVGL `fmt_txt` fonts** now cover
`U+00A0..U+00FF` (including both cases) plus
`U+2013/U+2014` en/em dash, `U+2018/U+2019` single smart quotes,
`U+201C/U+201D` double smart quotes, `U+2026` ellipsis,
`U+20AC` euro and `U+2122` trademark. Each falls back to the
original **Montserrat 14 or 20** for ASCII, degree/FontAwesome symbols
and anything not included. The 40-px clock uses the unchanged
Montserrat 40 glyphs.

The bitmaps come from Noto Sans Regular, distributed under **SIL OFL
1.1** (license in `main/fonts-Noto-OFL.txt`). Noto Sans is used *only*
for the missing Unicode characters; English headers/numbers and icons
retain Montserrat. This intentional small typeface mismatch must be
visually reviewed at entryway distance before shipping a real feed.
Neither source TTF nor binary font files are checked into this repo.

Source font SHA-256 (NotoSans-Regular.ttf from the build-host distribution):
`89c3c497f618fdaa0b2d1e98fef93582f28c71debd2c4a8cdf41f190ced2909d`

Rendering notes:
- `lp_font_latin1_14.c`: source 13 px, 2-bpp subpixel-free quantization
  mapped into Montserrat's existing 16 px line-height and 3 px baseline.
- `lp_font_latin1_20.c`: 2-bpp Noto-derived bitmap shapes scaled from
  the verified 13 px subset at ratio 18/13 with 2-D interpolation.
  Mapped to the existing 22 px line-height and 4 px baseline. This is
  **not** an independently hinted 20 px font and should be visually
  inspected for softer or heavier glyphs.
- Compact bitmaps are `const` and consume flash/RODATA, not the
  built-in 128 KiB LVGL heap. No glyph allocation during refresh.
- This is **not all Unicode**. Characters outside the listed ranges
  remain unsupported and must not be silently transliterated.
- Decomposed combining accent sequences (e.g., `e` followed by U+0301)
  are **not** normalized. The intended feed path must document its
  normalization policy before promising visual equivalence with
  precomposed Latin-1.

## Wiring and on-screen check

`lp_font_latin1_14/20` are used for agenda, dashboard and Wi-Fi labels,
including the scanned AP list and SSID form. The original 40 px clock
remains untouched. Diagnostics provides two **fictional** test strings:
`Café Müller — São Paulo | Zoë, garçon` (20 px) and
`Résumé “façade” – mañana, crème brûlée` (14 px).
This provides deterministic on-screen visual inspection without touching
real appointment data.

Independent acceptance:

1. Confirm native source tests and pinned ESP-IDF 6.0.3 ESP32-P4 build
   pass, including existing watchdog/heap configuration tests, and check
   1 MiB factory partition fit. Do not change partition scheme to fit.
2. Check C/bitmap glyph decoding against LVGL 9.4.0 and render the
   Diagnostics samples on a native/headless LVGL host if available.
3. **Only with fresh owner authorization**, flash the exact app-only
   image at `0x10000`, with private S4 backup/hash/readback. Never write
   bootloader, NVS, C5 or partition table.
4. On the physical LCD check accent placement, descender clipping,
   Smart quotes/em dash/ellipsis, `Café`, `Müller`, `São`, and
   `brûlée` at both sizes. Inspect long accented agenda titles and
   non-ASCII SSIDs; verify truncation and touch.
5. Re-run short Wi-Fi scan -> Settings -> Diagnostics/Close and check
   LVGL pool headroom + watchdogs. The 128 KiB allocator regression
   must remain fixed, independent of font coverage.
6. Keep the font PR separate from actual live agenda-provider work.
   Issue #53 is only done after glyph, sizing and hardware acceptance.

## Current limitations

- Dynamic combining marks, scripts outside Latin-1, emoji, other currency
  symbols, and right-to-left shaping aren't covered.
- Custom fallback glyphs are Noto rather than Montserrat and are 2bpp
  rather than the existing Montserrat 4bpp.
- Quality and exact metrics on the physical screen remain unverified.
- The earlier off-centre Diagnostics button is independent [issue #54](https://github.com/danielbrownjr/LaunchPad/issues/54).

Original LVGL 9.4 font/fallback documentation:
https://lvgl.io/docs/open/9.4/details/main-modules/font.html
