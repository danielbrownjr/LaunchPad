/*
 * Touchscreen Wi-Fi settings for the 800x480 V1.3 LaunchPad.
 * No driver, NVS, or network operations occur on the LVGL owning task.
 * Only bounded, nonblocking requests go to the network worker.
 */
#include "wifi_ui.h"
#include "network.h"
#include "credentials.h"

#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "lp_wifi_ui";
#include <string.h>

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *list_page;
    lv_obj_t *form_page;
    lv_obj_t *ap_list;
    lv_obj_t *message;
    lv_obj_t *saved;
    lv_obj_t *network_status;
    lv_obj_t *ip_address;
    lv_obj_t *form_feedback;
    lv_obj_t *ssid;
    lv_obj_t *password;
    lv_obj_t *keyboard;
    lv_obj_t *wifi_indicator;
    bool connected_indicator;
    lp_net_scan_t snapshot;
    lp_net_scan_status_t last_scan;
    lp_net_save_status_t last_save;
    bool forget_confirm;
} wifi_view_t;

static wifi_view_t s_ui;

/* U+2018/U+2019 smart apostrophes are absent from the pinned Montserrat
 * subset. Substitute ASCII apostrophes ONLY for UI labels. The original
 * SSID bytes remain unchanged for selection, connection and P4 NVS. */
static void display_ssid(char *out, size_t capacity, const char *ssid)
{
    if (!out || capacity == 0) return;
    size_t used = 0;
    size_t i = 0;
    size_t len = ssid ? strnlen(ssid, LP_SSID_MAX) : 0;
    while (i < len && used + 1 < capacity) {
        const unsigned char *p = (const unsigned char *)ssid;
        if (i + 2 < len && p[i] == 0xE2 && p[i + 1] == 0x80 &&
            (p[i + 2] == 0x98 || p[i + 2] == 0x99)) {
            out[used++] = '\'';
            i += 3;
        } else {
            out[used++] = ssid[i++];
        }
    }
    out[used] = '\0';
}

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
    if (s_ui.keyboard) lv_obj_add_flag(s_ui.keyboard, LV_OBJ_FLAG_HIDDEN);
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
    s_ui.forget_confirm = false;
    lv_label_set_text(s_ui.form_feedback, "");
    lv_textarea_set_text(s_ui.ssid, network_name ? network_name : "");
    lv_textarea_set_text(s_ui.password, "");
    lv_keyboard_set_textarea(s_ui.keyboard,
                             network_name ? s_ui.password : s_ui.ssid);
    lv_obj_add_flag(s_ui.list_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_ui.form_page, LV_OBJ_FLAG_HIDDEN);
    /* Keep the native LVGL button matrix on the SCREEN, never in the
     * initially hidden / clipped form container. Geometry must be computed
     * after the parent screen and keyboard are both visible. */
    lv_obj_remove_flag(s_ui.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.keyboard);
    lv_obj_update_layout(s_ui.keyboard);
    lv_area_t kb;
    lv_obj_get_coords(s_ui.keyboard, &kb);
    ESP_LOGI(TAG, "keyboard viewport (%ld,%ld)-(%ld,%ld), %ldx%ld",
             (long)kb.x1, (long)kb.y1, (long)kb.x2, (long)kb.y2,
             (long)lv_area_get_width(&kb), (long)lv_area_get_height(&kb));
    if (kb.x1 < 0 || kb.y1 < 0 || kb.x2 >= 800 || kb.y2 >= 480 ||
        lv_area_get_width(&kb) < 670 || lv_area_get_height(&kb) < 185) {
        ESP_LOGE(TAG, "keyboard bounds invalid, refusing credential entry");
        lv_obj_add_flag(s_ui.keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_ui.form_feedback,
                          "Keyboard layout failed; entry disabled.");
    }
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
        lv_label_set_text(s_ui.form_feedback, "Check SSID and password length.");
        lp_secure_wipe(&credentials, sizeof(credentials));
        return;
    } else if (!lp_network_submit_credentials(&credentials)) {
        lv_label_set_text(s_ui.form_feedback, "Wi-Fi busy, retry shortly.");
        lp_secure_wipe(&credentials, sizeof(credentials));
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
        char shown_ssid[LP_SSID_MAX + 1];
        display_ssid(shown_ssid, sizeof(shown_ssid), ap->ssid);
        snprintf(line, sizeof(line), "%s   %d dBm   %s",
                 shown_ssid, (int)ap->rssi,
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
    label(s_ui.panel, "Wi-Fi settings", 14, 8, 190);
    s_ui.network_status = label(s_ui.panel, "Starting...", 211, 8, 415);
    s_ui.ip_address = label(s_ui.panel, "IP: not assigned", 211, 30, 415);
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
    s_ui.form_feedback = label(s_ui.form_page, "", 169, 119, 298);
    button(s_ui.form_page, "Save & connect", 483, 107, 196, 42, save_pressed);
    /* Full-screen-root overlay: 800x480 fixed pixel viewport.
     * The keyboard must not be a child of the hidden form_page, which
     * produced a clipped thin strip on real hardware in PR #36. */
    s_ui.keyboard = lv_keyboard_create(root);
    lv_obj_set_size(s_ui.keyboard, 724, 205);
    /* LVGL keyboards default to bottom-middle alignment. Make the anchor
     * explicitly top-left so (38,239) is a screen-space position. */
    lv_obj_align(s_ui.keyboard, LV_ALIGN_TOP_LEFT, 38, 239);
    lv_obj_set_style_pad_all(s_ui.keyboard, 3, 0);
    lv_obj_set_style_pad_row(s_ui.keyboard, 3, 0);
    lv_obj_set_style_pad_column(s_ui.keyboard, 3, 0);
    lv_keyboard_set_popovers(s_ui.keyboard, false);
    lv_keyboard_set_textarea(s_ui.keyboard, s_ui.password);
    lv_obj_add_flag(s_ui.keyboard, LV_OBJ_FLAG_HIDDEN);
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
    /* White settings gear, then the independent Wi-Fi status symbol on its RIGHT.
     * Only actual DHCP-IP states are blue, not a stale system clock. */
    lv_obj_t *open = button(root, LV_SYMBOL_SETTINGS, 671, 12, 54, 42, open_setup);
    lv_obj_set_style_bg_opa(open, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(open, 0, 0);
    lv_obj_t *gear = lv_obj_get_child(open, 0);
    lv_obj_set_style_text_color(gear, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(gear, &lv_font_montserrat_20, 0);
    lv_obj_set_style_bg_opa(gear, LV_OPA_TRANSP, 0);
    if (!touch_ready) lv_obj_add_state(open, LV_STATE_DISABLED);

    s_ui.wifi_indicator = label(root, LV_SYMBOL_WIFI, 737, 20, 39);
    lv_obj_set_style_text_font(s_ui.wifi_indicator, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_ui.wifi_indicator, lv_color_hex(0x87919F), 0);
    s_ui.connected_indicator = false;
}

void lp_wifi_ui_tick(void)
{
    /* The header symbol updates every second even while Settings is closed. */
    lp_net_state_t network = lp_network_state();
    bool connected = network == LP_NET_IP_READY ||
                     network == LP_NET_SYNCING || network == LP_NET_SYNCED;
    if (s_ui.wifi_indicator && connected != s_ui.connected_indicator) {
        s_ui.connected_indicator = connected;
        lv_obj_set_style_text_color(s_ui.wifi_indicator,
            lv_color_hex(connected ? 0x389AFF : 0x87919F), 0);
    }
    if (!s_ui.panel || lv_obj_has_flag(s_ui.panel, LV_OBJ_FLAG_HIDDEN)) return;
    const char *net = "C5 starting";
    switch (lp_network_state()) {
        case LP_NET_UNCONFIGURED: net = "Not configured"; break;
        case LP_NET_CONNECTING: net = "Connecting..."; break;
        case LP_NET_DISCONNECTED: net = "Disconnected"; break;
        case LP_NET_IP_READY: net = "Connected, IP acquired"; break;
        case LP_NET_SYNCING: net = "Connected, synchronizing time"; break;
        case LP_NET_SYNCED: net = "Connected, clock synced"; break;
        case LP_NET_ERROR: net = "Network error"; break;
        default: break;
    }
    lv_label_set_text(s_ui.network_status, net);
    char saved[LP_SSID_MAX + 1] = {0};
    lp_network_saved_ssid(saved, sizeof(saved));
    char shown_ssid[LP_SSID_MAX + 1];
    display_ssid(shown_ssid, sizeof(shown_ssid), saved);
    char line[72];
    snprintf(line, sizeof(line), "Saved: %s", saved[0] ? shown_ssid : "none");
    lv_label_set_text(s_ui.saved, line);

    char ipv4[16] = {0};
    lp_network_ipv4(ipv4, sizeof(ipv4));
    snprintf(line, sizeof(line), "IP: %s", ipv4[0] ? ipv4 : "not assigned");
    lv_label_set_text(s_ui.ip_address, line);

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
            lv_label_set_text(s_ui.message, connected ? "Saved." :
                              "Saved. Wi-Fi connecting automatically.");
        else if (save == LP_SAVE_FORGOTTEN)
            lv_label_set_text(s_ui.message, "Saved network forgotten.");
        else if (save == LP_SAVE_FAILED)
            lv_label_set_text(s_ui.message, "Save/forget failed. Check device log.");
    }
    /* Complete the success message on DHCP, but never overwrite subsequent
     * scan, forget, or error feedback with a stale save notification. */
    if (save == LP_SAVE_OK && connected &&
        strcmp(lv_label_get_text(s_ui.message),
               "Saved. Wi-Fi connecting automatically.") == 0) {
        lv_label_set_text(s_ui.message, "Saved.");
    }
}
