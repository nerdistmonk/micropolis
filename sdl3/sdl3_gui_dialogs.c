/* sdl3_gui_dialogs.c: Modal windows orchestrator and router.
 */

#include "sdl3_dialog_common.h"
#include "scenario.h"

static GuiModalId active_modal = GUI_MODAL_NONE;
static OverlayViewMode active_overlay_mode = OVERLAY_VIEW_NORMAL;
static bool pending_annual_budget = false;
static int16_t saved_sim_speed = 3;

bool GUI_Dialogs_IsModalOpen(void)
{
    return (active_modal != GUI_MODAL_NONE);
}

void GUI_Dialogs_OpenCityMgmt(CityMgmtTab initial_tab)
{
    active_modal = GUI_MODAL_CITY_MGMT;
    Dialog_City_Open(initial_tab);
}

void GUI_Dialogs_OpenAnnualBudget(void)
{
    active_modal = GUI_MODAL_CITY_MGMT;
    Dialog_City_Open(CITY_TAB_BUDGET);
    pending_annual_budget = true;
    saved_sim_speed = (SimSpeed > 0) ? SimSpeed : 3;
    SetSimSpeed(0);
    Audio_PlayRunningSound(SimSpeed);
}

void GUI_Dialogs_OpenOverlays(void)
{
    active_modal = GUI_MODAL_OVERLAYS;
    Dialog_Overlays_Open();
}

void GUI_Dialogs_OpenFile(FileDialogTab initial_tab)
{
    active_modal = GUI_MODAL_FILE;
    Dialog_File_Open(initial_tab);
}

void GUI_Dialogs_OpenDisasters(void)
{
    active_modal = GUI_MODAL_DISASTERS;
    Dialog_Disasters_Open();
}

void GUI_Dialogs_OpenNews(void)
{
    active_modal = GUI_MODAL_NEWS;
    Dialog_News_Open();
}

void GUI_Dialogs_ShowScenarioResult(bool won, const char *msg)
{
    active_modal = GUI_MODAL_SCENARIO_RESULT;
    Dialog_Scenario_Show(won, msg);
}

void GUI_Dialogs_ShowQueryInspector(int32_t tile_x, int32_t tile_y)
{
    active_modal = GUI_MODAL_QUERY_INSPECTOR;
    Dialog_Inspector_Show(tile_x, tile_y);
}

void GUI_Dialogs_ShowNotice(int16_t picture_id, int32_t tile_x, int32_t tile_y)
{
    Dialog_Notice_Show(picture_id, tile_x, tile_y);
}

bool GUI_Dialogs_HandleNoticeKey(SDL_Keycode key)
{
    return Dialog_Notice_HandleKeyDown(key);
}

void GUI_Dialogs_UpdateNotice(uint64_t now)
{
    Dialog_Notice_Update(now);
}

void GUI_Dialogs_OpenQuitConfirm(void)
{
    if (active_modal == GUI_MODAL_QUIT_CONFIRM) return;
    GUI_Dialogs_Close();
    active_modal = GUI_MODAL_QUIT_CONFIRM;
    Dialog_Quit_Show();
}

bool GUI_Dialogs_TakeQuitConfirmed(void)
{
    return Dialog_Quit_TakeConfirmed();
}

void GUI_Dialogs_OpenChooseCity(void (*on_confirm)(int32_t), int32_t arg)
{
    GUI_Dialogs_Close();
    active_modal = GUI_MODAL_CHOOSE_CITY;
    Dialog_ChooseCity_Show(on_confirm, arg);
}

void GUI_Dialogs_OpenConfirm(const char *title, const char *line1, const char *line2, const char *confirm_label,
                             void (*on_confirm)(int32_t), int32_t arg)
{
    GUI_Dialogs_Close();
    active_modal = GUI_MODAL_CHOOSE_CITY;
    Dialog_Confirm_Show(title, line1, line2, confirm_label, on_confirm, arg);
}

void GUI_Dialogs_OpenEditor(void)
{
    if (!Scenario_EditorIsActive()) return;
    GUI_Dialogs_Close();
    active_modal = GUI_MODAL_EDITOR;
    Dialog_Editor_Open();
}

void GUI_Dialogs_Close(void)
{
    CitySave_Cancel();
    if (active_modal == GUI_MODAL_CITY_MGMT) {
        Dialog_City_Reset();
        if (pending_annual_budget) {
            ApplyBudgetCommit();
            pending_annual_budget = false;
            SetSimSpeed(saved_sim_speed);
            Audio_PlayRunningSound(SimSpeed);
        }
    } else if (active_modal == GUI_MODAL_CHOOSE_CITY) {
        Dialog_ChooseCity_Reset();
    } else if (active_modal == GUI_MODAL_EDITOR) {
        Dialog_Editor_Reset();
    }

    active_modal = GUI_MODAL_NONE;
}


OverlayViewMode GUI_Dialogs_GetActiveOverlay(void)
{
    return active_overlay_mode;
}

void GUI_Dialogs_SetActiveOverlay(OverlayViewMode mode)
{
    active_overlay_mode = mode;
}

bool GUI_Dialogs_HandleTextInput(const char *text)
{
    if (CitySave_IsActive()) {
        return CitySave_HandleText(text);
    }
    if (active_modal == GUI_MODAL_CITY_MGMT) {
        return Dialog_City_HandleTextInput(text);
    }
    if (active_modal == GUI_MODAL_EDITOR) {
        return Dialog_Editor_HandleTextInput(text);
    }
    return false;
}

bool GUI_Dialogs_HandleKeyDown(SDL_Keycode key)
{
    if (active_modal == GUI_MODAL_NONE) return false;

    if (CitySave_HandleKeyDown(key)) return true;

    if (active_modal == GUI_MODAL_EDITOR && Dialog_Editor_HandleKeyDown(key)) return true;

    if (key == SDLK_ESCAPE) {
        GUI_Dialogs_Close();
        return true;
    }

    if (active_modal == GUI_MODAL_SCENARIO_RESULT || active_modal == GUI_MODAL_QUERY_INSPECTOR ||
        active_modal == GUI_MODAL_QUIT_CONFIRM) {
        if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_KP_ENTER) {
            GUI_Dialogs_Close();
            return true;
        }
    }

    if (active_modal == GUI_MODAL_CITY_MGMT) {
        return Dialog_City_HandleKeyDown(key);
    }
    return true;
}

bool GUI_Dialogs_HandleClick(float mx, float my)
{
    switch (active_modal) {
    case GUI_MODAL_CITY_MGMT:
        return Dialog_City_HandleClick(mx, my);
    case GUI_MODAL_OVERLAYS:
        return Dialog_Overlays_HandleClick(mx, my);
    case GUI_MODAL_FILE:
        return Dialog_File_HandleClick(mx, my);
    case GUI_MODAL_DISASTERS:
        return Dialog_Disasters_HandleClick(mx, my);
    case GUI_MODAL_SCENARIO_RESULT:
        return Dialog_Scenario_HandleClick(mx, my);
    case GUI_MODAL_QUERY_INSPECTOR:
        return Dialog_Inspector_HandleClick(mx, my);
    case GUI_MODAL_QUIT_CONFIRM:
        return Dialog_Quit_HandleClick(mx, my);
    case GUI_MODAL_CHOOSE_CITY:
        return Dialog_ChooseCity_HandleClick(mx, my);
    case GUI_MODAL_EDITOR:
        return Dialog_Editor_HandleClick(mx, my);
    case GUI_MODAL_NEWS:
        return Dialog_News_HandleClick(mx, my);
    default:
        return Dialog_Notice_HandleClick(mx, my);
    }
}

bool GUI_Dialogs_HandleWheel(float wheel_y)
{
    if (active_modal == GUI_MODAL_NEWS) return Dialog_News_HandleWheel(wheel_y);
    return false;
}

void GUI_Dialogs_Render(SDL_Renderer *renderer)
{
    Dialog_Notice_Render(renderer);

    switch (active_modal) {
    case GUI_MODAL_CITY_MGMT:
        Dialog_City_Render(renderer);
        break;
    case GUI_MODAL_OVERLAYS:
        Dialog_Overlays_Render(renderer);
        break;
    case GUI_MODAL_FILE:
        Dialog_File_Render(renderer);
        break;
    case GUI_MODAL_DISASTERS:
        Dialog_Disasters_Render(renderer);
        break;
    case GUI_MODAL_SCENARIO_RESULT:
        Dialog_Scenario_Render(renderer);
        break;
    case GUI_MODAL_QUERY_INSPECTOR:
        Dialog_Inspector_Render(renderer);
        break;
    case GUI_MODAL_QUIT_CONFIRM:
        Dialog_Quit_Render(renderer);
        break;
    case GUI_MODAL_CHOOSE_CITY:
        Dialog_ChooseCity_Render(renderer);
        break;
    case GUI_MODAL_EDITOR:
        Dialog_Editor_Render(renderer);
        break;
    case GUI_MODAL_NEWS:
        Dialog_News_Render(renderer);
        break;
    default:
        break;
    }
}