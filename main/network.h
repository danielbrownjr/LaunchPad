#pragma once

#include "esp_err.h"

typedef enum {
    LP_NET_OFF = 0,
    LP_NET_STARTING,
    LP_NET_UNCONFIGURED,
    LP_NET_ERROR,
} lp_net_state_t;

/* Start the C5/SDIO bring-up in a separate FreeRTOS task after LCD is visible.
 * This slice does NOT join an AP, store credentials or synchronize the clock. */
esp_err_t lp_network_start(void);
lp_net_state_t lp_network_state(void);
