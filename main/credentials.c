#include "credentials.h"
#include <stdint.h>
#include <string.h>

static bool valid_text(const char *s, size_t max, size_t min)
{
    size_t n = strnlen(s, max + 1);
    if (n < min || n > max) return false;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c == 127) return false;
    }
    return true;
}

bool lp_credentials_valid(const lp_credentials_t *creds)
{
    return creds &&
        valid_text(creds->ssid, LP_SSID_MAX, 1) &&
        valid_text(creds->passphrase, LP_PASSPHRASE_MAX, 8);
}

/* Volatile byte stores prevent optimization from eliding a secret clear. */
void lp_secure_wipe(void *buf, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)buf;
    while (len--) *p++ = 0;
}
