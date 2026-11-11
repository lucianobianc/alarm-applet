#ifndef SOUND_H
#define SOUND_H
#include <sys/types.h>
int sound_play(const char *device, unsigned seconds, unsigned frequency);
pid_t sound_start(unsigned seconds, unsigned frequency);
void sound_stop(pid_t *pid);
#endif
