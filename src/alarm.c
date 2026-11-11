#include "alarm.h"
int alarm_hour(int h, int twelve, int pm) {
    if (twelve) return h < 1 || h > 12 ? -1 : h % 12 + (pm ? 12 : 0);
    return h < 0 || h > 23 ? -1 : h;
}
int alarm_valid(const Alarm *a) {
    return a->hour >= 0 && a->hour < 24 && a->minute >= 0 && a->minute < 60 &&
        a->days > 0 && a->days < 128 && a->duration >= 1 && a->duration <= 3600 && a->frequency >= 20 && a->frequency <= 20000;
}
int alarm_due(Alarm *a, time_t now) {
    struct tm local;
    if(!localtime_r(&now, &local)) return 0;
    /* Only fire at the scheduled minute, never catch up missed alarms. */
    if (!alarm_valid(a) || !(a->days & (1 << local.tm_wday)) ||
        a->hour != local.tm_hour || a->minute != local.tm_min) return 0;
    if (a->last_fired) {
        struct tm previous;
        if (localtime_r(&a->last_fired, &previous) && previous.tm_year == local.tm_year && previous.tm_yday == local.tm_yday) return 0;
    }
    a->last_fired = now;
    return 1;
}
