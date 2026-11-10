/* sdl3_settings.c: Loads and saves frontend preferences in a key=value file under the SDL pref path.
 */

#include "sdl3_settings.h"
#include "sim.h"
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

#define SETTINGS_FILE_NAME "settings.cfg"

static bool edge_scroll_enabled = false;
static bool tile_animation_enabled = true;
static bool status_messages_enabled = true;
static bool linear_filter_enabled = true;
static bool autumn_color_enabled = true;
static SoundLevel sound_level = SOUND_LEVEL_FULL;
static MusicVolume music_volume = MUSIC_VOLUME_MEDIUM;
static MusicOrder music_order = MUSIC_ORDER_SEQUENTIAL;
static char settings_path[512] = {0};

static const float sound_level_gains[SOUND_LEVEL_COUNT] = { 0.25f, 0.5f, 0.75f, 1.0f };
static const char *sound_level_names[SOUND_LEVEL_COUNT] = { "LOW", "MEDIUM", "HIGH", "FULL" };

/* Music is scaled well below the SFX levels so it sits under the effects at every setting. */
static const float music_volume_gains[MUSIC_VOLUME_COUNT] = { 0.0f, 0.08f, 0.16f, 0.3f };
static const char *music_volume_names[MUSIC_VOLUME_COUNT] = { "OFF", "LOW", "MEDIUM", "HIGH" };

static void ApplySimFlags(void)
{
    DoAnimation = tile_animation_enabled ? 1 : 0;
    DoMessages = status_messages_enabled ? 1 : 0;
}

static void Settings_Save(void)
{
    if (settings_path[0] == '\0') return;

    FILE *f = fopen(settings_path, "w");
    if (!f) {
        SDL_Log("Warning: could not save settings to %s", settings_path);
        return;
    }
    fprintf(f, "edge_auto_scroll=%d\n", edge_scroll_enabled ? 1 : 0);
    fprintf(f, "sound_level=%d\n", (int)sound_level);
    fprintf(f, "tile_animation=%d\n", tile_animation_enabled ? 1 : 0);
    fprintf(f, "status_messages=%d\n", status_messages_enabled ? 1 : 0);
    fprintf(f, "linear_filter=%d\n", linear_filter_enabled ? 1 : 0);
    fprintf(f, "autumn_color=%d\n", autumn_color_enabled ? 1 : 0);
    fprintf(f, "music_volume=%d\n", (int)music_volume);
    fprintf(f, "music_order=%d\n", (int)music_order);
    fclose(f);
}

void Settings_Load(void)
{
    char *pref_dir = SDL_GetPrefPath("Micropolis", "micropolis_sdl3");
    if (pref_dir) {
        snprintf(settings_path, sizeof(settings_path), "%s%s", pref_dir, SETTINGS_FILE_NAME);
        SDL_free(pref_dir);
    }

    if (settings_path[0] == '\0') {
        ApplySimFlags();
        return;
    }

    FILE *f = fopen(settings_path, "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            int value = 0;
            if (sscanf(line, "edge_auto_scroll=%d", &value) == 1) {
                edge_scroll_enabled = (value != 0);
            } else if (sscanf(line, "sound_level=%d", &value) == 1) {
                if (value >= 0 && value < (int)SOUND_LEVEL_COUNT) {
                    sound_level = (SoundLevel)value;
                }
            } else if (sscanf(line, "tile_animation=%d", &value) == 1) {
                tile_animation_enabled = (value != 0);
            } else if (sscanf(line, "status_messages=%d", &value) == 1) {
                status_messages_enabled = (value != 0);
            } else if (sscanf(line, "linear_filter=%d", &value) == 1) {
                linear_filter_enabled = (value != 0);
            } else if (sscanf(line, "autumn_color=%d", &value) == 1) {
                autumn_color_enabled = (value != 0);
            } else if (sscanf(line, "music_volume=%d", &value) == 1) {
                if (value >= 0 && value < (int)MUSIC_VOLUME_COUNT) {
                    music_volume = (MusicVolume)value;
                }
            } else if (sscanf(line, "music_order=%d", &value) == 1) {
                if (value >= 0 && value < (int)MUSIC_ORDER_COUNT) {
                    music_order = (MusicOrder)value;
                }
            }
        }
        fclose(f);
    }

    ApplySimFlags();
}

bool Settings_GetEdgeScroll(void)
{
    return edge_scroll_enabled;
}

void Settings_ToggleEdgeScroll(void)
{
    edge_scroll_enabled = !edge_scroll_enabled;
    Settings_Save();
}

bool Settings_GetTileAnimation(void)
{
    return tile_animation_enabled;
}

void Settings_ToggleTileAnimation(void)
{
    tile_animation_enabled = !tile_animation_enabled;
    ApplySimFlags();
    Settings_Save();
}

bool Settings_GetStatusMessages(void)
{
    return status_messages_enabled;
}

void Settings_ToggleStatusMessages(void)
{
    status_messages_enabled = !status_messages_enabled;
    ApplySimFlags();
    Settings_Save();
}

bool Settings_GetLinearFilter(void)
{
    return linear_filter_enabled;
}

void Settings_ToggleLinearFilter(void)
{
    linear_filter_enabled = !linear_filter_enabled;
    Settings_Save();
}

bool Settings_GetAutumnColor(void)
{
    return autumn_color_enabled;
}

void Settings_ToggleAutumnColor(void)
{
    autumn_color_enabled = !autumn_color_enabled;
    Settings_Save();
}

SoundLevel Settings_GetSoundLevel(void)
{
    return sound_level;
}

void Settings_CycleSoundLevel(void)
{
    sound_level = (SoundLevel)(((int)sound_level + 1) % (int)SOUND_LEVEL_COUNT);
    Settings_Save();
}

float Settings_GetSoundGain(void)
{
    return sound_level_gains[sound_level];
}

const char *Settings_GetSoundLevelName(void)
{
    return sound_level_names[sound_level];
}

void Settings_CycleMusicVolume(void)
{
    music_volume = (MusicVolume)(((int)music_volume + 1) % (int)MUSIC_VOLUME_COUNT);
    Settings_Save();
}

float Settings_GetMusicGain(void)
{
    return music_volume_gains[music_volume];
}

const char *Settings_GetMusicVolumeName(void)
{
    return music_volume_names[music_volume];
}

MusicOrder Settings_GetMusicOrder(void)
{
    return music_order;
}

void Settings_ToggleMusicOrder(void)
{
    music_order = (music_order == MUSIC_ORDER_SEQUENTIAL) ? MUSIC_ORDER_RANDOM : MUSIC_ORDER_SEQUENTIAL;
    Settings_Save();
}

const char *Settings_GetMusicOrderName(void)
{
    return music_order == MUSIC_ORDER_RANDOM ? "Random" : "Sequential";
}
