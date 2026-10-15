#include "credentials.h"
#include "clock_util.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
    lp_credentials_t c = {0};
    assert(!lp_credentials_valid(&c));
    strcpy(c.ssid, "Example AP");
    strcpy(c.passphrase, "1234567");
    assert(!lp_credentials_valid(&c));
    strcpy(c.passphrase, "12345678");
    assert(lp_credentials_valid(&c));
    memset(c.ssid, 'x', 32); c.ssid[32] = 0;
    assert(lp_credentials_valid(&c));
    memset(c.ssid, 'x', 33);
    assert(!lp_credentials_valid(&c));
    strcpy(c.ssid, "Example AP");
    strcpy(c.passphrase, "12345678");
    c.passphrase[0] = '\n';
    assert(!lp_credentials_valid(&c));
    lp_secure_wipe(&c, sizeof(c));
    for (size_t i = 0; i < sizeof(c); ++i) assert(((char *)&c)[i] == 0);
    lp_clock_timezone_init();
    char buf[20];
    assert(!lp_clock_format(0, buf, sizeof(buf)));
    assert(!lp_clock_format(LP_CLOCK_MIN_EPOCH, buf, 2));
    struct tm winter = {.tm_year=126, .tm_mon=0, .tm_mday=15, .tm_hour=12, .tm_isdst=-1};
    time_t w = mktime(&winter);
    assert(winter.tm_isdst == 0);
    assert(lp_clock_format(w, buf, sizeof(buf)));
    assert(strcmp(buf, "12:00 PM") == 0);
    struct tm summer = {.tm_year=126, .tm_mon=6, .tm_mday=15, .tm_hour=12, .tm_isdst=-1};
    time_t s = mktime(&summer);
    assert(summer.tm_isdst == 1);
    assert(lp_clock_format(s, buf, sizeof(buf)));
    assert(strcmp(buf, "12:00 PM") == 0);
    puts("credential bounds, secret wipe, Eastern TZ and DST: PASS");
    return 0;
}
