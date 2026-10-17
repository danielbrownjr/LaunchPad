/* LaunchPad S4. LVGL ownership stays on app_main; never use a provider,
 * clock, mutex, network API or JSON parser from this rendering module. */
#include "agenda_ui.h"

#include <stdio.h>
#include <string.h>

#define C_BG         0x0B1627U
#define C_CARD       0x152E46U
#define C_DETAIL     0x14263DU
#define C_TEXT       0xF4F7FCU
#define C_MUTED      0xAEC1D7U
#define C_DIM        0x76899CU
#define C_ACCENT     0x80D7F3U
#define C_WARNING    0xF9BE62U

typedef struct {
    lv_obj_t *card;
    lv_obj_t *time;
    lv_obj_t *tag;
    lv_obj_t *label;
    lv_obj_t *location;
} ui_row_t;

static lv_obj_t *s_hero_card;
static lv_obj_t *s_banner;
static lv_obj_t *s_source;
static lv_obj_t *s_hero_time;
static lv_obj_t *s_hero_label;
static lv_obj_t *s_hero_location;
static lv_obj_t *s_hero_hint;
static lv_obj_t *s_then;
static lv_obj_t *s_summary;
static lv_obj_t *s_preview_first;
static lv_obj_t *s_preview_second;
static lv_obj_t *s_overlay;
static lv_obj_t *s_detail_banner;
static lv_obj_t *s_detail_source;
static lv_obj_t *s_detail_freshness;
static lv_obj_t *s_empty;
static ui_row_t s_rows[LP_AGENDA_MAX_ITEMS];
static bool s_dim_initialized;
static bool s_last_dim;

static lv_obj_t *text_at(lv_obj_t *parent, const char *initial, int x, int y,
                         int width, uint32_t color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, initial);
    return label;
}

static lv_obj_t *card_at(lv_obj_t *parent, int x, int y, int w, int h,
                         uint32_t color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void update_text(lv_obj_t *label, const char *text)
{
    if (label && strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}

static void set_muted(lv_obj_t *label, bool muted)
{
    lv_obj_set_style_text_color(label,
        lv_color_hex(muted ? C_DIM : C_TEXT), 0);
}

static void close_agenda(lv_event_t *event)
{
    (void)event;
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void open_agenda(lv_event_t *event)
{
    (void)event;
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
}

static lv_obj_t *close_button(lv_obj_t *parent)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, 650, 12);
    lv_obj_set_size(button, 89, 44); /* >= 44px touch target. */
    lv_obj_set_style_bg_color(button, lv_color_hex(0x246B88), 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_t *caption = text_at(button, "Close", 20, 12, 60, C_TEXT,
                                &lv_font_montserrat_14);
    (void)caption;
    lv_obj_add_event_cb(button, close_agenda, LV_EVENT_CLICKED, NULL);
    return button;
}

void lp_agenda_ui_create(lv_obj_t *root)
{
    /* Left clock lives in dashboard.c. The right-hand card and lower agenda
     * preview are exclusively controlled by this module. */
    s_hero_card = card_at(root, 307, 66, 469, 246, C_CARD);
    s_banner = text_at(s_hero_card, "NO SCHEDULE DATA", 18, 12, 296,
                       C_WARNING, &lv_font_montserrat_14);
    s_source = text_at(s_hero_card, "NO SOURCE", 318, 12, 135,
                       C_MUTED, &lv_font_montserrat_14);
    s_hero_time = text_at(s_hero_card, "NO CONFIRMED NEXT STOP", 18, 47, 434,
                          C_TEXT, &lv_font_montserrat_20);
    s_hero_label = text_at(s_hero_card, "Waiting for fictional agenda", 18, 84,
                           432, C_TEXT, &lv_font_montserrat_20);
    s_hero_location = text_at(s_hero_card, "", 18, 123, 432,
                              C_MUTED, &lv_font_montserrat_20);
    s_hero_hint = text_at(s_hero_card, "", 18, 161, 432,
                          C_WARNING, &lv_font_montserrat_20);
    s_then = text_at(s_hero_card, "", 18, 208, 432,
                     C_MUTED, &lv_font_montserrat_14);

    s_summary = card_at(root, 24, 328, 752, 100, C_DETAIL);
    lv_obj_add_flag(s_summary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_summary, open_agenda, LV_EVENT_CLICKED, NULL);
    text_at(s_summary, "AGENDA  /  TAP TO VIEW ALL", 17, 10, 620,
            C_ACCENT, &lv_font_montserrat_14);
    s_preview_first = text_at(s_summary, "No agenda received yet", 17, 39, 716,
                              C_TEXT, &lv_font_montserrat_14);
    s_preview_second = text_at(s_summary, "", 17, 68, 716,
                               C_MUTED, &lv_font_montserrat_14);

    /* One bounded overlay with eight reusable rows. No per-refresh object
     * creation; list scrolling remains within the 800x480 screen. */
    s_overlay = card_at(root, 15, 15, 770, 450, C_BG);
    lv_obj_set_style_border_width(s_overlay, 2, 0);
    lv_obj_set_style_border_color(s_overlay, lv_color_hex(0x39738C), 0);
    s_detail_banner = text_at(s_overlay, "AGENDA", 20, 13, 608,
                              C_ACCENT, &lv_font_montserrat_20);
    s_detail_source = text_at(s_overlay, "NO SOURCE", 20, 46, 220,
                              C_WARNING, &lv_font_montserrat_14);
    s_detail_freshness = text_at(s_overlay, "", 247, 46, 390,
                                 C_MUTED, &lv_font_montserrat_14);
    (void)close_button(s_overlay);

    lv_obj_t *list = lv_obj_create(s_overlay);
    lv_obj_set_pos(list, 19, 82);
    lv_obj_set_size(list, 729, 348);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    s_empty = text_at(list, "No appointments available.", 12, 25, 680,
                      C_MUTED, &lv_font_montserrat_20);

    for (size_t i = 0; i < LP_AGENDA_MAX_ITEMS; ++i) {
        ui_row_t *row = &s_rows[i];
        row->card = card_at(list, 4, (int)i * 94, 699, 88, C_DETAIL);
        row->time = text_at(row->card, "", 13, 9, 465,
                            C_MUTED, &lv_font_montserrat_14);
        row->tag = text_at(row->card, "", 510, 9, 175,
                           C_WARNING, &lv_font_montserrat_14);
        row->label = text_at(row->card, "", 13, 32, 666,
                             C_TEXT, &lv_font_montserrat_20);
        row->location = text_at(row->card, "", 13, 63, 665,
                                C_MUTED, &lv_font_montserrat_14);
        lv_obj_add_flag(row->card, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void show_preview(const lp_agenda_vm_t *view)
{
    size_t shown = 0;
    char formatted[176];
    update_text(s_preview_first, "No appointments available.");
    update_text(s_preview_second, "");
    for (size_t i = 0; i < view->row_count && shown < 2; ++i) {
        const lp_agenda_vm_row_t *row = &view->rows[i];
        /* For a current schedule, prefer unfinished work in the compact
         * view. Full agenda retains DONE and CANCELLED. */
        if (!view->dim_schedule &&
            (strcmp(row->tag, "DONE") == 0 ||
             strcmp(row->tag, "CANCELLED") == 0)) continue;
        (void)snprintf(formatted, sizeof(formatted), "%s  %s  |  %s",
                       row->tag, row->time, row->label);
        update_text(shown == 0 ? s_preview_first : s_preview_second, formatted);
        ++shown;
    }
    if (!shown && view->row_count > 0)
        update_text(s_preview_first, "No remaining scheduled appointments.");
}

void lp_agenda_ui_apply(const lp_agenda_vm_t *view)
{
    if (!view || !s_hero_card) return;

    update_text(s_banner, view->banner);
    update_text(s_source, view->source[0] ? view->source : "NO SOURCE");
    update_text(s_detail_banner, view->banner);
    update_text(s_detail_source, view->source[0] ? view->source : "NO SOURCE");
    update_text(s_detail_freshness, view->freshness);
    bool has_hero = view->has_hero && !view->dim_schedule;
    update_text(s_hero_time, has_hero ? view->hero_time :
                "NO CONFIRMED NEXT STOP");
    update_text(s_hero_label, has_hero ? view->hero_label :
                (view->state == LP_AGENDA_SCREEN_NO_DATA
                 ? "Waiting for schedule" : view->banner));
    update_text(s_hero_location, has_hero ? view->hero_location : "");
    update_text(s_hero_hint, has_hero ? view->hero_hint : view->freshness);

    char then_line[128] = "";
    if (has_hero && view->has_then)
        (void)snprintf(then_line, sizeof(then_line), "THEN %s  %s",
                       view->then_time, view->then_label);
    update_text(s_then, then_line);
    /* Style mutations trigger LVGL redraw work. Only restyle on an actual
     * dim-state transition, not on every once-per-second clock refresh. */
    bool restyle = !s_dim_initialized || s_last_dim != view->dim_schedule;
    if (restyle) set_muted(s_hero_label, view->dim_schedule);
    show_preview(view);

    bool empty = view->row_count == 0;
    if (empty) lv_obj_remove_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);

    for (size_t i = 0; i < LP_AGENDA_MAX_ITEMS; ++i) {
        ui_row_t *ui = &s_rows[i];
        if (i >= view->row_count) {
            lv_obj_add_flag(ui->card, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const lp_agenda_vm_row_t *row = &view->rows[i];
        update_text(ui->time, row->time);
        update_text(ui->tag, row->tag);
        update_text(ui->label, row->label);
        update_text(ui->location, row->location);
        if (restyle || lv_obj_has_flag(ui->card, LV_OBJ_FLAG_HIDDEN))
            set_muted(ui->label, view->dim_schedule);
        lv_obj_remove_flag(ui->card, LV_OBJ_FLAG_HIDDEN);
    }
    s_last_dim = view->dim_schedule;
    s_dim_initialized = true;
}
