/*
 * Touchscreen Wi-Fi settings for the 800x480 V1.3 LaunchPad.
 * No driver, NVS, or network operations occur on the LVGL owning task.
 * Only bounded, nonblocking requests go to the network worker.
 */
#include "wifi_ui.h"
#include "network.h"
#include "credentials.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *list_page;
    lv_obj_t *form_page;
    lv_obj_t *ap_list;
    lv_obj_t *message;
    lv_obj_t *saved;
    lv_obj_t *ssid;
    lv_obj_t *password;
    lv_obj_t *keyboard;
    lp_net_scan_t snapshot;
    lp_net_scan_status_t last_scan;
    lp_net_save_status_t last_save;
    bool forget_confirm;
} wifi_view_t;

static wifi_view_t s_ui;

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xE3EDF8), 0);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
    return obj;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text,
                        int x, int y, int w, int h,
                        lv_event_cb_t callback)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x256B88), 0);
    lv_obj_t *caption = lv_label_create(obj);
    lv_label_set_text(caption, text);
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0);
    lv_obj_center(caption);
    if (callback) lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, NULL);
    return obj;
}

static void show_list(void)
{
    if (!s_ui.panel) return;
    if (s_ui.password) lv_textarea_set_text(s_ui.password, "");
    lv_obj_add_flag(s_ui.form_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_ui.list_page, LV_OBJ_FLAG_HIDDEN);
}

static void close_setup(lv_event_t *event)
{
    (void)event;
    show_list(); /* Clear the masked text, even on cancel. */
    s_ui.forget_confirm = false;
    lv_obj_add_flag(s_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

static void back_to_scan(lv_event_t *event)
{
    (void)event;
    show_list();
}

static void focus_field(lv_event_t *event)
{
    lv_obj_t *area = lv_event_get_target_obj(event);
    lv_keyboard_set_textarea(s_ui.keyboard, area);
}

static void enter_form(const char *network_name)
{
    lv_textarea_set_text(s_ui.ssid, network_name ? network_name : "");
    lv_textarea_set_text(s_ui.password, "");
    lv_keyboard_set_textarea(s_ui.keyboard,
                             network_name ? s_ui.password : s_ui.ssid);
    lv_obj_add_flag(s_ui.list_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_ui.form_page, LV_OBJ_FLAG_HIDDEN);
}

static void picked_network(lv_event_t *event)
{
    const lp_net_ap_t *ap = lv_event_get_user_data(event);
    if (!ap || !ap->personal_wpa) return;
    enter_form(ap->ssid);
}

static void manual_ssid(lv_event_t *event)
{
    (void)event;
    enter_form(NULL);
}

static void scan_pressed(lv_event_t *event)
{
    (void)event;
    s_ui.forget_confirm = false;
    if (lp_network_request_scan()) {
        lv_label_set_text(s_ui.message, "Scanning for nearby networks...");
        s_ui.last_scan = LP_SCAN_RUNNING;
        lv_obj_clean(s_ui.ap_list);
    } else {
        lv_label_set_text(s_ui.message, "C5 not ready or scan already running.");
    }
}

static void save_pressed(lv_event_t *event)
{
    (void)event;
    const char *ssid = lv_textarea_get_text(s_ui.ssid);
    const char *pass = lv_textarea_get_text(s_ui.password);
    lp_credentials_t credentials = {0};

    size_t ssid_len = strnlen(ssid, sizeof(credentials.ssid));
    size_t pass_len = strnlen(pass, sizeof(credentials.passphrase));
    bool valid = ssid_len > 0 && ssid_len <= LP_SSID_MAX &&
                 pass_len >= 8 && pass_len <= LP_PASSPHRASE_MAX;
    if (valid) {
        memcpy(credentials.ssid, ssid, ssid_len);
        memcpy(credentials.passphrase, pass, pass_len);
        valid = lp_credentials_valid(&credentials);
    }
    if (!valid) {
        lv_label_set_text(s_ui.message, "SSID 1-32 bytes, password 8-63 bytes required.");
        lv_obj_add_flag(s_ui.form_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_ui.list_page, LV_OBJ_FLAG_HIDDEN);
    } else if (!lp_network_submit_credentials(&credentials)) {
        lv_label_set_text(s_ui.message, "Wi-Fi busy; try Connect again.");
        lp_secure_wipe(&credentials, sizeof(credentials));
        lv_textarea_set_text(s_ui.password, "");
        return;
    } else {
        lv_label_set_text(s_ui.message, "Saving network to P4, then connecting...");
        s_ui.last_save = LP_SAVE_PENDING;
        show_list();
    }
    lp_secure_wipe(&credentials, sizeof(credentials));
    lv_textarea_set_text(s_ui.password, "");
}

static void forget_pressed(lv_event_t *event)
{
    (void)event;
    char saved[LP_SSID_MAX + 1] = {0};
    lp_network_saved_ssid(saved, sizeof(saved));
    if (saved[0] == '\0') {
        lv_label_set_text(s_ui.message, "No saved network to forget.");
        return;
    }
    if (!s_ui.forget_confirm) {
        s_ui.forget_confirm = true;
        lv_label_set_text(s_ui.message, "Tap Forget again to confirm.");
        return;
    }
    if (lp_network_forget_saved()) {
        lv_label_set_text(s_ui.message, "Removing saved network...");
        s_ui.last_save = LP_SAVE_PENDING;
    } else {
        lv_label_set_text(s_ui.message, "Wi-Fi busy; try forgetting again.");
    }
    s_ui.forget_confirm = false;
}

static void render_scan(void)
{
    lp_network_scan_copy(&s_ui.snapshot);
    lv_obj_clean(s_ui.ap_list);
    if (s_ui.snapshot.count == 0) {
        lv_label_set_text(s_ui.message, "No visible networks. Try Scan or Manual SSID.");
        return;
    }
    lv_label_set_text(s_ui.message, "Tap a WPA network to configure it.");
    for (size_t i = 0; i < s_ui.snapshot.count; ++i) {
        lp_net_ap_t *ap = &s_ui.snapshot.aps[i];
        char line[100];
        snprintf(line, sizeof(line), "%s   %d dBm   %s",
                 ap->ssid, (int)ap->rssi,
                 ap->personal_wpa ? "WPA" : "Unsupported security");
        lv_obj_t *item = lv_list_add_button(s_ui.ap_list, NULL, line);
        if (ap->personal_wpa) {
            lv_obj_add_event_cb(item, picked_network, LV_EVENT_CLICKED, ap);
        } else {
            lv_obj_add_state(item, LV_STATE_DISABLED);
        }
    }
}

static void create_panel(lv_obj_t *root)
{
    s_ui.panel = lv_obj_create(root);
    lv_obj_set_pos(s_ui.panel, 19, 22);
    lv_obj_set_size(s_ui.panel, 760, 432);
    lv_obj_set_style_bg_color(s_ui.panel, lv_color_hex(0x14263D), 0);
    lv_obj_set_style_bg_opa(s_ui.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.panel, 2, 0);
    lv_obj_set_style_border_color(s_ui.panel, lv_color_hex(0x39738C), 0);
    lv_obj_set_style_radius(s_ui.panel, 14, 0);
    lv_obj_set_style_pad_all(s_ui.panel, 4, 0);
    lv_obj_remove_flag(s_ui.panel, LV_OBJ_FLAG_SCROLLABLE);
    label(s_ui.panel, "Wi-Fi settings", 14, 8, 530);
    button(s_ui.panel, "Close", 648, 4, 94, 38, close_setup);

    s_ui.list_page = lv_obj_create(s_ui.panel);
    lv_obj_set_pos(s_ui.list_page, 10, 52);
    lv_obj_set_size(s_ui.list_page, 730, 352);
    lv_obj_set_style_bg_opa(s_ui.list_page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.list_page, 0, 0);
    lv_obj_set_style_pad_all(s_ui.list_page, 2, 0);
    lv_obj_remove_flag(s_ui.list_page, LV_OBJ_FLAG_SCROLLABLE);
    s_ui.saved = label(s_ui.list_page, "Saved: none", 12, 5, 670);
    s_ui.message = label(s_ui.list_page, "Scan to find nearby networks.", 12, 33, 670);
    s_ui.ap_list = lv_list_create(s_ui.list_page);
    lv_obj_set_pos(s_ui.ap_list, 12, 65);
    lv_obj_set_size(s_ui.ap_list, 682, 210);
    button(s_ui.list_page, "Scan", 12, 287, 128, 40, scan_pressed);
    button(s_ui.list_page, "Manual SSID", 153, 287, 182, 40, manual_ssid);
    button(s_ui.list_page, "Forget saved", 350, 287, 170, 40, forget_pressed);

    s_ui.form_page = lv_obj_create(s_ui.panel);
    lv_obj_set_pos(s_ui.form_page, 10, 52);
    lv_obj_set_size(s_ui.form_page, 730, 352);
    lv_obj_set_style_bg_opa(s_ui.form_page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.form_page, 0, 0);
    lv_obj_set_style_pad_all(s_ui.form_page, 2, 0);
    lv_obj_remove_flag(s_ui.form_page, LV_OBJ_FLAG_SCROLLABLE);
    label(s_ui.form_page, "Network", 12, 12, 115);
    s_ui.ssid = lv_textarea_create(s_ui.form_page);
    lv_obj_set_pos(s_ui.ssid, 120, 2);
    lv_obj_set_size(s_ui.ssid, 560, 44);
    lv_textarea_set_one_line(s_ui.ssid, true);
    lv_textarea_set_max_length(s_ui.ssid, LP_SSID_MAX);
    label(s_ui.form_page, "Password", 12, 66, 115);
    s_ui.password = lv_textarea_create(s_ui.form_page);
    lv_obj_set_pos(s_ui.password, 120, 55);
    lv_obj_set_size(s_ui.password, 560, 44);
    lv_textarea_set_one_line(s_ui.password, true);
    lv_textarea_set_max_length(s_ui.password, LP_PASSPHRASE_MAX);
    lv_textarea_set_password_mode(s_ui.password, true);
    lv_textarea_set_password_show_time(s_ui.password, 0);
    button(s_ui.form_page, "Back", 12, 107, 144, 42, back_to_scan);
    button(s_ui.form_page, "Save & connect", 483, 107, 196, 42, save_pressed);
    s_ui.keyboard = lv_keyboard_create(s_ui.form_page);
    lv_obj_set_pos(s_ui.keyboard, 12, 156);
    lv_obj_set_size(s_ui.keyboard, 670, 171);
    lv_keyboard_set_textarea(s_ui.keyboard, s_ui.password);
    lv_obj_add_event_cb(s_ui.ssid, focus_field, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_ui.password, focus_field, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_ui.form_page, LV_OBJ_FLAG_HIDDEN);
}

static void open_setup(lv_event_t *event)
{
    lv_obj_t *trigger = lv_event_get_target_obj(event);
    if (!s_ui.panel) create_panel(lv_obj_get_parent(trigger));
    s_ui.last_scan = LP_SCAN_IDLE;
    s_ui.last_save = LP_SAVE_IDLE;
    s_ui.forget_confirm = false;
    show_list();
    lv_obj_remove_flag(s_ui.panel, LV_OBJ_FLAG_HIDDEN);
    if (lp_network_scan_status() == LP_SCAN_READY) render_scan();
    else scan_pressed(NULL);
}

void lp_wifi_ui_init(lv_obj_t *root, bool touch_ready)
{
    lv_obj_t *open = button(root, "Wi-Fi Setup", 609, 12, 166, 42, open_setup);
    if (!touch_ready) lv_obj_add_state(open, LV_STATE_DISABLED);
}

void lp_wifi_ui_tick(void)
{
    if (!s_ui.panel || lv_obj_has_flag(s_ui.panel, LV_OBJ_FLAG_HIDDEN)) return;
    char saved[LP_SSID_MAX + 1] = {0};
    lp_network_saved_ssid(saved, sizeof(saved));
    char line[72];
    snprintf(line, sizeof(line), "Saved: %s", saved[0] ? saved : "none");
    lv_label_set_text(s_ui.saved, line);

    lp_net_scan_status_t scan = lp_network_scan_status();
    if (scan != s_ui.last_scan) {
        s_ui.last_scan = scan;
        if (scan == LP_SCAN_READY) render_scan();
        else if (scan == LP_SCAN_FAILED)
            lv_label_set_text(s_ui.message, "Scan failed. Retry when Wi-Fi is idle.");
    }
    lp_net_save_status_t save = lp_network_save_status();
    if (save != s_ui.last_save) {
        s_ui.last_save = save;
        if (save == LP_SAVE_OK)
            lv_label_set_text(s_ui.message, "Saved. Wi-Fi connecting automatically.");
        else if (save == LP_SAVE_FORGOTTEN)
            lv_label_set_text(s_ui.message, "Saved network forgotten.");
        else if (save == LP_SAVE_FAILED)
            lv_label_set_text(s_ui.message, "Save/forget failed. Check device log.");
    }
}
