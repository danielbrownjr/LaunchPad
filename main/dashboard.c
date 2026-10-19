#include "dashboard.h"
#include "lp_fonts.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "agenda_store.h"
#include "agenda_ui.h"
#include "agenda_view.h"
#include "board.h"
#include "clock_util.h"
#include "wifi_ui.h"

/* Only app_main calls these functions. Large DTO/VM buffers live in BSS,
 * not in the 8 KiB LVGL task's stack. */
static lp_agenda_store_state_t s_store;
static lp_agenda_vm_t s_view;

static lv_obj_t *s_root;
static lv_obj_t *s_footer;
static lv_obj_t *s_clock_label;
static lv_obj_t *s_clock_status;
static lv_obj_t *s_clock_detail;
static lv_obj_t *s_diagnostics;

static size_t s_psram_bytes;
static bool s_touch_ready;
static lp_net_state_t s_net_state = LP_NET_ERROR;

static lv_obj_t *label_at(lv_obj_t *parent, const char *value, int x, int y,
                           int width, uint32_t color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

static void label_if_changed(lv_obj_t *label, const char *value)
{
    if (strcmp(lv_label_get_text(label), value) != 0)
        lv_label_set_text(label, value);
}

static lv_obj_t *card_at(lv_obj_t *parent, int x, int y, int width, int height,
                          uint32_t background)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_bg_color(card, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

void lp_dashboard_update_clock(void)
{
    if (!s_clock_label) return;
    if (!lp_network_clock_valid()) {
        label_if_changed(s_clock_label, "--:--");
        label_if_changed(s_clock_status, "CLOCK NOT SET");
        label_if_changed(s_clock_detail, "Waiting for NTP sync");
        return;
    }

    char formatted[20];
    if (!lp_clock_format(time(NULL), formatted, sizeof(formatted))) return;
    label_if_changed(s_clock_label, formatted);
    const char *status;
    if (lp_network_clock_stale()) status = "LAST SYNC >24H";
    else if (lp_network_state() == LP_NET_SYNCED) status = "TIME SYNCED";
    else status = "OFFLINE TIME";
    label_if_changed(s_clock_status, status);
    label_if_changed(s_clock_detail, "Eastern time / automatic DST");
}

void lp_dashboard_set_network_state(lp_net_state_t state)
{
    if (!s_footer || state == s_net_state) return;
    s_net_state = state;
    const char *net;
    switch (state) {
        case LP_NET_STARTING: net = "C5 STARTING"; break;
        case LP_NET_UNCONFIGURED: net = "WIFI NOT CONFIGURED"; break;
        case LP_NET_CONNECTING: net = "WIFI CONNECTING"; break;
        case LP_NET_DISCONNECTED: net = "WIFI DISCONNECTED"; break;
        case LP_NET_IP_READY: net = "DHCP IP OK"; break;
        case LP_NET_SYNCING: net = "CLOCK SYNCING"; break;
        case LP_NET_SYNCED: net = "CLOCK SYNCED"; break;
        case LP_NET_ERROR: net = "NETWORK ERROR"; break;
        default: net = "C5 OFF"; break;
    }
    char text[100];
    (void)snprintf(text, sizeof(text), "NETWORK: %s  |  SCHEDULE STATE ABOVE", net);
    label_if_changed(s_footer, text);
}

void lp_dashboard_update_agenda(void)
{
    if (!s_root) return;
    if (!lp_agenda_store_read(&s_store)) {
        /* The store may be unavailable when its one-time mutex allocation
         * failed. Do not invent an accepted or fresh snapshot. */
        memset(&s_store, 0, sizeof(s_store));
        s_store.provider = LP_AGENDA_PROVIDER_NO_DATA;
    }
    lp_net_state_t network = lp_network_state();
    bool online = network == LP_NET_IP_READY ||
                  network == LP_NET_SYNCING || network == LP_NET_SYNCED;
    if (lp_agenda_vm_build(&s_store, (int64_t)time(NULL),
                           lp_network_clock_valid(), online, &s_view))
        lp_agenda_ui_apply(&s_view);
}

static void touch_test_clicked(lv_event_t *event)
{
    static uint32_t count;
    lv_obj_t *label = lv_event_get_user_data(event);
    if (count < UINT32_MAX) ++count;
    lv_label_set_text_fmt(label, "TOUCH OK: %lu", (unsigned long)count);
}

static void close_diagnostics(lv_event_t *event)
{
    (void)event;
    lv_obj_add_flag(s_diagnostics, LV_OBJ_FLAG_HIDDEN);
}

void lp_dashboard_open_diagnostics(void)
{
    if (!s_root) return;
    if (!s_diagnostics) {
        s_diagnostics = card_at(s_root, 17, 15, 766, 449, 0x14263D);
        lv_obj_set_style_border_width(s_diagnostics, 2, 0);
        lv_obj_set_style_border_color(s_diagnostics, lv_color_hex(0x39738C), 0);
        label_at(s_diagnostics, "LAUNCHPAD / DIAGNOSTICS", 19, 16, 600,
                 0x80D7F3, &lp_font_latin1_20);
        char detail[100];
        (void)snprintf(detail, sizeof(detail),
                       "ESP32-P4  |  PSRAM: %u MiB  |  TOUCH: %s",
                       (unsigned)(s_psram_bytes / (1024U * 1024U)),
                       s_touch_ready ? "I2C READY" : "OFFLINE");
        label_at(s_diagnostics, detail, 20, 74, 685, 0xF4F7FC,
                 &lp_font_latin1_20);
        label_at(s_diagnostics,
                 "RGB panel and touch fixture / engineering tools",
                 20, 130, 680, 0xAEC1D7, &lp_font_latin1_14);

        lv_obj_t *touch = lv_button_create(s_diagnostics);
        lv_obj_set_pos(touch, 20, 181);
        lv_obj_set_size(touch, 280, 50);
        lv_obj_set_style_bg_color(touch, lv_color_hex(0x246B88), 0);
        lv_obj_t *touch_label = label_at(touch, s_touch_ready ?
                              "TAP TO TEST" : "TOUCH OFFLINE", 20, 14, 245,
                              0xFFFFFF, &lp_font_latin1_14);
        if (s_touch_ready)
            lv_obj_add_event_cb(touch, touch_test_clicked, LV_EVENT_CLICKED,
                                touch_label);
        else
            lv_obj_add_state(touch, LV_STATE_DISABLED);

        const uint32_t swatches[] = {0xFF3030U, 0x20F060U, 0x3080FFU};
        for (int i = 0; i < 3; ++i) {
            lv_obj_t *swatch = card_at(s_diagnostics, 20 + i * 115, 270,
                                       101, 56, swatches[i]);
            lv_obj_set_style_radius(swatch, 5, 0);
        }

        lv_obj_t *close = lv_button_create(s_diagnostics);
        lv_obj_set_pos(close, 647, 12);
        lv_obj_set_size(close, 95, 45);
        lv_obj_set_style_bg_color(close, lv_color_hex(0x246B88), 0);
        lv_obj_t *caption = lv_label_create(close);
        lv_label_set_text(caption, "Close");
        lv_obj_set_style_text_font(caption, &lp_font_latin1_14, 0);
        lv_obj_set_style_text_color(caption, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(caption);
        lv_obj_add_event_cb(close, close_diagnostics, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_remove_flag(s_diagnostics, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_diagnostics);
}

void lp_dashboard_create(lv_display_t *display, size_t psram_bytes, bool touch_ready)
{
    lv_obj_t *root = lv_display_get_screen_active(display);
    s_root = root;
    lv_obj_set_style_bg_color(root, lv_color_hex(0x0B1627), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    label_at(root, "LAUNCHPAD", 26, 17, 400, 0x80D7F3,
             &lp_font_latin1_20);

    lv_obj_t *clock = card_at(root, 24, 66, 265, 246, 0x14263D);
    label_at(clock, "LOCAL TIME", 16, 18, 225, 0x9FB4CC,
             &lp_font_latin1_14);
    s_clock_label = label_at(clock, "--:--", 15, 58, 236, 0xF4F7FC,
                             &lv_font_montserrat_40);
    s_clock_status = label_at(clock, "CLOCK NOT SET", 16, 131, 235,
                              0xF9BE62, &lp_font_latin1_20);
    s_clock_detail = label_at(clock, "Waiting for NTP sync", 16, 187, 235,
                              0xAABBCF, &lp_font_latin1_14);

    lp_agenda_ui_create(root);

    s_psram_bytes = psram_bytes;
    s_touch_ready = touch_ready;
    s_footer = label_at(root, "NETWORK: C5 OFF", 26, 446, 744, 0xA6BBCF,
                        &lp_font_latin1_14);
    /* Force first status render, even when the device remains C5_OFF. */
    s_net_state = LP_NET_ERROR;
    lp_dashboard_set_network_state(LP_NET_OFF);
    lp_wifi_ui_init(root, touch_ready);
    lp_dashboard_update_agenda();
}
