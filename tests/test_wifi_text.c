#include "wifi_text.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char raw[] = "Fonda\xE2\x80\x99s House of Plastique";
    char before[sizeof(raw)];
    char rendered[33];
    memcpy(before, raw, sizeof(raw));

    lp_wifi_display_ssid(rendered, sizeof(rendered), raw);
    assert(strcmp(rendered, "Fonda's House of Plastique") == 0);
    assert(memcmp(raw, before, sizeof(raw)) == 0); /* Never mutate the SSID. */

    const char left_quote[] = "A\xE2\x80\x98B";
    lp_wifi_display_ssid(rendered, sizeof(rendered), left_quote);
    assert(strcmp(rendered, "A'B") == 0);

    lp_wifi_display_ssid(rendered, sizeof(rendered), "plain-ascii");
    assert(strcmp(rendered, "plain-ascii") == 0);

    char small[7] = {0};
    lp_wifi_display_ssid(small, sizeof(small), raw);
    assert(strcmp(small, "Fonda'") == 0);

    char sentinel[1] = {'x'};
    lp_wifi_display_ssid(sentinel, sizeof(sentinel), raw);
    assert(sentinel[0] == '\0');
    lp_wifi_display_ssid(sentinel, 0, raw);
    assert(sentinel[0] == '\0');

    lp_wifi_display_ssid(rendered, sizeof(rendered), NULL);
    assert(rendered[0] == '\0');

    puts("SSID label transformation: PASS");
    return 0;
}
