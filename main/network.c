/*
 * P4-hosted C5 networking and local serial provisioning.
 * Credentials are P4-NVS-only; the C5 Wi-Fi driver's storage stays RAM-only.
 * Never call LVGL or set time from a network callback without genuine SNTP.
 */
#include "network.h"
#include "credentials.h"
#include "clock_util.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

extern void esp_hosted_init(void);

#define NET_IP          BIT0
#define NET_DISCONNECT  BIT1
#define NET_NEW_CONFIG  BIT2
#define WIFI_NS         "lpwifi"
#define WIFI_KEY        "sta"

static const char *TAG = "lp_network";
static EventGroupHandle_t s_events;
static atomic_int s_state = ATOMIC_VAR_INIT(LP_NET_OFF);
static atomic_bool s_synced = ATOMIC_VAR_INIT(false);
static atomic_uint s_last_sync_tick = ATOMIC_VAR_INIT(0);
static bool s_sntp_started;

lp_net_state_t lp_network_state(void)
{
    return (lp_net_state_t)atomic_load(&s_state);
}

bool lp_network_clock_valid(void)
{
    return atomic_load(&s_synced) && time(NULL) >= LP_CLOCK_MIN_EPOCH;
}

bool lp_network_clock_stale(void)
{
    if (!lp_network_clock_valid()) return false;
    uint32_t elapsed = (uint32_t)xTaskGetTickCount() - atomic_load(&s_last_sync_tick);
    return elapsed > pdMS_TO_TICKS(24U * 60U * 60U * 1000U);
}

static void set_state(lp_net_state_t state)
{
    atomic_store(&s_state, state);
}

static void fail(const char *step, esp_err_t err)
{
    ESP_LOGE(TAG, "%s failed (%s)", step, esp_err_to_name(err));
    set_state(LP_NET_ERROR);
}

static esp_err_t load_credentials(lp_credentials_t *out, bool *found)
{
    *found = false;
    lp_secure_wipe(out, sizeof(*out));
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;

    size_t bytes = sizeof(*out);
    err = nvs_get_blob(h, WIFI_KEY, out, &bytes);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err == ESP_ERR_NVS_INVALID_LENGTH) {
        ESP_LOGW(TAG, "Outdated/invalid saved Wi-Fi credential size");
        return ESP_OK;
    }
    if (err != ESP_OK) return err;
    if (bytes != sizeof(*out) || !lp_credentials_valid(out)) {
        ESP_LOGW(TAG, "Saved Wi-Fi credentials invalid; serial setup required");
        lp_secure_wipe(out, sizeof(*out));
        return ESP_OK;
    }
    *found = true;
    return ESP_OK;
}

static esp_err_t save_credentials(const lp_credentials_t *creds)
{
    if (!lp_credentials_valid(creds)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, WIFI_KEY, creds, sizeof(*creds));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* No console line editor or command history. Disable host terminal local
 * echo and recording before sending a passphrase. Drain overlong lines. */
static bool serial_line(char *out, size_t cap)
{
    size_t used = 0;
    bool overflow = false;
    for (;;) {
        int ch = getchar();
        if (ch == EOF) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (used == 0 && !overflow) continue; /* Drain CRLF remainder. */
            break;
        }
        if (used + 1 < cap) out[used++] = (char)ch;
        else overflow = true;
    }
    out[used] = '\0';
    if (overflow) lp_secure_wipe(out, cap);
    return !overflow;
}

static void serial_task(void *arg)
{
    (void)arg;
    setvbuf(stdin, NULL, _IONBF, 0);
    puts("LaunchPad dev Wi-Fi: type 'wifi' on J10 USB serial.");
    puts("Disable local echo/session logging before typing a passphrase.");
    for (;;) {
        char command[16] = {0};
        if (!serial_line(command, sizeof(command))) {
            puts("Unknown or overlong command");
            continue;
        }
        if (strcmp(command, "wifi") != 0) {
            puts("Type 'wifi' to configure WPA station.");
            continue;
        }

        lp_credentials_t creds = {0};
        puts("SSID, then Enter (1..32 bytes):");
        bool ssid_ok = serial_line(creds.ssid, sizeof(creds.ssid));
        puts("Passphrase, then Enter (8..63 bytes; no firmware echo):");
        bool pass_ok = serial_line(creds.passphrase, sizeof(creds.passphrase));

        if (ssid_ok && pass_ok && lp_credentials_valid(&creds)) {
            esp_err_t err = save_credentials(&creds);
            if (err == ESP_OK) {
                puts("Saved to P4 NVS; connecting.");
                xEventGroupSetBits(s_events, NET_NEW_CONFIG);
            } else {
                ESP_LOGE(TAG, "Credential NVS write failed: %s", esp_err_to_name(err));
            }
        } else {
            puts("Invalid credentials; nothing saved.");
        }
        lp_secure_wipe(&creds, sizeof(creds));
        lp_secure_wipe(command, sizeof(command));
    }
}

static void time_sync_cb(struct timeval *timeval)
{
    /* SNTP callback is the trust boundary, not an epoch plausibility guess. */
    if (!timeval || timeval->tv_sec < LP_CLOCK_MIN_EPOCH) return;
    atomic_store(&s_last_sync_tick, (uint32_t)xTaskGetTickCount());
    atomic_store(&s_synced, true);
    lp_net_state_t state = lp_network_state();
    if (state == LP_NET_SYNCING || state == LP_NET_IP_READY) {
        set_state(LP_NET_SYNCED);
    }
    ESP_LOGI(TAG, "SNTP UTC synchronization confirmed");
}

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        set_state(LP_NET_IP_READY);
        xEventGroupSetBits(s_events, NET_IP);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        set_state(LP_NET_DISCONNECTED);
        xEventGroupSetBits(s_events, NET_DISCONNECT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        set_state(LP_NET_DISCONNECTED);
        xEventGroupSetBits(s_events, NET_DISCONNECT);
    }
}

static esp_err_t wifi_set_credentials(const lp_credentials_t *creds)
{
    wifi_config_t cfg = {0};
    memcpy(cfg.sta.ssid, creds->ssid, strlen(creds->ssid));
    memcpy(cfg.sta.password, creds->passphrase, strlen(creds->passphrase));
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    lp_secure_wipe(&cfg, sizeof(cfg));
    return err;
}

static esp_err_t sync_start(void)
{
    if (!s_sntp_started) {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        cfg.sync_cb = time_sync_cb;
        esp_err_t err = esp_netif_sntp_init(&cfg);
        if (err == ESP_OK) s_sntp_started = true;
        return err;
    }
    return esp_netif_sntp_start();
}

static void network_task(void *arg)
{
    (void)arg;
    s_events = xEventGroupCreate();
    if (!s_events) {
        fail("event group allocation", ESP_ERR_NO_MEM);
        goto done;
    }
    lp_clock_timezone_init();
    /* Never erase NVS automatically, even if old/corrupt/full. */
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) { fail("P4 NVS init", err); goto done; }
    err = esp_netif_init();
    if (err != ESP_OK) { fail("esp_netif_init", err); goto done; }
    err = esp_event_loop_create_default();
    if (err != ESP_OK) { fail("event loop", err); goto done; }
    err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                     event_handler, NULL);
    if (err != ESP_OK) { fail("Wi-Fi event handler", err); goto done; }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     event_handler, NULL);
    if (err != ESP_OK) { fail("IP event handler", err); goto done; }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP,
                                     event_handler, NULL);
    if (err != ESP_OK) { fail("IP-loss event handler", err); goto done; }

    /* GPIO28 is a switched C5 power rail, controlled by ESP-Hosted. */
    ESP_LOGI(TAG, "Starting ESP-Hosted C5 2.12.3, SDIO slot 1");
    esp_hosted_init();
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    if (!sta) { fail("Wi-Fi STA netif", ESP_ERR_NO_MEM); goto done; }

    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) { fail("remote Wi-Fi init", err); goto done; }
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM); /* No C5 flash writes. */
    if (err != ESP_OK) { fail("remote Wi-Fi RAM storage", err); goto done; }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) { fail("remote station mode", err); goto done; }
    err = esp_wifi_start();
    if (err != ESP_OK) { fail("remote station start", err); goto done; }

    if (xTaskCreate(serial_task, "lp_wifi_setup", 4096, NULL, 3, NULL) != pdPASS) {
        fail("serial provisioning task", ESP_ERR_NO_MEM);
        goto done;
    }

    unsigned attempts = 0;
    for (;;) {
        lp_credentials_t creds;
        bool found = false;
        err = load_credentials(&creds, &found);
        if (err != ESP_OK) {
            lp_secure_wipe(&creds, sizeof(creds));
            fail("read P4 NVS", err);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        if (!found) {
            set_state(LP_NET_UNCONFIGURED);
            ESP_LOGI(TAG, "No configured Wi-Fi; waiting for serial 'wifi'");
            xEventGroupWaitBits(s_events, NET_NEW_CONFIG, pdTRUE,
                                pdFALSE, portMAX_DELAY);
            continue;
        }

        err = wifi_set_credentials(&creds);
        lp_secure_wipe(&creds, sizeof(creds));
        if (err != ESP_OK) {
            fail("Wi-Fi config RPC", err);
        } else {
            set_state(LP_NET_CONNECTING);
            xEventGroupClearBits(s_events, NET_IP | NET_DISCONNECT);
            err = esp_wifi_connect();
            if (err != ESP_OK) {
                fail("Wi-Fi connect RPC", err);
            } else {
                EventBits_t flags = xEventGroupWaitBits(
                    s_events, NET_IP | NET_DISCONNECT | NET_NEW_CONFIG,
                    pdTRUE, pdFALSE, pdMS_TO_TICKS(20000));
                if (flags & NET_NEW_CONFIG) {
                    (void)esp_wifi_disconnect();
                    attempts = 0;
                    continue;
                }
                if (flags & NET_IP) {
                    attempts = 0;
                    ESP_LOGI(TAG, "DHCP IP acquired; starting SNTP");
                    set_state(LP_NET_SYNCING);
                    err = sync_start();
                    if (err != ESP_OK) {
                        fail("SNTP start", err);
                        (void)esp_wifi_disconnect();
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        continue;
                    }
                    for (;;) {
                        EventBits_t f = xEventGroupWaitBits(
                            s_events, NET_DISCONNECT | NET_NEW_CONFIG,
                            pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
                        if (f & NET_NEW_CONFIG) {
                            (void)esp_wifi_disconnect();
                            attempts = 0;
                            break;
                        }
                        if (f & NET_DISCONNECT) break;
                        if (err == ESP_OK) {
                            set_state(lp_network_clock_valid() && !lp_network_clock_stale()
                                      ? LP_NET_SYNCED : LP_NET_SYNCING);
                        }
                    }
                    continue;
                }
                /* The station can associate without obtaining DHCP. Disconnect
                 * before retrying rather than connecting atop stale state. */
                (void)esp_wifi_disconnect();
            }
        }
        set_state(LP_NET_DISCONNECTED);
        unsigned pause = 1U << (attempts < 5U ? attempts : 5U);
        if (attempts < 5U) ++attempts;
        ESP_LOGW(TAG, "Wi-Fi unavailable, retrying in %u seconds", pause);
        (void)xEventGroupWaitBits(s_events, NET_NEW_CONFIG, pdTRUE,
                                  pdFALSE, pdMS_TO_TICKS(pause * 1000U));
    }

done:
    vTaskDelete(NULL);
}

esp_err_t lp_network_start(void)
{
    if (lp_network_state() != LP_NET_OFF) return ESP_ERR_INVALID_STATE;
    set_state(LP_NET_STARTING);
    if (xTaskCreate(network_task, "lp_c5", 8192, NULL, 4, NULL) != pdPASS) {
        set_state(LP_NET_ERROR);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
