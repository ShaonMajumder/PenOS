#ifndef DRIVERS_SPEAKER_H
#define DRIVERS_SPEAKER_H

#include <stdint.h>

// PC Speaker functions
void speaker_init(void);
void speaker_play(uint32_t frequency);
void speaker_stop(void);
void speaker_beep(uint32_t frequency, uint32_t duration_ms);
void speaker_startup_sound(void);

#endif
