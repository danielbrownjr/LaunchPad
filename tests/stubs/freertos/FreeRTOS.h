#pragma once

#include <stdint.h>

/* Minimal test-only subset. Firmware always uses ESP-IDF's actual headers. */
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY UINT32_MAX
