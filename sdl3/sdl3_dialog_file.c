/* sdl3_dialog_file.c: File persistence, historical scenarios, and preference toggles.
 */

#include "sdl3_dialog_common.h"
#include "sdl3_gui.h"
#include "sdl3_audio.h"
#include "sdl3_settings.h"
#include <ctype.h>
#include <stdint.h>

static FileDialogTab active_file_tab = FILE_TAB_SAVE_LOAD;
static char file_status_message[128] = {0};
static SDL_Color file_status_color = { 245, 245, 250, 255 };
static int new_city_level = 0;
static const char *const NewCityLevelNames[3] = { "Easy", "Medium", "Hard" };
static Uint32 native_result_event = 0;
static bool native_pending = false;

typedef struct {
    NativeFileRequest request;
    char *path;
    bool dialog_error;
} NativeFileResult;

static const struct {
    int id;
    const char *title;
} scenario_grid[] = {
    { 1, "1. Dullsville (1900)" },
    { 2, "2. San Francisco (1906)" },
    { 3, "3. Hamburg (1944)" },
    { 4, "4. Bern (1965)" },
    { 5, "5. Tokyo (1957)" },
    { 6, "6. Detroit (1972)" },
    { 7, "7. Boston (2010)" },
    { 8, "8. Rio de Janeiro (2047)" }
};

#define SCENARIO_GRID_COUNT (sizeof(scenario_grid) / sizeof(scenario_grid[0]))

static SDL_FRect ScenarioGridRect(size_t i)
{
    SDL_FRect r = {
        240.0f + 30.0f + (float)(i % 2) * 250.0f,
        120.0f + 120.0f + (float)(i / 2) * 50.0f,
        230.0f,
        34.0f
    };
    return r;
}

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
    { -1, 0, 1, 100 },
    { -1, 0, 1, 0 }
};
static const int terrain_opt_count[TERRAIN_OPT_COUNT] = { 4, 4, 4, 3 };
static int terrain_opt_index[TERRAIN_OPT_COUNT] = { 0, 0, 0, 0 };

static SDL_FRect TerrainOptRect(int i)
{
    SDL_FRect r = {
        240.0f + 60.0f + (float)(i % 2) * 210.0f,
        120.0f + 370.0f + (float)(i / 2) * 36.0f,
        200.0f,
        28.0f
    };
    return r;
}

void Dialog_File_Open(FileDialogTab tab)
{
    active_file_tab = tab;
    file_status_message[0] = '\0';
}

/* Run only after the "Choose Another City" confirmation. */
static void NewCityConfirmed(int32_t level)
{
    Sim_InitEngine();
    for (int i = 0; i < TERRAIN_OPT_COUNT; i++) {
        int32_t value = terrain_opt_values[i][terrain_opt_index[i]];
        if (i == 0) TreeLevel = value;
        else if (i == 1) LakeLevel = value;
        else if (i == 2) CurveLevel = value;
        else CreateIsland = value;
    }
    GenerateMap((uint32_t)SDL_GetTicks());
    SetNewCityLevel((int16_t)level);
    SetSimSpeed(3);
    char buf[64];
    snprintf(buf, sizeof(buf), "New %s city generated!", NewCityLevelNames[GameLevel]);
    GUI_ShowStatus(buf);
}

static void ScenarioConfirmed(int32_t id)
{
    GUI_LoadScenarioWithStatus((int16_t)id);
}

static bool EndsWithCityExt(const char *path)
{
    size_t n = strlen(path);
    if (n < 4) return false;
    const char *ext = path + n - 4;
    return ext[0] == '.' &&
           tolower((unsigned char)ext[1]) == 'c' &&
           tolower((unsigned char)ext[2]) == 't' &&
           tolower((unsigned char)ext[3]) == 'y';
}

static void WithCityExt(const char *in, char *out, size_t cap)
{
    snprintf(out, cap, "%s", in);
    if (!EndsWithCityExt(out)) {
        strncat(out, ".cty", cap - strlen(out) - 1);
    }
}

static const char *BaseName(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
    return slash ? slash + 1 : path;
}

/* Runs on the main thread: the core is never touched from a dialog callback. */
static void ApplyNativeResult(const NativeFileResult *r)
{
    char path[512];

    switch (r->request) {
    case NATIVE_SAVE_FILE:
        if (r->dialog_error) {
            snprintf(file_status_message, sizeof(file_status_message), "Error: Save dialog failed");
            file_status_color = col_red;
        } else if (r->path) {
            WithCityExt(r->path, path, sizeof(path));
            if (SaveCity(path)) {
                snprintf(file_status_message, sizeof(file_status_message), "Successfully saved city to %.80s", BaseName(path));
                file_status_color = col_green;
            } else {
                snprintf(file_status_message, sizeof(file_status_message), "Error: Could not save to %.80s", BaseName(path));
                file_status_color = col_red;
                Audio_MakeSound("warning", "Sorry -speed 85");
            }
        }
        break;

    case NATIVE_LOAD_FILE:
        if (r->dialog_error) {
            snprintf(file_status_message, sizeof(file_status_message), "Error: Load dialog failed");
            file_status_color = col_red;
        } else if (r->path) {
            if (LoadCity(r->path)) {
                snprintf(file_status_message, sizeof(file_status_message), "Loaded %.80s successfully", BaseName(r->path));
                file_status_color = col_green;
            } else {
                snprintf(file_status_message, sizeof(file_status_message), "Failed to load %.80s", BaseName(r->path));
                file_status_color = col_red;
                Audio_MakeSound("warning", "Sorry -speed 85");
            }
        }
        break;

    case NATIVE_SAVE_QUIT:
        if (r->dialog_error) {
            Dialog_Quit_SaveFinished(false, NULL);
        } else if (r->path) {
            WithCityExt(r->path, path, sizeof(path));
            Dialog_Quit_SaveFinished(SaveCity(path), BaseName(path));
        }
        break;
    }
}

/* SDL may invoke this on any thread, so it only queues the result as a user event. */
static void SDLCALL NativeFileCallback(void *userdata, const char *const *filelist, int filter)
{
    (void)filter;
    NativeFileResult *r = (NativeFileResult *)SDL_calloc(1, sizeof(*r));
    if (!r) return;

    r->request = (NativeFileRequest)(uintptr_t)userdata;
    if (!filelist) {
        r->dialog_error = true;
    } else if (filelist[0]) {
        r->path = SDL_strdup(filelist[0]);
    }

    SDL_Event ev;
    SDL_zero(ev);
    ev.type = native_result_event;
    ev.user.type = native_result_event;
    ev.user.data1 = r;
    if (!SDL_PushEvent(&ev)) {
        SDL_free(r->path);
        SDL_free(r);
    }
}

/* Resolves the cities/ folder copied next to the executable by CMake. */
static const char *CitiesDir(void)
{
    static char dir[1024];
    if (!dir[0]) {
        const char *base = SDL_GetBasePath();
        snprintf(dir, sizeof(dir), "%scities", base ? base : "");
    }
    return dir;
}

bool Dialog_File_NativePending(void)
{
    return native_pending;
}

void Dialog_File_RequestNative(NativeFileRequest req)
{
    static const SDL_DialogFileFilter save_filters[] = { { "Micropolis city", "cty" } };
    static const SDL_DialogFileFilter load_filters[] = { { "Micropolis city", "cty;snro" } };

    if (native_pending) return;

    if (!native_result_event) native_result_event = SDL_RegisterEvents(1);
    if (!native_result_event) {
        NativeFileResult fail = { req, NULL, true };
        ApplyNativeResult(&fail);
        return;
    }

    native_pending = true;
    SDL_Window *window = Video_GetWindow();
    void *userdata = (void *)(uintptr_t)req;
    const char *dir = CitiesDir();
    if (req == NATIVE_LOAD_FILE) {
        SDL_ShowOpenFileDialog(NativeFileCallback, userdata, window, load_filters, 1, dir, false);
    } else {
        char default_path[1100];
        snprintf(default_path, sizeof(default_path), "%s/mycity.cty", dir);
        SDL_ShowSaveFileDialog(NativeFileCallback, userdata, window, save_filters, 1, default_path);
    }
}

bool Dialog_File_HandleNativeEvent(const SDL_Event *event)
{
    if (!native_result_event || event->type != native_result_event) return false;

    NativeFileResult *r = (NativeFileResult *)event->user.data1;
    native_pending = false;
    ApplyNativeResult(r);
    SDL_free(r->path);
    SDL_free(r);
    return true;
}

bool Dialog_File_HandleClick(float mx, float my)
{
    float dx = 240.0f;
    float dy = 120.0f;
    float dw = 540.0f;

    if (mx >= dx + dw - 32.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 32.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    float tab_x = dx + 16.0f;
    float tab_y = dy + 42.0f;
    float tab_w = 120.0f;
    float tab_h = 28.0f;

    for (int i = 0; i < FILE_TAB_COUNT; i++) {
        if (mx >= tab_x && mx <= tab_x + tab_w && my >= tab_y && my <= tab_y + tab_h) {
            active_file_tab = (FileDialogTab)i;
            return true;
        }
        tab_x += tab_w + 8.0f;
    }

    if (active_file_tab == FILE_TAB_SAVE_LOAD) {
        SDL_FRect save_btn = { dx + 60.0f, dy + 138.0f, 180.0f, 34.0f };
        if (mx >= save_btn.x && mx <= save_btn.x + save_btn.w &&
            my >= save_btn.y && my <= save_btn.y + save_btn.h) {
            Dialog_File_RequestNative(NATIVE_SAVE_FILE);
            return true;
        }

        SDL_FRect load_btn = { dx + 300.0f, dy + 138.0f, 180.0f, 34.0f };
        if (mx >= load_btn.x && mx <= load_btn.x + load_btn.w &&
            my >= load_btn.y && my <= load_btn.y + load_btn.h) {
            Dialog_File_RequestNative(NATIVE_LOAD_FILE);
            return true;
        }

        SDL_FRect new_btn = { dx + 180.0f, dy + 185.0f, 180.0f, 34.0f };
        if (mx >= new_btn.x && mx <= new_btn.x + new_btn.w &&
            my >= new_btn.y && my <= new_btn.y + new_btn.h) {
            GUI_Dialogs_OpenChooseCity(NewCityConfirmed, new_city_level);
            return true;
        }

        for (int i = 0; i < 3; i++) {
            SDL_FRect diff_btn = { dx + 60.0f + (float)i * 140.0f, dy + 318.0f, 120.0f, 28.0f };
            if (mx >= diff_btn.x && mx <= diff_btn.x + diff_btn.w &&
                my >= diff_btn.y && my <= diff_btn.y + diff_btn.h) {
                new_city_level = i;
                return true;
            }
        }

        for (int i = 0; i < TERRAIN_OPT_COUNT; i++) {
            SDL_FRect opt = TerrainOptRect(i);
            if (mx >= opt.x && mx <= opt.x + opt.w && my >= opt.y && my <= opt.y + opt.h) {
                terrain_opt_index[i] = (terrain_opt_index[i] + 1) % terrain_opt_count[i];
                return true;
            }
        }
    } else if (active_file_tab == FILE_TAB_SCENARIOS) {
        for (size_t i = 0; i < SCENARIO_GRID_COUNT; i++) {
            SDL_FRect r = ScenarioGridRect(i);
            if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                GUI_Dialogs_OpenChooseCity(ScenarioConfirmed, scenario_grid[i].id);
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
            Settings_TogglePictureNotices();
            return true;
        }
    }

    return true;
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
    float dx = 240.0f;
    float dy = 120.0f;
    float dw = 540.0f;
    float dh = 480.0f;

    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
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

    const char *tabs[] = { "Save / Load", "Scenarios", "Options", "About" };
    float tab_x = dx + 16.0f;
    float tab_y = dy + 42.0f;
    float tab_w = 120.0f;
    float tab_h = 28.0f;

    for (int i = 0; i < FILE_TAB_COUNT; i++) {
        SDL_FRect tr = { tab_x, tab_y, tab_w, tab_h };
        if (active_file_tab == (FileDialogTab)i) {
            SDL_SetRenderDrawColor(renderer, 50, 85, 140, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
            SDL_RenderRect(renderer, &tr);
            GUI_DrawText(renderer, (int)tr.x + 14, (int)tr.y + 6, tabs[i], col_white);
        } else {
            SDL_SetRenderDrawColor(renderer, 32, 38, 52, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 60, 72, 92, 255);
            SDL_RenderRect(renderer, &tr);
            GUI_DrawText(renderer, (int)tr.x + 14, (int)tr.y + 6, tabs[i], col_gray);
        }
        tab_x += tab_w + 8.0f;
    }

    SDL_FRect content_box = { dx + 16.0f, dy + 80.0f, dw - 32.0f, dh - 96.0f };
    SDL_SetRenderDrawColor(renderer, 18, 22, 30, 255);
    SDL_RenderFillRect(renderer, &content_box);
    SDL_SetRenderDrawColor(renderer, 50, 60, 80, 255);
    SDL_RenderRect(renderer, &content_box);

    int cx = (int)content_box.x + 16;
    int cy = (int)content_box.y + 16;

    if (active_file_tab == FILE_TAB_SAVE_LOAD) {
        GUI_DrawText(renderer, cx, cy, "CITY PERSISTENCE & FILE DISK", col_gold);
        GUI_DrawText(renderer, cx, cy + 22, "Save and Load open your system's file dialog.", col_white);

        SDL_FRect save_btn = { dx + 60.0f, dy + 138.0f, 180.0f, 34.0f };
        SDL_SetRenderDrawColor(renderer, 40, 70, 110, 255);
        SDL_RenderFillRect(renderer, &save_btn);
        SDL_SetRenderDrawColor(renderer, 80, 140, 220, 255);
        SDL_RenderRect(renderer, &save_btn);
        GUI_DrawText(renderer, (int)save_btn.x + 44, (int)save_btn.y + 8, "Save City", col_white);

        SDL_FRect load_btn = { dx + 300.0f, dy + 138.0f, 180.0f, 34.0f };
        SDL_SetRenderDrawColor(renderer, 40, 90, 70, 255);
        SDL_RenderFillRect(renderer, &load_btn);
        SDL_SetRenderDrawColor(renderer, 70, 160, 120, 255);
        SDL_RenderRect(renderer, &load_btn);
        GUI_DrawText(renderer, (int)load_btn.x + 44, (int)load_btn.y + 8, "Load City", col_white);

        SDL_FRect new_btn = { dx + 180.0f, dy + 185.0f, 180.0f, 34.0f };
        SDL_SetRenderDrawColor(renderer, 100, 70, 30, 255);
        SDL_RenderFillRect(renderer, &new_btn);
        SDL_SetRenderDrawColor(renderer, 200, 140, 50, 255);
        SDL_RenderRect(renderer, &new_btn);
        GUI_DrawText(renderer, (int)new_btn.x + 36, (int)new_btn.y + 8, "New City Map", col_gold);

        GUI_DrawText(renderer, (int)dx + 60, (int)dy + 298, "New city difficulty:", col_white);
        for (int i = 0; i < 3; i++) {
            SDL_FRect diff_btn = { dx + 60.0f + (float)i * 140.0f, dy + 318.0f, 120.0f, 28.0f };
            bool selected = (new_city_level == i);
            SDL_SetRenderDrawColor(renderer, selected ? 70 : 35, selected ? 55 : 45, selected ? 25 : 62, 255);
            SDL_RenderFillRect(renderer, &diff_btn);
            SDL_SetRenderDrawColor(renderer, selected ? 255 : 70, selected ? 215 : 90, selected ? 0 : 130, 255);
            SDL_RenderRect(renderer, &diff_btn);
            GUI_DrawText(renderer, (int)diff_btn.x + 34, (int)diff_btn.y + 6, NewCityLevelNames[i], selected ? col_gold : col_white);
        }

        GUI_DrawText(renderer, (int)dx + 60, (int)dy + 352, "New city terrain:", col_white);
        for (int i = 0; i < TERRAIN_OPT_COUNT; i++) {
            SDL_FRect opt = TerrainOptRect(i);
            bool changed = (terrain_opt_index[i] != 0);
            SDL_SetRenderDrawColor(renderer, changed ? 70 : 35, changed ? 55 : 45, changed ? 25 : 62, 255);
            SDL_RenderFillRect(renderer, &opt);
            SDL_SetRenderDrawColor(renderer, changed ? 255 : 70, changed ? 215 : 90, changed ? 0 : 130, 255);
            SDL_RenderRect(renderer, &opt);
            char opt_buf[48];
            snprintf(opt_buf, sizeof(opt_buf), "%s: %s", terrain_opt_names[i], terrain_opt_labels[i][terrain_opt_index[i]]);
            GUI_DrawText(renderer, (int)opt.x + 10, (int)opt.y + 6, opt_buf, changed ? col_gold : col_white);
        }

        if (file_status_message[0]) {
            GUI_DrawText(renderer, cx, cy + 180, file_status_message, file_status_color);
        }

    } else if (active_file_tab == FILE_TAB_SCENARIOS) {
        GUI_DrawText(renderer, cx, cy, "HISTORICAL CRISIS SCENARIOS", col_gold);

        for (size_t i = 0; i < SCENARIO_GRID_COUNT; i++) {
            SDL_FRect r = ScenarioGridRect(i);
            SDL_SetRenderDrawColor(renderer, 35, 45, 62, 255);
            SDL_RenderFillRect(renderer, &r);
            SDL_SetRenderDrawColor(renderer, 70, 90, 130, 255);
            SDL_RenderRect(renderer, &r);
            GUI_DrawText(renderer, (int)r.x + 12, (int)r.y + 8, scenario_grid[i].title, col_white);
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
        DrawOptionRow(renderer, cx, opt_x, (int)opt_y + 315, "Picture Notices (Pop-ups)", Settings_GetPictureNotices());
    } else if (active_file_tab == FILE_TAB_ABOUT) {
        /* Credits from Unix message 300; the version matches Unix MicropolisVersion. */
        static const char *about_lines[] = {
            "Micropolis Version 4.0 Copyright (C) 2007",
            "by Electronic Arts.",
            "",
            "Based on the Original Micropolis Concept and Design",
            "by Will Wright.",
            "",
            "TCL/Tk User Interface Designed and Created",
            "by Don Hopkins, DUX Software.",
            "",
            "Ported to Linux, Optimized and Adapted for OLPC",
            "by Don Hopkins.",
            "",
            "Licensed under the GNU General Public License,",
            "version 3, with additional conditions."
        };
        GUI_DrawText(renderer, cx, cy, "ABOUT MICROPOLIS", col_gold);
        for (size_t i = 0; i < sizeof(about_lines) / sizeof(about_lines[0]); i++) {
            GUI_DrawText(renderer, cx + 10, cy + 40 + (int)i * 22, about_lines[i], col_white);
        }
    }
}