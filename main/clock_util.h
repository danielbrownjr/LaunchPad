#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/* US Eastern, with rules for daylight-saving changes in March/November. */
#define LP_TIMEZONE "EST5EDT,M3.2.0/2,M11.1.0/2"
#define LP_CLOCK_MIN_EPOCH ((time_t)1704067200) /* 2024-01-01 UTC */
void lp_clock_timezone_init(void);
bool lp_clock_format(time_t when, char *buf, size_t cap);
