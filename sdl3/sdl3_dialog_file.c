/* sdl3_dialog_file.c: File persistence, historical scenarios, and preference toggles.
 */

#include "sdl3_dialog_common.h"
#include "sdl3_gui.h"
#include "sdl3_audio.h"
#include "sdl3_music.h"
#include "sdl3_settings.h"
#include "scenario.h"
#include <ctype.h>
#include <stdint.h>

static FileDialogTab active_file_tab = FILE_TAB_SAVE_LOAD;
static int new_city_level = 0;
static const char *const NewCityLevelNames[3] = { "Easy", "Medium", "Hard" };

#define TERRAIN_OPT_COUNT 4
#define TERRAIN_OPT_VALUES 4

static const char *const terrain_opt_names[TERRAIN_OPT_COUNT] = { "Trees", "Lakes", "Rivers", "Island" };
static const char *const terrain_opt_labels[TERRAIN_OPT_COUNT][TERRAIN_OPT_VALUES] = {
    { "Random", "None", "Sparse", "Dense" },
    { "Random", "None", "Few", "Many" },
    { "Random", "None", "Straight", "Curvy" },
    { "Random", "Never", "Always", NULL }
};
static const int32_t terrain_opt_values[TERRAIN_OPT_COUNT][TERRAIN_OPT_VALUES] = {
    { -1, 0, 10, 100 },
    { -1, 0, 4, 20 },
    { -1, 0, 1, 190 },
    { -1, 0, 1, 0 }
};
static const int terrain_opt_count[TERRAIN_OPT_COUNT] = { 4, 4, 4, 3 };
static int terrain_opt_index[TERRAIN_OPT_COUNT] = { 0, 0, 0, 0 };

static SDL_FRect FileActionRect(float cx, float y, const char *label, float pad, float h)
{
    float w = GUI_ButtonWidth(label, pad);
    SDL_FRect r = { cx - w / 2.0f, y, w, h };
    return r;
}

static SDL_FRect FileSaveRect(float dx, float dy)
{
    return FileActionRect(dx + 150.0f, dy + 138.0f, "Save City", 24.0f, 34.0f);
}

static SDL_FRect FileLoadRect(float dx, float dy)
{
    return FileActionRect(dx + 390.0f, dy + 138.0f, "Load City", 24.0f, 34.0f);
}

static SDL_FRect FileNewRect(float dx, float dy)
{
    return FileActionRect(dx + 270.0f, dy + 185.0f, "New City Map", 24.0f, 34.0f);
}

static SDL_FRect FileEditorRect(float dx, float dy)
{
    return FileActionRect(dx + 150.0f, dy + 228.0f, "Editor Mode", 24.0f, 34.0f);
}

static SDL_FRect SaveNameSaveRect(float dx, float dy)
{
    return FileActionRect(dx + 220.0f, dy + 242.0f, "Save", 24.0f, 34.0f);
}

static SDL_FRect SaveNameCancelRect(float dx, float dy)
{
    return FileActionRect(dx + 320.0f, dy + 242.0f, "Cancel", 24.0f, 34.0f);
}

static SDL_FRect FileScenarioRect(float dx, float dy)
{
    return FileActionRect(dx + 390.0f, dy + 228.0f, "Load Scenario", 24.0f, 34.0f);
}

static SDL_FRect FileDifficultyRect(float dx, float dy, int i)
{
    return FileActionRect(dx + 120.0f + (float)i * 140.0f, dy + 318.0f, NewCityLevelNames[i], 20.0f, 28.0f);
}

static SDL_FRect TerrainOptRect(float dx, float dy, int i)
{
    SDL_FRect r = {
        dx + 60.0f + (float)(i % 2) * 210.0f,
        dy + 370.0f + (float)(i / 2) * 36.0f,
        200.0f,
        28.0f
    };
    return r;
}

static const float file_tab_width = 98.0f;
static const float file_tab_gap = 4.0f;

static SDL_FRect FileTabRect(float dx, float dy, int i)
{
    SDL_FRect r = { dx + 16.0f + (float)i * (file_tab_width + file_tab_gap), dy + 42.0f, file_tab_width, 28.0f };
    return r;
}

static SDL_FRect JukeboxVolumeRect(float dx, float dy)
{
    SDL_FRect r = { dx + 32.0f, dy + 166.0f, 260.0f, 26.0f };
    return r;
}

static SDL_FRect JukeboxOrderRect(float dx, float dy)
{
    SDL_FRect r = { dx + 32.0f, dy + 204.0f, 260.0f, 26.0f };
    return r;
}

static SDL_FRect JukeboxSkipRect(float dx, float dy)
{
    SDL_FRect r = { dx + 32.0f, dy + 242.0f, 120.0f, 26.0f };
    return r;
}

static bool PointInRect(SDL_FRect r, float mx, float my)
{
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

void Dialog_File_Open(FileDialogTab tab)
{
    active_file_tab = tab;
    CityIO_ClearStatus();
    ScnList_Close();
    CityList_Close();
    CitySave_Cancel();
}

static void EditorStartConfirmed(int32_t arg)
{
    (void)arg;
    Dialog_Editor_Start();
}

/* Copies the File tab's terrain choices into the generator globals. */
void Dialog_File_ApplyTerrainOptions(void)
{
    for (int i = 0; i < TERRAIN_OPT_COUNT; i++) {
        int32_t value = terrain_opt_values[i][terrain_opt_index[i]];
        if (i == 0) TreeLevel = value;
        else if (i == 1) LakeLevel = value;
        else if (i == 2) CurveLevel = value;
        else CreateIsland = value;
    }
}

/* Run only after the "Choose Another City" confirmation. */
static void NewCityConfirmed(int32_t level)
{
    Scenario_EditorEnd();
    Sim_InitEngine();
    Dialog_File_ApplyTerrainOptions();
    GenerateMap((uint32_t)SDL_GetTicks());
    SetNewCityLevel((int16_t)level);
    SetSimSpeed(3);
    char buf[64];
    snprintf(buf, sizeof(buf), "New %s city generated!", NewCityLevelNames[GameLevel]);
    GUI_ShowStatus(buf);
}

bool Dialog_File_HandleClick(float mx, float my)
{
    float dw = 540.0f;
    float dx = DialogCenteredX(dw);
    float dy = DialogCenteredY(520.0f);

    if (mx >= dx + dw - 32.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 32.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    for (int i = 0; i < FILE_TAB_COUNT; i++) {
        SDL_FRect tr = FileTabRect(dx, dy, i);
        if (mx >= tr.x && mx <= tr.x + tr.w && my >= tr.y && my <= tr.y + tr.h) {
            ScnList_Close();
            CityList_Close();
            CitySave_Cancel();
            active_file_tab = (FileDialogTab)i;
            return true;
        }
    }

    if (active_file_tab == FILE_TAB_SAVE_LOAD && CitySave_IsActive()) {
        if (PointInRect(SaveNameSaveRect(dx, dy), mx, my)) {
            CitySave_Submit();
        } else if (PointInRect(SaveNameCancelRect(dx, dy), mx, my)) {
            CitySave_Cancel();
        }
        return true;
    }

    if (active_file_tab == FILE_TAB_SAVE_LOAD && CityList_IsOpen()) {
        return CityList_HandleClick(mx, my, dx, dy);
    }

    if (active_file_tab == FILE_TAB_SAVE_LOAD && ScnList_IsOpen()) {
        return ScnList_HandleClick(mx, my, dx, dy);
    }

    if (active_file_tab == FILE_TAB_JUKEBOX) {
        if (PointInRect(JukeboxVolumeRect(dx, dy), mx, my)) {
            Settings_CycleMusicVolume();
            return true;
        }
        if (PointInRect(JukeboxOrderRect(dx, dy), mx, my)) {
            Settings_ToggleMusicOrder();
            return true;
        }
        if (PointInRect(JukeboxSkipRect(dx, dy), mx, my)) {
            Music_Skip();
            return true;
        }
    }

    if (active_file_tab == FILE_TAB_SAVE_LOAD) {
        SDL_FRect save_btn = FileSaveRect(dx, dy);
        if (mx >= save_btn.x && mx <= save_btn.x + save_btn.w &&
            my >= save_btn.y && my <= save_btn.y + save_btn.h) {
            ScnList_Close();
            CityList_Close();
            CitySave_Begin(CITY_SAVE_FROM_FILE);
            return true;
        }

        SDL_FRect load_btn = FileLoadRect(dx, dy);
        if (mx >= load_btn.x && mx <= load_btn.x + load_btn.w &&
            my >= load_btn.y && my <= load_btn.y + load_btn.h) {
            ScnList_Close();
            CitySave_Cancel();
            CityList_Open();
            return true;
        }

        SDL_FRect new_btn = FileNewRect(dx, dy);
        if (mx >= new_btn.x && mx <= new_btn.x + new_btn.w &&
            my >= new_btn.y && my <= new_btn.y + new_btn.h) {
            GUI_Dialogs_OpenChooseCity(NewCityConfirmed, new_city_level);
            return true;
        }

        SDL_FRect editor_btn = FileEditorRect(dx, dy);
        if (mx >= editor_btn.x && mx <= editor_btn.x + editor_btn.w &&
            my >= editor_btn.y && my <= editor_btn.y + editor_btn.h) {
            GUI_Dialogs_OpenChooseCity(EditorStartConfirmed, 0);
            return true;
        }

        SDL_FRect scn_btn = FileScenarioRect(dx, dy);
        if (mx >= scn_btn.x && mx <= scn_btn.x + scn_btn.w &&
            my >= scn_btn.y && my <= scn_btn.y + scn_btn.h) {
            CitySave_Cancel();
            CityList_Close();
            ScnList_Open();
            return true;
        }

        for (int i = 0; i < 3; i++) {
            SDL_FRect diff_btn = FileDifficultyRect(dx, dy, i);
            if (mx >= diff_btn.x && mx <= diff_btn.x + diff_btn.w &&
                my >= diff_btn.y && my <= diff_btn.y + diff_btn.h) {
                new_city_level = i;
                return true;
            }
        }

        for (int i = 0; i < TERRAIN_OPT_COUNT; i++) {
            SDL_FRect opt = TerrainOptRect(dx, dy, i);
            if (mx >= opt.x && mx <= opt.x + opt.w && my >= opt.y && my <= opt.y + opt.h) {
                terrain_opt_index[i] = (terrain_opt_index[i] + 1) % terrain_opt_count[i];
                return true;
            }
        }
    } else if (active_file_tab == FILE_TAB_OPTIONS) {
        float opt_x = dx + 340.0f;
        float opt_y = dy + 105.0f;

        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y && my <= opt_y + 26.0f) {
            autoBudget = !autoBudget;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 35.0f && my <= opt_y + 61.0f) {
            autoBulldoze = !autoBulldoze;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 70.0f && my <= opt_y + 96.0f) {
            autoGo = !autoGo;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 105.0f && my <= opt_y + 131.0f) {
            NoDisasters = !NoDisasters;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 140.0f && my <= opt_y + 166.0f) {
            UserSoundOn = !UserSoundOn;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 175.0f && my <= opt_y + 201.0f) {
            Settings_ToggleEdgeScroll();
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 210.0f && my <= opt_y + 236.0f) {
            Settings_CycleSoundLevel();
            Audio_ApplySoundLevel();
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 245.0f && my <= opt_y + 271.0f) {
            Settings_ToggleTileAnimation();
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 280.0f && my <= opt_y + 306.0f) {
            Settings_ToggleStatusMessages();
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 315.0f && my <= opt_y + 341.0f) {
            Settings_ToggleLinearFilter();
            Video_SetLinearFilter(Settings_GetLinearFilter());
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 350.0f && my <= opt_y + 376.0f) {
            Settings_ToggleAutumnColor();
            return true;
        }
    }

    return true;
}

static void DrawJukeboxButton(SDL_Renderer *renderer, SDL_FRect btn, const char *label)
{
    SDL_SetRenderDrawColor(renderer, 40, 55, 80, 255);
    SDL_RenderFillRect(renderer, &btn);
    SDL_SetRenderDrawColor(renderer, 90, 120, 170, 255);
    SDL_RenderRect(renderer, &btn);
    GUI_DrawText(renderer, (int)btn.x + 10, (int)btn.y + 5, label, col_white);
}

static void DrawOptionRow(SDL_Renderer *renderer, int cx, float opt_x, int row_y, const char *label, bool on)
{
    GUI_DrawText(renderer, cx + 10, row_y + 4, label, col_white);

    SDL_FRect btn = { opt_x, (float)row_y, 90.0f, 26.0f };
    SDL_SetRenderDrawColor(renderer, on ? 30 : 45, on ? 70 : 25, on ? 40 : 25, 255);
    SDL_RenderFillRect(renderer, &btn);
    SDL_SetRenderDrawColor(renderer, on ? 80 : 180, on ? 220 : 60, on ? 100 : 60, 255);
    SDL_RenderRect(renderer, &btn);
    GUI_DrawText(renderer, (int)btn.x + 18, (int)btn.y + 5, on ? "[ ON ]" : "[ OFF ]", on ? col_green : col_red);
}

void Dialog_File_Render(SDL_Renderer *renderer)
{
    float dw = 540.0f;
    float dh = 520.0f;
    float dx = DialogCenteredX(dw);
    float dy = DialogCenteredY(dh);

    SDL_FRect scrim = { 0.0f, 0.0f, (float)Video_GetCanvasWidth(), (float)Video_GetCanvasHeight() };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 24, 28, 38, 250);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
    SDL_RenderRect(renderer, &win_rect);

    GUI_DrawText(renderer, (int)dx + 16, (int)dy + 14, "FILE & SCENARIO MANAGER", col_gold);

    SDL_FRect close_rect = { dx + dw - 28.0f, dy + 8.0f, 20.0f, 20.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 6, (int)close_rect.y + 4, "X", col_white);

    const char *tabs[] = { "Save / Load", "Options", "About", "Jukebox" };
    for (int i = 0; i < FILE_TAB_COUNT; i++) {
        SDL_FRect tr = FileTabRect(dx, dy, i);
        if (active_file_tab == (FileDialogTab)i) {
            SDL_SetRenderDrawColor(renderer, 50, 85, 140, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
            SDL_RenderRect(renderer, &tr);
            GUI_DrawText(renderer, (int)tr.x + 10, (int)tr.y + 6, tabs[i], col_white);
        } else {
            SDL_SetRenderDrawColor(renderer, 32, 38, 52, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 60, 72, 92, 255);
            SDL_RenderRect(renderer, &tr);
            GUI_DrawText(renderer, (int)tr.x + 10, (int)tr.y + 6, tabs[i], col_gray);
        }
    }

    SDL_FRect content_box = { dx + 16.0f, dy + 80.0f, dw - 32.0f, dh - 96.0f };
    SDL_SetRenderDrawColor(renderer, 18, 22, 30, 255);
    SDL_RenderFillRect(renderer, &content_box);
    SDL_SetRenderDrawColor(renderer, 50, 60, 80, 255);
    SDL_RenderRect(renderer, &content_box);

    int cx = (int)content_box.x + 16;
    int cy = (int)content_box.y + 16;

    if (active_file_tab == FILE_TAB_SAVE_LOAD && CitySave_IsActive()) {
        GUI_DrawText(renderer, cx, cy, "SAVE CITY", col_gold);
        GUI_DrawText(renderer, cx, cy + 22, "Cities are stored in the cities folder beside the game.", col_white);
        GUI_DrawText(renderer, cx, cy + 46, "City name:", col_white);

        SDL_FRect field = { dx + 32.0f, dy + 162.0f, dw - 64.0f, 32.0f };
        CitySave_DrawField(renderer, field);

        SDL_Color status_color;
        const char *status = CityIO_Status(&status_color);
        if (status[0] != '\0') {
            GUI_DrawText(renderer, cx, cy + 106, status, status_color);
        }

        SDL_FRect save_btn = SaveNameSaveRect(dx, dy);
        SDL_SetRenderDrawColor(renderer, 40, 90, 70, 255);
        SDL_RenderFillRect(renderer, &save_btn);
        SDL_SetRenderDrawColor(renderer, 70, 160, 120, 255);
        SDL_RenderRect(renderer, &save_btn);
        GUI_DrawTextCentered(renderer, save_btn, "Save", col_white);

        SDL_FRect cancel_btn = SaveNameCancelRect(dx, dy);
        SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
        SDL_RenderFillRect(renderer, &cancel_btn);
        SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
        SDL_RenderRect(renderer, &cancel_btn);
        GUI_DrawTextCentered(renderer, cancel_btn, "Cancel", col_white);
    } else if (active_file_tab == FILE_TAB_SAVE_LOAD && CityList_IsOpen()) {
        CityList_Render(renderer, dx, dy);
    } else if (active_file_tab == FILE_TAB_SAVE_LOAD && ScnList_IsOpen()) {
        ScnList_Render(renderer, dx, dy);
    } else if (active_file_tab == FILE_TAB_SAVE_LOAD) {
        GUI_DrawText(renderer, cx, cy, "CITY PERSISTENCE & FILE DISK", col_gold);
        GUI_DrawText(renderer, cx, cy + 22, "Save and load cities from the cities folder beside the game.", col_white);

        SDL_FRect save_btn = FileSaveRect(dx, dy);
        SDL_SetRenderDrawColor(renderer, 40, 70, 110, 255);
        SDL_RenderFillRect(renderer, &save_btn);
        SDL_SetRenderDrawColor(renderer, 80, 140, 220, 255);
        SDL_RenderRect(renderer, &save_btn);
        GUI_DrawTextCentered(renderer, save_btn, "Save City", col_white);

        SDL_FRect load_btn = FileLoadRect(dx, dy);
        SDL_SetRenderDrawColor(renderer, 40, 90, 70, 255);
        SDL_RenderFillRect(renderer, &load_btn);
        SDL_SetRenderDrawColor(renderer, 70, 160, 120, 255);
        SDL_RenderRect(renderer, &load_btn);
        GUI_DrawTextCentered(renderer, load_btn, "Load City", col_white);

        SDL_FRect new_btn = FileNewRect(dx, dy);
        SDL_SetRenderDrawColor(renderer, 100, 70, 30, 255);
        SDL_RenderFillRect(renderer, &new_btn);
        SDL_SetRenderDrawColor(renderer, 200, 140, 50, 255);
        SDL_RenderRect(renderer, &new_btn);
        GUI_DrawTextCentered(renderer, new_btn, "New City Map", col_gold);

        SDL_FRect editor_btn = FileEditorRect(dx, dy);
        SDL_SetRenderDrawColor(renderer, 40, 80, 100, 255);
        SDL_RenderFillRect(renderer, &editor_btn);
        SDL_SetRenderDrawColor(renderer, 90, 180, 210, 255);
        SDL_RenderRect(renderer, &editor_btn);
        GUI_DrawTextCentered(renderer, editor_btn, "Editor Mode", col_cyan);

        SDL_FRect scn_btn = FileScenarioRect(dx, dy);
        SDL_SetRenderDrawColor(renderer, 80, 60, 100, 255);
        SDL_RenderFillRect(renderer, &scn_btn);
        SDL_SetRenderDrawColor(renderer, 170, 130, 220, 255);
        SDL_RenderRect(renderer, &scn_btn);
        GUI_DrawTextCentered(renderer, scn_btn, "Load Scenario", col_white);

        GUI_DrawText(renderer, (int)dx + 60, (int)dy + 298, "New city difficulty:", col_white);
        for (int i = 0; i < 3; i++) {
            SDL_FRect diff_btn = FileDifficultyRect(dx, dy, i);
            bool selected = (new_city_level == i);
            SDL_SetRenderDrawColor(renderer, selected ? 70 : 35, selected ? 55 : 45, selected ? 25 : 62, 255);
            SDL_RenderFillRect(renderer, &diff_btn);
            SDL_SetRenderDrawColor(renderer, selected ? 255 : 70, selected ? 215 : 90, selected ? 0 : 130, 255);
            SDL_RenderRect(renderer, &diff_btn);
            GUI_DrawTextCentered(renderer, diff_btn, NewCityLevelNames[i], selected ? col_gold : col_white);
        }

        GUI_DrawText(renderer, (int)dx + 60, (int)dy + 352, "New city terrain:", col_white);
        for (int i = 0; i < TERRAIN_OPT_COUNT; i++) {
            SDL_FRect opt = TerrainOptRect(dx, dy, i);
            bool changed = (terrain_opt_index[i] != 0);
            SDL_SetRenderDrawColor(renderer, changed ? 70 : 35, changed ? 55 : 45, changed ? 25 : 62, 255);
            SDL_RenderFillRect(renderer, &opt);
            SDL_SetRenderDrawColor(renderer, changed ? 255 : 70, changed ? 215 : 90, changed ? 0 : 130, 255);
            SDL_RenderRect(renderer, &opt);
            char opt_buf[48];
            snprintf(opt_buf, sizeof(opt_buf), "%s: %s", terrain_opt_names[i], terrain_opt_labels[i][terrain_opt_index[i]]);
            GUI_DrawText(renderer, (int)opt.x + 10, (int)opt.y + 6, opt_buf, changed ? col_gold : col_white);
        }

        SDL_Color status_color;
        const char *status = CityIO_Status(&status_color);
        if (status[0] != '\0') {
            GUI_DrawText(renderer, cx, cy + 180, status, status_color);
        }

    } else if (active_file_tab == FILE_TAB_OPTIONS) {
        GUI_DrawText(renderer, cx, cy, "SIMULATION PREFERENCES & TOGGLES", col_gold);

        struct {
            const char *label;
            bool state;
        } opts[] = {
            { "Auto-Budget Processing", (bool)autoBudget },
            { "Auto Bulldoze", (bool)autoBulldoze },
            { "Auto-Goto Incident Coordinates", (bool)autoGo },
            { "Spontaneous Disasters Enabled", (bool)(!NoDisasters) },
            { "Sound Effects & Audio Playback", (bool)UserSoundOn }
        };

        float opt_x = dx + 340.0f;
        float opt_y = dy + 105.0f;

        for (int i = 0; i < 5; i++) {
            int row_y = (int)opt_y + (i * 35);
            GUI_DrawText(renderer, cx + 10, row_y + 4, opts[i].label, col_white);

            SDL_FRect toggle_btn = { opt_x, (float)row_y, 90.0f, 26.0f };
            SDL_SetRenderDrawColor(renderer, opts[i].state ? 30 : 45, opts[i].state ? 70 : 25, opts[i].state ? 40 : 25, 255);
            SDL_RenderFillRect(renderer, &toggle_btn);

            SDL_SetRenderDrawColor(renderer, opts[i].state ? 80 : 180, opts[i].state ? 220 : 60, opts[i].state ? 100 : 60, 255);
            SDL_RenderRect(renderer, &toggle_btn);

            const char *status_str = opts[i].state ? "[ ON ]" : "[ OFF ]";
            GUI_DrawText(renderer, (int)toggle_btn.x + 18, (int)toggle_btn.y + 5, status_str, opts[i].state ? col_green : col_red);
        }

        int edge_row_y = (int)opt_y + 175;
        GUI_DrawText(renderer, cx + 10, edge_row_y + 4, "Edge Auto-Scroll (Map Borders)", col_white);
        bool edge_on = Settings_GetEdgeScroll();
        SDL_FRect edge_btn = { opt_x, (float)edge_row_y, 90.0f, 26.0f };
        SDL_SetRenderDrawColor(renderer, edge_on ? 30 : 45, edge_on ? 70 : 25, edge_on ? 40 : 25, 255);
        SDL_RenderFillRect(renderer, &edge_btn);
        SDL_SetRenderDrawColor(renderer, edge_on ? 80 : 180, edge_on ? 220 : 60, edge_on ? 100 : 60, 255);
        SDL_RenderRect(renderer, &edge_btn);
        GUI_DrawText(renderer, (int)edge_btn.x + 18, (int)edge_btn.y + 5, edge_on ? "[ ON ]" : "[ OFF ]", edge_on ? col_green : col_red);

        int level_row_y = (int)opt_y + 210;
        GUI_DrawText(renderer, cx + 10, level_row_y + 4, "Sound Level (Click to Cycle)", col_white);
        SDL_FRect level_btn = { opt_x, (float)level_row_y, 90.0f, 26.0f };
        SDL_SetRenderDrawColor(renderer, 40, 50, 90, 255);
        SDL_RenderFillRect(renderer, &level_btn);
        SDL_SetRenderDrawColor(renderer, 120, 150, 220, 255);
        SDL_RenderRect(renderer, &level_btn);
        GUI_DrawText(renderer, (int)level_btn.x + 8, (int)level_btn.y + 5, Settings_GetSoundLevelName(), col_white);

        DrawOptionRow(renderer, cx, opt_x, (int)opt_y + 245, "Tile Animation (Map Blinking)", Settings_GetTileAnimation());
        DrawOptionRow(renderer, cx, opt_x, (int)opt_y + 280, "Status Messages (Ticker)", Settings_GetStatusMessages());
        DrawOptionRow(renderer, cx, opt_x, (int)opt_y + 315, "Linear Filtering (Smooth Scaling)", Settings_GetLinearFilter());
        DrawOptionRow(renderer, cx, opt_x, (int)opt_y + 350, "Autumn Colors (Seasonal Trees)", Settings_GetAutumnColor());
    } else if (active_file_tab == FILE_TAB_ABOUT) {
        static const char *about_lines[] = {
            "Micropolis Version 5.0 Copyright (C) 2026",
            "by Electronic Arts.",
            "",
            "Based on the original Micropolis concept and design",
            "by Will Wright.",
            "",
            "Originally ported to Unix by Don Hopkins.",
            "Originally ported to Linux by Don Hopkins for the OLPC Project.",
            "",
            "Ported to SDL3 GNU/Linux by R. Shaklfrd, Gemini and Claude.",
            "",
            "Licensed under the GNU General Public License,",
            "version 3, with additional conditions."
        };
        GUI_DrawText(renderer, cx, cy, "ABOUT MICROPOLIS", col_gold);
        for (size_t i = 0; i < sizeof(about_lines) / sizeof(about_lines[0]); i++) {
            GUI_DrawText(renderer, cx + 10, cy + 40 + (int)i * 22, about_lines[i], col_white);
        }
    } else if (active_file_tab == FILE_TAB_JUKEBOX) {
        char info[160];
        GUI_DrawText(renderer, cx, cy, "JUKEBOX - OGG MUSIC FROM THE music/ FOLDER", col_gold);

        if (Music_GetTrackCount() > 0 && !Music_IsAvailable()) {
            snprintf(info, sizeof(info), "Audio device unavailable, music is off.");
        } else if (Music_GetTrackCount() > 0) {
            snprintf(info, sizeof(info), "Tracks found: %d", Music_GetTrackCount());
        } else {
            snprintf(info, sizeof(info), "No .ogg files found in the music/ folder.");
        }
        GUI_DrawText(renderer, cx, cy + 26, info, col_white);

        const char *now = Music_GetNowPlaying();
        snprintf(info, sizeof(info), "Now playing: %s", now[0] ? now : "(nothing)");
        GUI_DrawText(renderer, cx, cy + 48, info, col_white);

        SDL_FRect vol_btn = JukeboxVolumeRect(dx, dy);
        snprintf(info, sizeof(info), "Volume: %s", Settings_GetMusicVolumeName());
        DrawJukeboxButton(renderer, vol_btn, info);

        SDL_FRect order_btn = JukeboxOrderRect(dx, dy);
        snprintf(info, sizeof(info), "Order: %s", Settings_GetMusicOrderName());
        DrawJukeboxButton(renderer, order_btn, info);

        SDL_FRect skip_btn = JukeboxSkipRect(dx, dy);
        DrawJukeboxButton(renderer, skip_btn, "Skip Track");
    }
}