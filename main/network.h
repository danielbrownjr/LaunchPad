#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "credentials.h"

typedef enum {
    LP_NET_OFF = 0,
    LP_NET_STARTING,
    LP_NET_UNCONFIGURED,
    LP_NET_CONNECTING,
    LP_NET_DISCONNECTED,
    LP_NET_IP_READY,
    LP_NET_SYNCING,
    LP_NET_SYNCED,
    LP_NET_ERROR,
} lp_net_state_t;

#define LP_NET_SCAN_MAX 12

typedef struct {
    char ssid[LP_SSID_MAX + 1];
    int8_t rssi;
    bool personal_wpa; /* Open/enterprise/WEP cannot be provisioned here. */
} lp_net_ap_t;

typedef struct {
    size_t count;
    lp_net_ap_t aps[LP_NET_SCAN_MAX];
} lp_net_scan_t;

typedef enum {
    LP_SCAN_IDLE = 0,
    LP_SCAN_RUNNING,
    LP_SCAN_READY,
    LP_SCAN_FAILED,
} lp_net_scan_status_t;

typedef enum {
    LP_SAVE_IDLE = 0,
    LP_SAVE_PENDING,
    LP_SAVE_OK,
    LP_SAVE_FAILED,
    LP_SAVE_FORGOTTEN,
} lp_net_save_status_t;

esp_err_t lp_network_start(void);
lp_net_state_t lp_network_state(void);
bool lp_network_clock_valid(void);
bool lp_network_clock_stale(void);

/* Non-blocking, bounded cross-task commands. Never call driver/NVS from LVGL.
 * False = not ready, unsupported credentials or another action pending. */
bool lp_network_request_scan(void);
bool lp_network_submit_credentials(const lp_credentials_t *creds);
bool lp_network_forget_saved(void);

/* Snapshot getters, safe on the LVGL task. Never expose a saved password. */
lp_net_scan_status_t lp_network_scan_status(void);
void lp_network_scan_copy(lp_net_scan_t *out);
lp_net_save_status_t lp_network_save_status(void);
void lp_network_saved_ssid(char *out, size_t len);
