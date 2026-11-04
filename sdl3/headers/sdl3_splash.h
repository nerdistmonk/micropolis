/* sdl3_splash.h: Startup splash screen shown before the first city view.
 */

#ifndef SDL3_SPLASH_H
#define SDL3_SPLASH_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

void Splash_Start(void);
bool Splash_IsActive(void);
void Splash_Update(uint64_t now_ms);
void Splash_Dismiss(void);
void Splash_Render(SDL_Renderer *renderer);

#endif /* SDL3_SPLASH_H */
