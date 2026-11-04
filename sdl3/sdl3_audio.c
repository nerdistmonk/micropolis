/* sdl3_audio.c: Audio subsystem for Micropolis SDL3.
 *
 * WAV playback via SDL3 AudioStreams with queue clearing and rate throttling.
 */

#include "sdl3_audio.h"
#include "sdl3_settings.h"
#include "sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static SDL_AudioStream *audio_stream = NULL;
static uint64_t last_tool_sound_time = 0;

typedef struct {
    char name[64];
    uint8_t *audio_buf;
    uint32_t audio_len;
    SDL_AudioSpec spec;
} SoundSample;

#define MAX_CACHED_SOUNDS 48
static SoundSample sound_cache[MAX_CACHED_SOUNDS];
static int num_cached = 0;

static FILE *FindSoundFile(const char *stem, char *out_path, size_t max_len)
{
    const char *dirs[] = {
        "sounds/",
        "../sounds/",
        "../../sounds/"
    };

    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        snprintf(out_path, max_len, "%s%s.wav", dirs[i], stem);
        FILE *f = fopen(out_path, "rb");
        if (f) return f;
    }
    return NULL;
}

static SoundSample *LoadWavFile(const char *stem)
{
    char path[512];
    FILE *f = FindSoundFile(stem, path, sizeof(path));
    if (!f) return NULL;
    fclose(f);

    SDL_AudioSpec spec;
    uint8_t *buf = NULL;
    uint32_t len = 0;

    if (!SDL_LoadWAV(path, &spec, &buf, &len)) {
        return NULL;
    }

    SoundSample *s = &sound_cache[num_cached++];
    snprintf(s->name, sizeof(s->name), "%s", stem);
    s->audio_buf = buf;
    s->audio_len = len;
    s->spec = spec;

    return s;
}

static SoundSample *GetOrLoadSound(const char *stem)
{
    for (int i = 0; i < num_cached; i++) {
        if (SDL_strcasecmp(sound_cache[i].name, stem) == 0) {
            return &sound_cache[i];
        }
    }
    if (num_cached >= MAX_CACHED_SOUNDS) return NULL;

    return LoadWavFile(stem);
}

typedef struct {
    const char *name;
    const char *stem;
} SoundAlias;

/* Unix sound names (res/sound.tcl) and core shorthand mapped to WAV stems in sounds/ */
static const SoundAlias sound_aliases[] = {
    { "Explosion-High", "explosion-hi" },
    { "Explosion-Low", "explosion-low" },
    { "HonkHonk-High", "honkhonk-hi" },
    { "HonkHonk-Low", "honkhonk-low" },
    { "HonkHonk-Med", "honkhonk-med" },
    { "HeavyTraffic", "heavytraffic" },
    { "Bulldozer", "bulldozer" },
    { "traffic", "heavytraffic" },
    { "expl-hi", "explosion-hi" },
    { "expl-low", "explosion-low" },
};

static bool ContainsNoCase(const char *haystack, const char *needle)
{
    return SDL_strcasestr(haystack, needle) != NULL;
}

static void PlayPcmData(const uint8_t *data, uint32_t len, const SDL_AudioSpec *spec)
{
    if (!audio_stream || !data || len == 0 || !spec) return;

    /* Prevent stream stacking backlog */
    SDL_ClearAudioStream(audio_stream);

    SDL_SetAudioStreamFormat(audio_stream, spec, NULL);
    SDL_PutAudioStreamData(audio_stream, data, (int)len);
    SDL_FlushAudioStream(audio_stream);
}

static void PlaySyntheticTone(int freq_hz, int duration_ms)
{
    if (!audio_stream) return;

    const int rate = 22050;
    int total_samples = (rate * duration_ms) / 1000;
    int16_t *buf = (int16_t *)malloc((size_t)total_samples * sizeof(int16_t));
    if (!buf) return;

    for (int i = 0; i < total_samples; i++) {
        float t = (float)i / (float)rate;
        float decay = 1.0f - ((float)i / (float)total_samples);
        buf[i] = (int16_t)(sinf(2.0f * 3.14159f * (float)freq_hz * t) * 16000.0f * decay);
    }

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 1;
    spec.freq = rate;

    PlayPcmData((const uint8_t *)buf, (uint32_t)(total_samples * sizeof(int16_t)), &spec);
    free(buf);
}

bool Audio_Init(void)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Failed to initialize SDL3 Audio: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec target_spec;
    target_spec.format = SDL_AUDIO_S16LE;
    target_spec.channels = 2;
    target_spec.freq = 44100;

    audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &target_spec,
        NULL,
        NULL
    );

    if (!audio_stream) {
        SDL_Log("Failed to create default audio stream: %s", SDL_GetError());
        return false;
    }

    Audio_ApplySoundLevel();
    SDL_ResumeAudioStreamDevice(audio_stream);
    return true;
}

void Audio_ApplySoundLevel(void)
{
    if (audio_stream) {
        SDL_SetAudioStreamGain(audio_stream, Settings_GetSoundGain());
    }
}

void Audio_Shutdown(void)
{
    if (audio_stream) {
        SDL_DestroyAudioStream(audio_stream);
        audio_stream = NULL;
    }

    for (int i = 0; i < num_cached; i++) {
        if (sound_cache[i].audio_buf) {
            free(sound_cache[i].audio_buf);
            sound_cache[i].audio_buf = NULL;
        }
    }
    num_cached = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void Audio_MakeSound(const char *channel, const char *sound_name)
{
    (void)channel;
    if (!sound_name || !audio_stream || !UserSoundOn) return;

    /* Throttle repetitive drag-tool audio to prevent rapid fire stutter */
    uint64_t now = SDL_GetTicks();
    bool is_tool_sound = (ContainsNoCase(sound_name, "bulldoze") ||
                          ContainsNoCase(sound_name, "build") ||
                          ContainsNoCase(sound_name, "road") ||
                          ContainsNoCase(sound_name, "wire"));

    if (is_tool_sound && (now - last_tool_sound_time < 90)) {
        return;
    }
    if (is_tool_sound) {
        last_tool_sound_time = now;
    }

    char base_name[64];
    int32_t speed = 100;
    const char *speed_tag = strstr(sound_name, " -speed ");
    if (speed_tag) {
        speed = atoi(speed_tag + 8);
        if (speed <= 0) speed = 100;
        snprintf(base_name, sizeof(base_name), "%.*s", (int)(speed_tag - sound_name), sound_name);
    } else {
        snprintf(base_name, sizeof(base_name), "%s", sound_name);
    }
    for (char *p = base_name; *p; p++) *p = (char)tolower((unsigned char)*p);

    const char *stem = base_name;
    for (size_t i = 0; i < sizeof(sound_aliases) / sizeof(sound_aliases[0]); i++) {
        if (SDL_strcasecmp(base_name, sound_aliases[i].name) == 0) {
            stem = sound_aliases[i].stem;
            break;
        }
    }

    SoundSample *s = GetOrLoadSound(stem);
    if (s && s->audio_buf) {
        SDL_AudioSpec spec = s->spec;
        if (speed != 100) spec.freq = (int)((int64_t)spec.freq * speed / 100);
        PlayPcmData(s->audio_buf, s->audio_len, &spec);
    } else {
        if (ContainsNoCase(sound_name, "expl") || ContainsNoCase(sound_name, "bulldoze")) {
            PlaySyntheticTone(100, 150);
        } else if (ContainsNoCase(sound_name, "siren") || ContainsNoCase(sound_name, "police")) {
            PlaySyntheticTone(880, 250);
        } else {
            PlaySyntheticTone(440, 80);
        }
    }
}

void Audio_PlayRunningSound(int16_t speed)
{
    char name[32];
    int32_t pitch = (speed > 0) ? 100 + 10 * speed : 90;
    snprintf(name, sizeof(name), "boing -speed %d", pitch);
    Audio_MakeSound("edit", name);
}

void Audio_PlaySound(int16_t sound_id, int16_t x, int16_t y)
{
    (void)x;
    (void)y;
    switch (sound_id) {
        case 1: Audio_MakeSound("city", "police"); break;
        case 2: Audio_MakeSound("city", "traffic"); break;
        case 3: Audio_MakeSound("city", "monster"); break;
        case 4: Audio_MakeSound("city", "expl-low"); break;
        case 5: Audio_MakeSound("city", "expl-hi"); break;
        default:
            Audio_MakeSound("city", "beep");
            break;
    }
}