#ifndef ALARM_H
#define ALARM_H
#include <time.h>
#define MAX_ALARMS 128
typedef struct { int hour, minute, days, duration; time_t last_fired; } Alarm;
int alarm_due(Alarm *alarm, time_t now);
int alarm_hour(int hour, int twelve_hour, int pm);
int alarm_valid(const Alarm *alarm);
#endif
