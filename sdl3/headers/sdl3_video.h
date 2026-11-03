/* sdl3_video.h: SDL3 video, windowing, tile rendering, and minimap subsystem.
 */

#ifndef SDL3_VIDEO_H
#define SDL3_VIDEO_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

#define WINDOW_DEFAULT_WIDTH 1024
#define WINDOW_DEFAULT_HEIGHT 768
#define TILE_PIXEL_SIZE 16

bool Video_Init(int width, int height);
void Video_Shutdown(void);
void Video_RenderFrame(void);

void Video_SetCamera(int32_t tile_x, int32_t tile_y);
void Video_GetCamera(int32_t *tile_x, int32_t *tile_y);
void Video_PanCamera(int32_t delta_x, int32_t delta_y);
void Video_CenterOnTile(int32_t tile_x, int32_t tile_y);
bool Video_ScreenToTile(float screen_x, float screen_y, int32_t *tile_x, int32_t *tile_y);

/* Window event callback for resizing */
void Video_HandleWindowEvent(const SDL_Event *event);

/* Minimap query and interaction */
SDL_FRect Video_GetMinimapRect(void);
bool Video_HandleMinimapClick(float screen_x, float screen_y);

SDL_Renderer *Video_GetRenderer(void);
SDL_Window *Video_GetWindow(void);

/* Helper to load arbitrary image/XPM files into textures */
SDL_Texture *Video_LoadTexture(const char *path, int *out_w, int *out_h);

#endif /* SDL3_VIDEO_H */