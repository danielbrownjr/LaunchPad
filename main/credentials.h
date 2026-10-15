#pragma once
#include <stdbool.h>
#include <stddef.h>
#define LP_SSID_MAX 32
#define LP_PASSPHRASE_MAX 63
typedef struct {
    char ssid[LP_SSID_MAX + 1];
    char passphrase[LP_PASSPHRASE_MAX + 1];
} lp_credentials_t;
bool lp_credentials_valid(const lp_credentials_t *creds);
void lp_secure_wipe(void *buf, size_t len);
