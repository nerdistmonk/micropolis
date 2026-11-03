/* sdl3_gui_dialogs.h: Modal windows and persistent overlay layer dialogs.
 */

#ifndef SDL3_GUI_DIALOGS_H
#define SDL3_GUI_DIALOGS_H

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef enum {
  GUI_MODAL_NONE = 0,
  GUI_MODAL_CITY_MGMT,
  GUI_MODAL_OVERLAYS,
  GUI_MODAL_SCENARIO_RESULT,
  GUI_MODAL_FILE,
  GUI_MODAL_DISASTERS,
  GUI_MODAL_QUERY_INSPECTOR
} GuiModalId;

typedef enum {
  CITY_TAB_BUDGET = 0,
  CITY_TAB_TAXES,
  CITY_TAB_EVALUATION,
  CITY_TAB_GRAPHS,
  CITY_TAB_CITY_INFO,
  CITY_TAB_COUNT
} CityMgmtTab;

typedef enum {
  FILE_TAB_SAVE_LOAD = 0,
  FILE_TAB_SCENARIOS,
  FILE_TAB_OPTIONS,
  FILE_TAB_COUNT
} FileDialogTab;

typedef enum {
  OVERLAY_VIEW_NORMAL = 0,
  OVERLAY_VIEW_POWER,
  OVERLAY_VIEW_TRAFFIC,
  OVERLAY_VIEW_POLLUTION,
  OVERLAY_VIEW_CRIME,
  OVERLAY_VIEW_LAND_VALUE,
  OVERLAY_VIEW_POP_DENSITY
} OverlayViewMode;

bool GUI_Dialogs_IsModalOpen(void);
void GUI_Dialogs_OpenCityMgmt(CityMgmtTab initial_tab);
void GUI_Dialogs_OpenOverlays(void);
void GUI_Dialogs_OpenFile(FileDialogTab initial_tab);
void GUI_Dialogs_OpenDisasters(void);
void GUI_Dialogs_ShowScenarioResult(bool won, const char *msg);
void GUI_Dialogs_ShowQueryInspector(int32_t tile_x, int32_t tile_y);
void GUI_Dialogs_Close(void);

OverlayViewMode GUI_Dialogs_GetActiveOverlay(void);
void GUI_Dialogs_SetActiveOverlay(OverlayViewMode mode);

bool GUI_Dialogs_HandleClick(float mx, float my);
bool GUI_Dialogs_HandleKeyDown(SDL_Keycode key);
bool GUI_Dialogs_HandleTextInput(const char *text);

void GUI_Dialogs_Render(SDL_Renderer *renderer);

#endif /* SDL3_GUI_DIALOGS_H */