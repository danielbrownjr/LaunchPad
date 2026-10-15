#define _POSIX_C_SOURCE 200809L
#include "clock_util.h"
#include <stdlib.h>
#include <time.h>

void lp_clock_timezone_init(void)
{
    (void)setenv("TZ", LP_TIMEZONE, 1);
    tzset();
}

bool lp_clock_format(time_t when, char *buf, size_t cap)
{
    struct tm local;
    if (!buf || cap < 9 || when < LP_CLOCK_MIN_EPOCH) return false;
    if (localtime_r(&when, &local) == NULL) return false;
    return strftime(buf, cap, "%I:%M %p", &local) > 0;
}
