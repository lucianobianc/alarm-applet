#include "storage.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int make_directory(const char *path) {
    if(!path || !*path) {errno=EINVAL;return -1;}
    char *copy=strdup(path);
    if(!copy) return -1;
    for(char *p=copy+1;;p++) {
        if(*p!='/' && *p) continue;
        char separator=*p; *p=0;
        if(mkdir(copy,0700)<0) {
            struct stat status;
            if(errno!=EEXIST || stat(copy,&status)<0 || !S_ISDIR(status.st_mode)) {
                free(copy); return -1;
            }
        }
        *p=separator;
        if(!separator) break;
    }
    free(copy); return 0;
}

int alarms_load(const char *path, Alarm alarms[MAX_ALARMS], int *count) {
    FILE *file=fopen(path,"r");
    if(!file) { if(errno==ENOENT) { *count=0; return 0; } return -1; }
    Alarm loaded[MAX_ALARMS]; int length=0,ok=1;
    char line[512];
    while(fgets(line,sizeof line,file)) {
        char *cursor=line,*end; int fields=0; intmax_t values[6]={0,0,0,0,1000,0};
        while(isspace((unsigned char)*cursor)) cursor++;
        if(!*cursor) continue;
        while(*cursor && fields<6) {
            errno=0; intmax_t value=strtoimax(cursor,&end,10);
            if(cursor==end || errno || (*end && !isspace((unsigned char)*end))) { ok=0; break; }
            values[fields++]=value; cursor=end;
            while(isspace((unsigned char)*cursor)) cursor++;
        }
        if(!ok || *cursor || fields<4 || length==MAX_ALARMS) {ok=0;break;}
        for(int i=0;i<5;i++) if(values[i]<0 || values[i]>INT_MAX) ok=0;
        if(!ok || values[5]<0 || (intmax_t)(time_t)values[5]!=values[5]) {ok=0;break;}
        Alarm alarm={(int)values[0],(int)values[1],(int)values[2],(int)values[3],(int)values[4],(time_t)values[5]};
        if(!alarm_valid(&alarm)) {ok=0;break;}
        loaded[length++]=alarm;
    }
    if(ferror(file)) ok=0;
    if(fclose(file)) ok=0;
    if(!ok) {errno=EINVAL;return -1;}
    memcpy(alarms,loaded,(size_t)length*sizeof *alarms); *count=length;
    return 0;
}

int alarms_save(const char *path, const Alarm *alarms, int count) {
    size_t size=strlen(path)+16; char *temporary=malloc(size);
    if(!temporary) return -1;
    snprintf(temporary,size,"%s.XXXXXX",path);
    int fd=mkstemp(temporary);
    if(fd<0) {free(temporary);return -1;}
    FILE *file=fdopen(fd,"w");
    if(!file) {close(fd);unlink(temporary);free(temporary);return -1;}
    int ok=1;
    for(int i=0;i<count;i++) {
        const Alarm *a=&alarms[i];
        if(fprintf(file,"%d %d %d %d %d %jd\n",a->hour,a->minute,a->days,a->duration,a->frequency,(intmax_t)a->last_fired)<0) ok=0;
    }
    if(fflush(file) || fsync(fd)) ok=0;
    if(fclose(file)) ok=0;
    if(ok && rename(temporary,path)) ok=0;
    if(!ok) unlink(temporary);
    free(temporary); return ok?0:-1;
}
