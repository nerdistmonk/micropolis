/* sdl3_gui_text.h: TTF typography and font rendering subsystem.
 */

#ifndef SDL3_GUI_TEXT_H
#define SDL3_GUI_TEXT_H

#include <stdbool.h>
#include <SDL3/SDL.h>

bool GUI_Text_Init(void);
void GUI_Text_Shutdown(void);

int GUI_GetTextWidth(const char *text);
void GUI_DrawText(SDL_Renderer *renderer, int x, int y, const char *text, SDL_Color color);

#endif /* SDL3_GUI_TEXT_H */