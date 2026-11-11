#include "sound.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void ignore_signal(int number) { (void)number; }
static void short_pause(void) { struct timespec t={0,10000000}; nanosleep(&t,NULL); }
static void lifecycle(void) {
    /* The app installs a signal handler; the audio worker must reset it. */
    signal(SIGTERM,ignore_signal);
    pid_t worker=sound_start(3600,1000); assert(worker>0);
    short_pause(); pid_t stopped=worker;
    sound_stop(&worker); assert(worker==0);
    assert(waitpid(stopped,NULL,WNOHANG)<0 && errno==ECHILD);
    /* Adopt the audio worker to verify that it dies when its parent exits. */
    assert(prctl(PR_SET_CHILD_SUBREAPER,1)==0);
    int descriptors[2]; assert(pipe(descriptors)==0);
    pid_t parent=fork(); assert(parent>=0);
    if(!parent) {
        close(descriptors[0]);
        pid_t audio=sound_start(3600,1000);
        if(write(descriptors[1],&audio,sizeof audio)!=(ssize_t)sizeof audio) _exit(1);
        short_pause(); _exit(0);
    }
    close(descriptors[1]); pid_t audio=0;
    assert(read(descriptors[0],&audio,sizeof audio)==(ssize_t)sizeof audio); close(descriptors[0]);
    assert(audio>0); int status;
    assert(waitpid(parent,&status,0)==parent && WIFEXITED(status) && !WEXITSTATUS(status));
    assert(waitpid(audio,&status,0)==audio);
    assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGTERM);
    puts("Audio stop, reaping and parent-exit cleanup passed.");
}
int main(int argc,char **argv) {
    if(argc>1 && !strcmp(argv[1],"--lifecycle")) {lifecycle();return 0;}
    unsigned frequency=argc>1?(unsigned)strtoul(argv[1],NULL,10):1000;
    return sound_play("default",1,frequency);
}
