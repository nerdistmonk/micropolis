/* sdl3_screenshot.h: F12 screenshots of the presented window, saved as PNG files.
 */

#ifndef SDL3_SCREENSHOT_H
#define SDL3_SCREENSHOT_H

#include <SDL3/SDL.h>

void Screenshot_Request(void);
void Screenshot_CaptureIfRequested(SDL_Renderer *renderer);

#endif /* SDL3_SCREENSHOT_H */
