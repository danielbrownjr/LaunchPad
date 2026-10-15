#pragma once
#include <stdbool.h>
#include "esp_err.h"

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

esp_err_t lp_network_start(void);
lp_net_state_t lp_network_state(void);
bool lp_network_clock_valid(void);
bool lp_network_clock_stale(void);
