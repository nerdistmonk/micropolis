/* sdl3_gui.h: HUD, tool palette, dialog windows, overlays, and tooltip subsystem.
 */

#ifndef SDL3_GUI_H
#define SDL3_GUI_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>
#include "sdl3_gui_dialogs.h"
#include "sdl3_help.h"

#define STATUS_BAR_HEIGHT 28
#define TOOLBAR_WIDTH 108
/* Screen pixel where the camera's tile (cam_x, cam_y) is drawn; the map lies right of the toolbar and below the status bar */
#define MAP_ORIGIN_X TOOLBAR_WIDTH
#define MAP_ORIGIN_Y STATUS_BAR_HEIGHT

bool GUI_Init(void);
void GUI_Shutdown(void);
void GUI_Render(SDL_Renderer *renderer);

/* Event handling hooks */
bool GUI_HandleClick(float mouse_x, float mouse_y, uint8_t button);
void GUI_HandleMotion(float mouse_x, float mouse_y);
bool GUI_HandleKeyDown(SDL_Keycode key);
bool GUI_HandleTextInput(const char *text);

/* Help topic under a window point, for context help mode */
HelpTopic GUI_HelpTopicAt(float mouse_x, float mouse_y);

/* Dialog control functions */
bool GUI_IsModalOpen(void);
void GUI_OpenCityMgmtModal(CityMgmtTab initial_tab);
void GUI_OpenOverlaysModal(void);
void GUI_OpenFileModal(FileDialogTab initial_tab);
void GUI_ShowStatus(const char *text);
bool GUI_LoadScenarioWithStatus(int16_t id);
int32_t GUI_CityPopDisplay(void);
void GUI_OpenDisastersModal(void);
void GUI_CloseModal(void);

/* Overlay mode query */
OverlayViewMode GUI_GetActiveOverlayMode(void);
void GUI_SetActiveOverlayMode(OverlayViewMode mode);

/* Helper to render simple text onto SDL_Renderer */
void GUI_DrawText(SDL_Renderer *renderer, int x, int y, const char *text, SDL_Color color);

#endif /* SDL3_GUI_H */