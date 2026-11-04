/* sdl3_audio.h: Audio subsystem and sound effects loader for Micropolis SDL3.
 */

#ifndef SDL3_AUDIO_H
#define SDL3_AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

bool Audio_Init(void);
void Audio_Shutdown(void);
void Audio_ApplySoundLevel(void);

/* Sound callback hooks registered with core engine */
void Audio_PlaySound(int16_t sound_id, int16_t x, int16_t y);
void Audio_MakeSound(const char *channel, const char *sound_name);

#endif /* SDL3_AUDIO_H */