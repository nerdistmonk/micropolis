/* sdl3_gui_text.h: TTF typography and font rendering subsystem.
 */

#ifndef SDL3_GUI_TEXT_H
#define SDL3_GUI_TEXT_H

#include <stdbool.h>
#include <SDL3/SDL.h>

bool GUI_Text_Init(void);
void GUI_Text_Shutdown(void);

int GUI_GetTextWidth(const char *text);
int GUI_GetLineHeight(void);
float GUI_ButtonWidth(const char *label, float pad);
void GUI_DrawText(SDL_Renderer *renderer, int x, int y, const char *text, SDL_Color color);
void GUI_DrawTextCentered(SDL_Renderer *renderer, SDL_FRect rect, const char *text, SDL_Color color);

#endif /* SDL3_GUI_TEXT_H */