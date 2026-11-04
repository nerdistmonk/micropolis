/* sdl3_settings.h: Frontend preferences persisted outside the city save.
 */

#ifndef SDL3_SETTINGS_H
#define SDL3_SETTINGS_H

#include <stdbool.h>

typedef enum {
    SOUND_LEVEL_LOW,
    SOUND_LEVEL_MEDIUM,
    SOUND_LEVEL_HIGH,
    SOUND_LEVEL_FULL,
    SOUND_LEVEL_COUNT
} SoundLevel;

void Settings_Load(void);

bool Settings_GetEdgeScroll(void);
void Settings_ToggleEdgeScroll(void);

SoundLevel Settings_GetSoundLevel(void);
void Settings_CycleSoundLevel(void);
float Settings_GetSoundGain(void);
const char *Settings_GetSoundLevelName(void);

#endif /* SDL3_SETTINGS_H */
