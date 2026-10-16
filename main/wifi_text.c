#include "wifi_text.h"

#include "credentials.h"
#include <stdint.h>
#include <string.h>

/* U+2018/U+2019 smart apostrophes are absent from the pinned Montserrat
 * subset. Substitute ASCII apostrophes only for rendering. Other SSID
 * bytes are preserved unchanged, including valid non-ASCII UTF-8. */
void lp_wifi_display_ssid(char *out, size_t capacity, const char *ssid)
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
