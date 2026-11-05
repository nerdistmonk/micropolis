/* sdl3_music.h: Background OGG music played from the music/ directory.
 */

#ifndef SDL3_MUSIC_H
#define SDL3_MUSIC_H

#include <stdbool.h>

bool Music_Init(void);
void Music_Shutdown(void);
void Music_Update(void);
void Music_Skip(void);

int Music_GetTrackCount(void);
const char *Music_GetNowPlaying(void);

#endif /* SDL3_MUSIC_H */
