/* sdl3_paths.h: Resolve bundled asset paths relative to the executable.
 *
 * The working directory is arbitrary (desktop launchers, AppImage), so all
 * assets are looked up next to the binary, where the build copies them.
 */

#ifndef SDL3_PATHS_H
#define SDL3_PATHS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>

static inline bool Res_Path(char *out, size_t len, const char *rel)
{
    const char *base = SDL_GetBasePath();
    int n = snprintf(out, len, "%s%s", base ? base : "", rel);
    return n >= 0 && (size_t)n < len;
}

#endif /* SDL3_PATHS_H */
