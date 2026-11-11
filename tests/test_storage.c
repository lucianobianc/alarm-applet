#include "storage.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_file(const char *path,const char *text) {
    FILE *file=fopen(path,"w"); assert(file); assert(fputs(text,file)>=0); assert(fclose(file)==0);
}
int main(void) {
    char directory[]="/tmp/alarm-storage-XXXXXX"; assert(mkdtemp(directory));
    char nested[256],path[256];
    snprintf(nested,sizeof nested,"%s/a/b",directory);
    assert(make_directory(nested)==0); assert(make_directory(nested)==0);
    snprintf(path,sizeof path,"%s/a/b/alarms",directory);
    Alarm alarms[MAX_ALARMS]; int count=-1;
    assert(alarms_load(path,alarms,&count)==0 && count==0);
    write_file(path,"9 30 127 5\n12 0 1 8 1500\n");
    assert(alarms_load(path,alarms,&count)==0 && count==2);
    assert(alarms[0].frequency==1000 && alarms[1].frequency==1500);
    alarms[0].last_fired=1794130200;
    assert(alarms_save(path,alarms,count)==0);
    Alarm restored[MAX_ALARMS]; int restored_count=0;
    assert(alarms_load(path,restored,&restored_count)==0 && restored_count==2);
    assert(restored[0].last_fired==alarms[0].last_fired);
    const char *bad[]={"9 0 0 5 1000\n","9 0 127 5 1000 garbage\n","9 0 127 5 999999999999999999999999999\n","9 0 127 5 1000 -1\n","9 0 127 5 1000 0 7\n"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++) {
        write_file(path,bad[i]); count=2; alarms[0].hour=17;
        assert(alarms_load(path,alarms,&count)<0);
        assert(count==2 && alarms[0].hour==17);
    }
    assert(alarms_save(directory,restored,restored_count)<0); /* Failed rename cannot erase the destination. */
    unlink(path); rmdir(nested);
    snprintf(nested,sizeof nested,"%s/a",directory); rmdir(nested); rmdir(directory);
    puts("Storage: legacy records, round trip, last firing, malformed input and safe writes passed.");
}
