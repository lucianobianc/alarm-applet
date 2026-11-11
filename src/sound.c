#include "sound.h"
#include <alsa/asoundlib.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int sound_play(const char *device, unsigned seconds) {
    snd_pcm_t *pcm=NULL;
    int result=snd_pcm_open(&pcm,device,SND_PCM_STREAM_PLAYBACK,0);
    if(result<0) { fprintf(stderr,"Audio: %s\n",snd_strerror(result)); return 1; }
    result=snd_pcm_set_params(pcm,SND_PCM_FORMAT_S16_LE,SND_PCM_ACCESS_RW_INTERLEAVED,1,48000,1,50000);
    if(result<0) { fprintf(stderr,"Audio: %s\n",snd_strerror(result)); snd_pcm_close(pcm); return 1; }
    const unsigned rate=48000;
    unsigned long total=(unsigned long)seconds*rate;
    short samples[480];
    for(unsigned long offset=0;offset<total;offset+=480) {
        unsigned frames=total-offset<480?(unsigned)(total-offset):480;
        for(unsigned i=0;i<frames;i++) {
            unsigned long position=offset+i;
            unsigned phase=(unsigned)(position%(rate/2));
            double envelope=0;
            if(phase<rate/4) {
                envelope=1;
                if(phase<240) envelope=phase/240.0;
                if(phase>rate/4-240) envelope=(rate/4-phase)/240.0;
            }
            samples[i]=(short)(6000*envelope*sin(2*3.141592653589793*880*position/rate));
        }
        unsigned written=0;
        while(written<frames) {
            snd_pcm_sframes_t n=snd_pcm_writei(pcm,samples+written,frames-written);
            if(n<0) { if(snd_pcm_recover(pcm,(int)n,1)<0) {snd_pcm_close(pcm);return 1;} }
            else written+=(unsigned)n;
        }
    }
    result=snd_pcm_drain(pcm); snd_pcm_close(pcm); return result<0;
}
pid_t sound_start(unsigned seconds) {
    pid_t child=fork();
    if(child==0) _exit(sound_play("default",seconds));
    return child;
}
void sound_stop(pid_t *pid) {
    if(*pid>0) { kill(*pid,SIGTERM); while(waitpid(*pid,NULL,0)<0 && errno==EINTR) {} }
    *pid=0;
}
