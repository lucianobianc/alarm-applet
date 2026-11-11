#include "alarm.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(alarm_hour(12,1,0)==0); assert(alarm_hour(12,1,1)==12);
    assert(alarm_hour(1,1,1)==13); assert(alarm_hour(0,1,0)==-1);
    assert(alarm_hour(23,0,0)==23); assert(alarm_hour(24,0,0)==-1);
    struct tm t = {.tm_year=126,.tm_mon=10,.tm_mday=8,.tm_hour=9,.tm_min=30,.tm_isdst=-1};
    time_t now=mktime(&t); /* Sunday */
    Alarm a={9,30,1,5,1000,0};
    assert(alarm_valid(&a)); assert(alarm_due(&a,now));
    assert(!alarm_due(&a,now+1)); assert(!alarm_due(&a,now+60));
    assert(!alarm_due(&a,now+86400)); assert(alarm_due(&a,now+7*86400));
    a.days=64; a.last_fired=0; assert(alarm_due(&a,now+6*86400));
    a.days=0; assert(!alarm_valid(&a)); a.days=1;
    a.duration=0; assert(!alarm_valid(&a)); a.duration=3601; assert(!alarm_valid(&a));
    a.duration=5; a.frequency=19; assert(!alarm_valid(&a));
    a.frequency=20001; assert(!alarm_valid(&a)); a.frequency=1000; assert(alarm_valid(&a));
    puts("Alarm scheduling and input validation passed.");
}
