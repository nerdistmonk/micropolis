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

bool Settings_GetTileAnimation(void);
void Settings_ToggleTileAnimation(void);
bool Settings_GetStatusMessages(void);
void Settings_ToggleStatusMessages(void);
bool Settings_GetPictureNotices(void);
void Settings_TogglePictureNotices(void);

bool Settings_GetLinearFilter(void);
void Settings_ToggleLinearFilter(void);

SoundLevel Settings_GetSoundLevel(void);
void Settings_CycleSoundLevel(void);
float Settings_GetSoundGain(void);
const char *Settings_GetSoundLevelName(void);

typedef enum {
    MUSIC_VOLUME_OFF,
    MUSIC_VOLUME_LOW,
    MUSIC_VOLUME_MEDIUM,
    MUSIC_VOLUME_HIGH,
    MUSIC_VOLUME_COUNT
} MusicVolume;

typedef enum {
    MUSIC_ORDER_SEQUENTIAL,
    MUSIC_ORDER_RANDOM,
    MUSIC_ORDER_COUNT
} MusicOrder;

MusicVolume Settings_GetMusicVolume(void);
void Settings_CycleMusicVolume(void);
float Settings_GetMusicGain(void);
const char *Settings_GetMusicVolumeName(void);

MusicOrder Settings_GetMusicOrder(void);
void Settings_ToggleMusicOrder(void);
const char *Settings_GetMusicOrderName(void);

#endif /* SDL3_SETTINGS_H */
