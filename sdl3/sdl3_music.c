/* sdl3_music.c: Background music from OGG files in the music/ directory.
 *
 * Playback uses SDL3_mixer on its own device, separate from the WAV effect
 * voices in sdl3_audio.c. Track end is detected by polling in Music_Update().
 */

#include "sdl3_music.h"
#include "sdl3_settings.h"
#include "sdl3_paths.h"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MUSIC_MAX_TRACKS 256
#define MUSIC_NAME_LEN 256
#define MUSIC_PATH_LEN 768

static MIX_Mixer *mixer = NULL;
static MIX_Track *track = NULL;
static MIX_Audio *audio = NULL;
static char music_dir[MUSIC_PATH_LEN] = "";
static char track_names[MUSIC_MAX_TRACKS][MUSIC_NAME_LEN];
static int track_count = 0;
static int current_track = -1;
static bool stalled = false;
static char now_playing[MUSIC_NAME_LEN] = "";

static int CompareTrackNames(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static bool IsOggFile(const char *name)
{
    size_t len = strlen(name);
    if (len < 4) return false;
    const char *ext = name + len - 4;
    return ext[0] == '.' &&
           tolower((unsigned char)ext[1]) == 'o' &&
           tolower((unsigned char)ext[2]) == 'g' &&
           tolower((unsigned char)ext[3]) == 'g';
}

static bool ScanDirectory(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return false;

    track_count = 0;
    struct dirent *ent;
    while (track_count < MUSIC_MAX_TRACKS && (ent = readdir(d)) != NULL) {
        if (IsOggFile(ent->d_name)) {
            snprintf(track_names[track_count], MUSIC_NAME_LEN, "%s", ent->d_name);
            track_count++;
        }
    }
    closedir(d);

    qsort(track_names, (size_t)track_count, MUSIC_NAME_LEN, CompareTrackNames);
    snprintf(music_dir, sizeof(music_dir), "%s", dir);
    return true;
}

static void FindTracks(void)
{
    char dir[MUSIC_PATH_LEN];
    if (Res_Path(dir, sizeof(dir), "music/") && ScanDirectory(dir) && track_count > 0) return;
    track_count = 0;
    music_dir[0] = '\0';
}

static int PickNextTrack(void)
{
    if (track_count == 1) return 0;

    if (Settings_GetMusicOrder() == MUSIC_ORDER_RANDOM) {
        if (current_track < 0) return rand() % track_count;
        /* Pick from the other tracks so the same song never repeats back to back. */
        int r = rand() % (track_count - 1);
        if (r >= current_track) r++;
        return r;
    }

    return (current_track + 1) % track_count;
}

static void SetNowPlaying(const char *name)
{
    snprintf(now_playing, sizeof(now_playing), "%s", name);
    size_t len = strlen(now_playing);
    if (len > 4) now_playing[len - 4] = '\0';
}

static bool LoadTrack(int index)
{
    char path[MUSIC_PATH_LEN];
    snprintf(path, sizeof(path), "%s%s", music_dir, track_names[index]);

    MIX_Audio *next = MIX_LoadAudio(mixer, path, false);
    if (!next) {
        SDL_Log("Music: could not load %s: %s", path, SDL_GetError());
        return false;
    }

    MIX_StopTrack(track, 0);
    MIX_SetTrackAudio(track, next);
    if (audio) MIX_DestroyAudio(audio);
    audio = next;

    MIX_SetTrackGain(track, Settings_GetMusicGain());
    return MIX_PlayTrack(track, 0);
}

static void PlayNextTrack(void)
{
    for (int attempt = 0; attempt < track_count; attempt++) {
        int index = PickNextTrack();
        current_track = index;
        if (LoadTrack(index)) {
            SetNowPlaying(track_names[index]);
            return;
        }
    }

    /* Every track failed to load; stop retrying until the user skips. */
    stalled = true;
    now_playing[0] = '\0';
}

bool Music_Init(void)
{
    FindTracks();
    if (track_count == 0) {
        SDL_Log("Music: no .ogg files found in the music/ directory");
        return false;
    }

    if (!MIX_Init()) {
        SDL_Log("Music: MIX_Init failed: %s", SDL_GetError());
        return false;
    }

    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!mixer) {
        SDL_Log("Music: could not open audio device: %s", SDL_GetError());
        MIX_Quit();
        return false;
    }

    track = MIX_CreateTrack(mixer);
    if (!track) {
        SDL_Log("Music: could not create track: %s", SDL_GetError());
        MIX_DestroyMixer(mixer);
        mixer = NULL;
        MIX_Quit();
        return false;
    }

    srand((unsigned)time(NULL));
    PlayNextTrack();
    return true;
}

void Music_Shutdown(void)
{
    if (track) {
        MIX_StopTrack(track, 0);
        MIX_DestroyTrack(track);
        track = NULL;
    }
    if (audio) {
        MIX_DestroyAudio(audio);
        audio = NULL;
    }
    if (mixer) {
        MIX_DestroyMixer(mixer);
        mixer = NULL;
        MIX_Quit();
    }
    current_track = -1;
    stalled = false;
    now_playing[0] = '\0';
}

void Music_Update(void)
{
    if (!track || stalled) return;

    MIX_SetTrackGain(track, Settings_GetMusicGain());
    if (!MIX_TrackPlaying(track)) {
        PlayNextTrack();
    }
}

void Music_Skip(void)
{
    if (!track) return;

    stalled = false;
    MIX_StopTrack(track, 0);
    PlayNextTrack();
}

int Music_GetTrackCount(void)
{
    return track_count;
}

bool Music_IsAvailable(void)
{
    return mixer != NULL;
}

const char *Music_GetNowPlaying(void)
{
    return now_playing;
}
