/*
 * LaunchPad ESP32-P4 / ESP32-C5 bring-up, explicitly C5 only.
 * The host uses Espressif ESP-Hosted 2.12.3 via the V1.3 four-bit SDIO bus.
 * No vendor code is copied. No credentials, SNTP or automatic C5 flashing.
 */
#include "network.h"

#include <stdatomic.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Public ESP-Hosted 2.x initialization function, as called by the vendor
 * sample. Its void return does not prove a completed C5 handshake. The
 * subsequent remote Wi-Fi RPC result determines if this stage is ready. */
extern void esp_hosted_init(void);

static const char *TAG = "lp_network";
static atomic_int s_state = ATOMIC_VAR_INIT(LP_NET_OFF);

lp_net_state_t lp_network_state(void)
{
    return (lp_net_state_t)atomic_load(&s_state);
}

static void fail(const char *what, esp_err_t err)
{
    ESP_LOGE(TAG, "%s failed: %s; clock remains unsynchronized",
             what, esp_err_to_name(err));
    atomic_store(&s_state, LP_NET_ERROR);
}

static void network_task(void *arg)
{
    (void)arg;
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        fail("esp_netif_init", err);
        goto done;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        fail("esp_event_loop_create_default", err);
        goto done;
    }

    /* ESP-Hosted owns GPIO28 power cycling through the reset configuration.
     * Never drive the C5 load switch directly while SDIO is active. */
    ESP_LOGI(TAG, "Starting ESP-Hosted C5 2.12.3 on SDIO slot 1");
    esp_hosted_init();

    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    if (sta == NULL) {
        fail("esp_netif_create_default_wifi_sta", ESP_ERR_NO_MEM);
        goto done;
    }

    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        fail("remote esp_wifi_init / C5 handshake", err);
        goto done;
    }
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        fail("esp_wifi_set_storage", err);
        goto done;
    }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        fail("esp_wifi_set_mode", err);
        goto done;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        fail("remote esp_wifi_start", err);
        goto done;
    }

    /* The radio/remote Wi-Fi RPC succeeded. This is NOT an IP connection.
     * Provisioning will be added after the actual C5 firmware is identified. */
    ESP_LOGI(TAG, "C5 remote Wi-Fi started; no AP credentials configured; no IP");
    atomic_store(&s_state, LP_NET_UNCONFIGURED);

done:
    vTaskDelete(NULL);
}

esp_err_t lp_network_start(void)
{
    if (lp_network_state() != LP_NET_OFF) {
        return ESP_ERR_INVALID_STATE;
    }
    atomic_store(&s_state, LP_NET_STARTING);
    if (xTaskCreate(network_task, "lp_c5", 8192, NULL, 4, NULL) != pdPASS) {
        atomic_store(&s_state, LP_NET_ERROR);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
