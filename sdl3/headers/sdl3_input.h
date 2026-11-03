/* sdl3_input.h: SDL3 event polling, mouse tools, and camera controls.
 */

#ifndef SDL3_INPUT_H
#define SDL3_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

void Input_Init(void);
void Input_ProcessEvents(bool *running);
int16_t Input_GetActiveTool(void);
void Input_SetActiveTool(int16_t tool);

#endif /* SDL3_INPUT_H */