/* sdl3_splash.c: Startup splash screen showing the Micropolis logo for a few seconds.
 */

#include "sdl3_splash.h"
#include "sdl3_video.h"
#include <stdio.h>

#define SPLASH_DURATION_MS 3000

static SDL_Texture *splash_texture = NULL;
static int splash_w = 0;
static int splash_h = 0;
static bool splash_active = false;
static uint64_t splash_start_ms = 0;

void Splash_Start(void)
{
    const char *prefixes[] = { "", "images/", "../images/", "../../images/" };
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]) && !splash_texture; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%ssplash.xpm", prefixes[i]);
        splash_texture = Video_LoadTexture(path, &splash_w, &splash_h);
    }

    splash_active = (splash_texture != NULL);
    splash_start_ms = SDL_GetTicks();
}

bool Splash_IsActive(void)
{
    return splash_active;
}

void Splash_Update(uint64_t now_ms)
{
    if (splash_active && now_ms - splash_start_ms >= SPLASH_DURATION_MS) {
        Splash_Dismiss();
    }
}

void Splash_Dismiss(void)
{
    if (!splash_active) return;
    splash_active = false;
    if (splash_texture) {
        SDL_DestroyTexture(splash_texture);
        splash_texture = NULL;
    }
}

void Splash_Render(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (!splash_texture) return;

    SDL_FRect dst = {
        ((float)Video_GetCanvasWidth() - (float)splash_w) / 2.0f,
        ((float)Video_GetCanvasHeight() - (float)splash_h) / 2.0f,
        (float)splash_w,
        (float)splash_h
    };
    SDL_RenderTexture(renderer, splash_texture, NULL, &dst);
}
