/* sdl3_settings.c: Loads and saves frontend preferences in a key=value file under the SDL pref path.
 */

#include "sdl3_settings.h"
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

#define SETTINGS_FILE_NAME "settings.cfg"

static bool edge_scroll_enabled = false;
static SoundLevel sound_level = SOUND_LEVEL_FULL;
static char settings_path[512] = {0};

static const float sound_level_gains[SOUND_LEVEL_COUNT] = { 0.25f, 0.5f, 0.75f, 1.0f };
static const char *sound_level_names[SOUND_LEVEL_COUNT] = { "LOW", "MEDIUM", "HIGH", "FULL" };

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
    fclose(f);
}

void Settings_Load(void)
{
    char *pref_dir = SDL_GetPrefPath("Micropolis", "micropolis_sdl3");
    if (pref_dir) {
        snprintf(settings_path, sizeof(settings_path), "%s%s", pref_dir, SETTINGS_FILE_NAME);
        SDL_free(pref_dir);
    }

    if (settings_path[0] == '\0') return;

    FILE *f = fopen(settings_path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int value = 0;
        if (sscanf(line, "edge_auto_scroll=%d", &value) == 1) {
            edge_scroll_enabled = (value != 0);
        } else if (sscanf(line, "sound_level=%d", &value) == 1) {
            if (value >= 0 && value < (int)SOUND_LEVEL_COUNT) {
                sound_level = (SoundLevel)value;
            }
        }
    }
    fclose(f);
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
