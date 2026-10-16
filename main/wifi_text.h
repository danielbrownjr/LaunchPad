#pragma once

#include <stddef.h>

/* UI-only display fallback for apostrophes missing from the current font.
 * Never use the result for Wi-Fi association or persistent credentials. */
void lp_wifi_display_ssid(char *out, size_t capacity, const char *ssid);
