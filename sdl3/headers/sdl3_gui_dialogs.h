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
    GUI_MODAL_QUERY_INSPECTOR,
    GUI_MODAL_QUIT_CONFIRM,
    GUI_MODAL_CHOOSE_CITY,
    GUI_MODAL_EDITOR
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
    FILE_TAB_OPTIONS,
    FILE_TAB_ABOUT,
    FILE_TAB_JUKEBOX,
    FILE_TAB_COUNT
} FileDialogTab;

typedef enum {
    OVERLAY_VIEW_NORMAL = 0,
    OVERLAY_VIEW_POWER,
    OVERLAY_VIEW_TRAFFIC,
    OVERLAY_VIEW_POLLUTION,
    OVERLAY_VIEW_CRIME,
    OVERLAY_VIEW_LAND_VALUE,
    OVERLAY_VIEW_POP_DENSITY,
    OVERLAY_VIEW_RATE_OF_GROWTH,
    OVERLAY_VIEW_FIRE_COVERAGE,
    OVERLAY_VIEW_POLICE_COVERAGE,
    OVERLAY_VIEW_RES_ZONE,
    OVERLAY_VIEW_COM_ZONE,
    OVERLAY_VIEW_IND_ZONE,
    OVERLAY_VIEW_TRANSPORT
} OverlayViewMode;

bool GUI_Dialogs_IsModalOpen(void);
void GUI_Dialogs_OpenCityMgmt(CityMgmtTab initial_tab);
void GUI_Dialogs_OpenAnnualBudget(void);
void GUI_Dialogs_OpenOverlays(void);
void GUI_Dialogs_OpenFile(FileDialogTab initial_tab);
void GUI_Dialogs_OpenDisasters(void);
void GUI_Dialogs_ShowScenarioResult(bool won, const char *msg);
void GUI_Dialogs_ShowQueryInspector(int32_t tile_x, int32_t tile_y);
void GUI_Dialogs_ShowNotice(int16_t picture_id, int32_t tile_x, int32_t tile_y);
bool GUI_Dialogs_HandleNoticeKey(SDL_Keycode key);
void GUI_Dialogs_UpdateNotice(uint64_t now);
void GUI_Dialogs_OpenQuitConfirm(void);
bool GUI_Dialogs_TakeQuitConfirmed(void);
void GUI_Dialogs_OpenChooseCity(void (*on_confirm)(int32_t), int32_t arg);
void GUI_Dialogs_OpenConfirm(const char *title, const char *line1, const char *line2, const char *confirm_label,
                             void (*on_confirm)(int32_t), int32_t arg);
void GUI_Dialogs_OpenEditor(void);
void GUI_Dialogs_Close(void);

/* Scenario editor palette actions */
void GUI_Editor_Save(void);
void GUI_Editor_Step(void);
void GUI_Editor_Exit(void);
void GUI_Editor_Random(void);

OverlayViewMode GUI_Dialogs_GetActiveOverlay(void);
void GUI_Dialogs_SetActiveOverlay(OverlayViewMode mode);

bool GUI_Dialogs_HandleClick(float mx, float my);
bool GUI_Dialogs_HandleKeyDown(SDL_Keycode key);
bool GUI_Dialogs_HandleTextInput(const char *text);

void GUI_Dialogs_Render(SDL_Renderer *renderer);

#endif /* SDL3_GUI_DIALOGS_H */