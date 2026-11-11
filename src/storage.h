#ifndef STORAGE_H
#define STORAGE_H
#include "alarm.h"
int alarms_load(const char *path, Alarm alarms[MAX_ALARMS], int *count);
int alarms_save(const char *path, const Alarm *alarms, int count);
int make_directory(const char *path);
#endif
