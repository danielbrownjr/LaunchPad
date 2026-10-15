#include "dashboard.h"

#include <stdio.h>
#include <stdint.h>

#include "board.h"
#include "clock_util.h"
#include "wifi_ui.h"
#include <time.h>

static lv_obj_t *s_footer;
static lv_obj_t *s_clock_label;
static lv_obj_t *s_clock_status;
static lv_obj_t *s_clock_detail;

void lp_dashboard_update_clock(void)
{
    if (!s_clock_label) return;
    if (!lp_network_clock_valid()) {
        lv_label_set_text(s_clock_label, "--:--");
        lv_label_set_text(s_clock_status, "CLOCK NOT SET");
        lv_label_set_text(s_clock_detail, "Waiting for NTP sync");
        return;
    }
    char formatted[20];
    if (!lp_clock_format(time(NULL), formatted, sizeof(formatted))) return;
    lv_label_set_text(s_clock_label, formatted);
    if (lp_network_clock_stale()) {
        lv_label_set_text(s_clock_status, "SYNC STALE");
    } else if (lp_network_state() == LP_NET_SYNCED) {
        lv_label_set_text(s_clock_status, "TIME SYNCED");
    } else {
        lv_label_set_text(s_clock_status, "OFFLINE TIME");
    }
    lv_label_set_text(s_clock_detail, "Eastern time (automatic DST)");
}

static size_t s_psram_bytes;
static bool s_touch_ready;
static lp_net_state_t s_net_state = LP_NET_OFF;

void lp_dashboard_set_network_state(lp_net_state_t state)
{
    if (s_footer == NULL || state == s_net_state) return;
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
    char status[128];
    if (s_psram_bytes > 0) {
        snprintf(status, sizeof(status),
                 "PSRAM: %u MiB detected   |   P4   |   %s   |   %s",
                 (unsigned)(s_psram_bytes / (1024U * 1024U)), net,
                 s_touch_ready ? "TOUCH I2C READY" : "TOUCH OFFLINE");
    } else {
        snprintf(status, sizeof(status),
                 "PSRAM: NOT DETECTED   |   P4   |   %s   |   %s",
                 net, s_touch_ready ? "TOUCH I2C READY" : "TOUCH OFFLINE");
    }
    lv_label_set_text(s_footer, status);
}

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

static lv_obj_t *card_at(lv_obj_t *parent, int x, int y, int width, int height,
                          uint32_t background)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_bg_color(card, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 15, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void touch_test_clicked(lv_event_t *event)
{
    static uint32_t count;
    lv_obj_t *label = lv_event_get_user_data(event);
    if (count < UINT32_MAX) ++count;
    lv_label_set_text_fmt(label, "TOUCH OK: %lu", (unsigned long)count);
}

void lp_dashboard_create(lv_display_t *display, size_t psram_bytes, bool touch_ready)
{
    lv_obj_t *root = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x0B1627), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    label_at(root, "LAUNCHPAD / BOARD BRING-UP", 26, 17, 530, 0x80D7F3, &lv_font_montserrat_20);
    /* Touch Wi-Fi setup replaces the old static demo banner. */

    lv_obj_t *clock = card_at(root, 24, 64, 295, 244, 0x14263D);
    label_at(clock, "LOCAL TIME", 20, 18, 250, 0x9FB4CC, &lv_font_montserrat_14);
    s_clock_label = label_at(clock, "--:--", 20, 52, 255, 0xF4F7FC, &lv_font_montserrat_40);
    s_clock_status = label_at(clock, "CLOCK NOT SET", 20, 118, 260, 0xF9BE62, &lv_font_montserrat_20);
    s_clock_detail = label_at(clock, "Waiting for NTP sync", 20, 163, 255, 0xAABBCF, &lv_font_montserrat_14);

    lv_obj_t *touch_button = lv_button_create(clock);
    lv_obj_set_pos(touch_button, 20, 201);
    lv_obj_set_size(touch_button, 252, 36);
    lv_obj_set_style_bg_color(touch_button, lv_color_hex(0x246B88), 0);
    lv_obj_set_style_radius(touch_button, 8, 0);
    lv_obj_t *touch_label = lv_label_create(touch_button);
    lv_label_set_text(touch_label, touch_ready ? "TAP TO TEST" : "TOUCH OFFLINE");
    lv_obj_set_style_text_font(touch_label, &lv_font_montserrat_14, 0);
    lv_obj_center(touch_label);
    if (touch_ready) {
        lv_obj_add_event_cb(touch_button, touch_test_clicked, LV_EVENT_CLICKED, touch_label);
    } else {
        lv_obj_add_state(touch_button, LV_STATE_DISABLED);
    }

    lv_obj_t *next = card_at(root, 337, 64, 439, 244, 0x152E46);
    label_at(next, "NEXT STOP / SYNTHETIC DATA", 20, 18, 406, 0x80D7F3, &lv_font_montserrat_14);
    label_at(next, "09:30 AM", 20, 48, 380, 0xF4F7FC, &lv_font_montserrat_40);
    label_at(next, "Example instrument visit", 20, 111, 402, 0xF4F7FC, &lv_font_montserrat_20);
    label_at(next, "Sampletown, MA", 20, 155, 390, 0xAEC1D7, &lv_font_montserrat_20);
    label_at(next, "NOT A REAL APPOINTMENT", 20, 207, 398, 0xF9BE62, &lv_font_montserrat_14);

    lv_obj_t *summary = card_at(root, 24, 326, 752, 89, 0x14263D);
    label_at(summary, "TODAY / FICTIONAL", 18, 12, 300, 0x80D7F3, &lv_font_montserrat_14);
    label_at(summary, "14:00  -  Demo service follow-up", 18, 38, 530, 0xF4F7FC, &lv_font_montserrat_20);

    const uint32_t swatches[] = {0xFF3030, 0x20F060, 0x3080FF};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *swatch = card_at(summary, 620 + (i * 38), 30, 28, 33, swatches[i]);
        lv_obj_set_style_radius(swatch, 3, 0);
    }

    s_psram_bytes = psram_bytes;
    s_touch_ready = touch_ready;
    s_footer = label_at(root, "C5 OFF", 26, 439, 755, 0xA6BBCF, &lv_font_montserrat_14);
    /* Force first full status render, even though the static label has a stub. */
    s_net_state = LP_NET_ERROR;
    lp_dashboard_set_network_state(LP_NET_OFF);
    lp_wifi_ui_init(root, touch_ready);
}
