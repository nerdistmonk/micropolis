/* sdl3_dialog_city_io.c: In-game city save (name entry) and city load list.
 * Cities live in the cities folder next to the executable. Used by File > Save/Load and Quit > Save & Quit.
 */

#include "sdl3_dialog_common.h"
#include "sim.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CITY_NAME_MAX      40
#define CITY_LIST_MAX      128
#define CITY_LIST_VISIBLE  5
#define CITY_FILE_BYTES    256
#define CITY_PATH_BYTES    (1024 + CITY_FILE_BYTES)
#define CITY_STATUS_BYTES  160

typedef struct {
    char file[CITY_FILE_BYTES];
    char label[CITY_FILE_BYTES];
} CityListEntry;

static bool save_active = false;
static CitySaveOrigin save_origin = CITY_SAVE_FROM_FILE;
static char save_name[CITY_NAME_MAX + 1] = {0};
static char overwrite_pending[CITY_NAME_MAX + 1] = {0};
static char status[CITY_STATUS_BYTES] = {0};
static SDL_Color status_color = { 245, 245, 250, 255 };

static bool list_open = false;
static CityListEntry entries[CITY_LIST_MAX];
static int entry_count = 0;
static int scroll_top = 0;

static bool IsNameChar(char c)
{
    return isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_';
}

/* Saves use .cot, which older builds (listing .cty only) cannot see. .cty files stay readable. */
#define CITY_SAVE_EXT ".cot"

static bool HasCityExtension(const char *name)
{
    size_t len = strlen(name);
    return len > 4 && (SDL_strcasecmp(name + len - 4, ".cot") == 0 || SDL_strcasecmp(name + len - 4, ".cty") == 0);
}

static bool FileExists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void SetStatus(const char *msg, SDL_Color color)
{
    snprintf(status, sizeof(status), "%s", msg);
    status_color = color;
}

static void TrimmedName(char *out, size_t cap, const char *src)
{
    while (*src == ' ') src++;
    snprintf(out, cap, "%s", src);
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
}

static void AppendSaveName(const char *text)
{
    for (const char *p = text; *p != '\0'; p++) {
        size_t len = strlen(save_name);
        if (len >= CITY_NAME_MAX) break;
        if (!IsNameChar(*p)) continue;
        save_name[len] = *p;
        save_name[len + 1] = '\0';
    }
}

/* The cities folder is copied next to the executable by CMake. */
const char *CityIO_Dir(void)
{
    static char dir[1024];
    if (!dir[0]) {
        const char *base = SDL_GetBasePath();
        snprintf(dir, sizeof(dir), "%scities/", base ? base : "");
    }
    return dir;
}

void CitySave_Begin(CitySaveOrigin origin)
{
    save_origin = origin;
    save_active = true;
    overwrite_pending[0] = '\0';
    status[0] = '\0';

    save_name[0] = '\0';
    AppendSaveName(CityName);
    char trimmed[CITY_NAME_MAX + 1];
    TrimmedName(trimmed, sizeof(trimmed), save_name);
    snprintf(save_name, sizeof(save_name), "%s", trimmed[0] != '\0' ? trimmed : "mycity");

    SDL_StartTextInput(Video_GetWindow());
}

void CitySave_Cancel(void)
{
    if (!save_active) return;
    save_active = false;
    overwrite_pending[0] = '\0';
    status[0] = '\0';
    SDL_StopTextInput(Video_GetWindow());
}

bool CitySave_IsActive(void)
{
    return save_active;
}

bool CitySave_HandleText(const char *text)
{
    if (!save_active) return false;
    if (text) {
        AppendSaveName(text);
        overwrite_pending[0] = '\0';
    }
    return true;
}

bool CitySave_HandleKeyDown(SDL_Keycode key)
{
    if (!save_active) return false;

    switch (key) {
    case SDLK_ESCAPE:
        CitySave_Cancel();
        break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        CitySave_Submit();
        break;
    case SDLK_BACKSPACE: {
        size_t len = strlen(save_name);
        if (len > 0) save_name[len - 1] = '\0';
        overwrite_pending[0] = '\0';
        break;
    }
    default:
        break;
    }
    return true;
}

void CitySave_Submit(void)
{
    if (!save_active) return;

    char name[CITY_NAME_MAX + 1];
    TrimmedName(name, sizeof(name), save_name);
    if (name[0] == '\0') {
        SetStatus("Enter a city name.", col_red);
        return;
    }

    char path[CITY_PATH_BYTES];
    snprintf(path, sizeof(path), "%s%s" CITY_SAVE_EXT, CityIO_Dir(), name);

    /* An existing city needs a second Save to overwrite it */
    if (FileExists(path) && strcmp(overwrite_pending, name) != 0) {
        snprintf(overwrite_pending, sizeof(overwrite_pending), "%s", name);
        char msg[CITY_STATUS_BYTES];
        snprintf(msg, sizeof(msg), "%s" CITY_SAVE_EXT " exists. Press Save again to overwrite.", name);
        SetStatus(msg, col_gold);
        return;
    }

    SDL_CreateDirectory(CityIO_Dir());
    if (!SaveCity(path)) {
        char msg[CITY_STATUS_BYTES];
        snprintf(msg, sizeof(msg), "Error: Could not save to %s" CITY_SAVE_EXT, name);
        SetStatus(msg, col_red);
        Audio_MakeSound("warning", "Sorry -speed 85");
        return;
    }

    save_active = false;
    overwrite_pending[0] = '\0';
    SDL_StopTextInput(Video_GetWindow());

    char msg[CITY_STATUS_BYTES];
    snprintf(msg, sizeof(msg), "Saved city to %s" CITY_SAVE_EXT, name);
    SetStatus(msg, col_green);
    if (save_origin == CITY_SAVE_FROM_QUIT) {
        Dialog_Quit_SaveFinished();
    }
}

void CitySave_DrawField(SDL_Renderer *renderer, SDL_FRect box)
{
    SDL_SetRenderDrawColor(renderer, 14, 18, 26, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    SDL_RenderRect(renderer, &box);

    /* Keeps the end of a long name, and the caret, inside the box */
    const int max_w = (int)box.w - 16;
    const char *shown = save_name;
    char line[CITY_NAME_MAX + 4];
    for (;;) {
        snprintf(line, sizeof(line), "%s_", shown);
        if (*shown == '\0' || GUI_GetTextWidth(line) <= max_w) break;
        shown++;
    }
    GUI_DrawText(renderer, (int)box.x + 8, (int)box.y + 6, line, col_white);
}

const char *CityIO_Status(SDL_Color *color)
{
    if (color) *color = status_color;
    return status;
}

void CityIO_ClearStatus(void)
{
    status[0] = '\0';
}

/* Scans the cities folder for .cot and .cty files. Entries are read when the list opens. */
static SDL_EnumerationResult SDLCALL CollectCity(void *userdata, const char *dirname, const char *fname)
{
    (void)userdata;
    (void)dirname;
    if (entry_count >= CITY_LIST_MAX) return SDL_ENUM_SUCCESS;
    if (!HasCityExtension(fname) || strlen(fname) >= CITY_FILE_BYTES) return SDL_ENUM_CONTINUE;

    CityListEntry *e = &entries[entry_count++];
    snprintf(e->file, sizeof(e->file), "%s", fname);
    snprintf(e->label, sizeof(e->label), "%.*s", (int)(strlen(fname) - 4), fname);
    return SDL_ENUM_CONTINUE;
}

static int CompareCities(const void *a, const void *b)
{
    const CityListEntry *ea = (const CityListEntry *)a;
    const CityListEntry *eb = (const CityListEntry *)b;
    return SDL_strcasecmp(ea->label, eb->label);
}

static int CityMaxScroll(void)
{
    return entry_count > CITY_LIST_VISIBLE ? entry_count - CITY_LIST_VISIBLE : 0;
}

static bool PointIn(SDL_FRect r, float mx, float my)
{
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

static SDL_FRect CityRowRect(float dx, float dy, int visible_index)
{
    SDL_FRect r = { dx + 32.0f, dy + 138.0f + (float)visible_index * 54.0f, 476.0f, 50.0f };
    return r;
}

static SDL_FRect CityUpRect(float dx, float dy)
{
    SDL_FRect r = { dx + 32.0f, dy + 414.0f, 90.0f, 28.0f };
    return r;
}

static SDL_FRect CityDownRect(float dx, float dy)
{
    SDL_FRect r = { dx + 132.0f, dy + 414.0f, 90.0f, 28.0f };
    return r;
}

static SDL_FRect CityBackRect(float dx, float dy)
{
    SDL_FRect r = { dx + 418.0f, dy + 414.0f, 90.0f, 28.0f };
    return r;
}

static void DrawNavButton(SDL_Renderer *renderer, SDL_FRect r, const char *label, bool enabled)
{
    SDL_SetRenderDrawColor(renderer, enabled ? 40 : 28, enabled ? 60 : 32, enabled ? 95 : 44, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, enabled ? 90 : 55, enabled ? 130 : 70, enabled ? 200 : 100, 255);
    SDL_RenderRect(renderer, &r);
    GUI_DrawTextCentered(renderer, r, label, enabled ? col_white : col_gray);
}

/* Runs only after the "Choose Another City" confirmation. */
static void CityListConfirmed(int32_t index)
{
    if (index < 0 || index >= entry_count) return;

    const CityListEntry *e = &entries[index];
    char path[CITY_PATH_BYTES];
    snprintf(path, sizeof(path), "%s%s", CityIO_Dir(), e->file);
    CityList_Close();

    char msg[CITY_FILE_BYTES + 64];
    if (LoadCity(path)) {
        CityName_FromFile(path);
        snprintf(msg, sizeof(msg), "Loaded %s", e->label);
    } else {
        snprintf(msg, sizeof(msg), "Failed to load %s", e->label);
        Audio_MakeSound("warning", "Sorry -speed 85");
    }
    GUI_ShowStatus(msg);
}

void CityList_Open(void)
{
    list_open = true;
    entry_count = 0;
    scroll_top = 0;
    (void)SDL_EnumerateDirectory(CityIO_Dir(), CollectCity, NULL);
    if (entry_count > 1) {
        qsort(entries, (size_t)entry_count, sizeof(entries[0]), CompareCities);
    }
}

void CityList_Close(void)
{
    list_open = false;
}

bool CityList_IsOpen(void)
{
    return list_open;
}

void CityList_Render(SDL_Renderer *renderer, float dx, float dy)
{
    int cx = (int)dx + 32;
    GUI_DrawText(renderer, cx, (int)dy + 96, "LOAD CITY", col_gold);
    GUI_DrawText(renderer, cx, (int)dy + 118, "Choosing a city abandons the current city.", col_white);

    if (entry_count == 0) {
        GUI_DrawText(renderer, cx + 10, (int)dy + 142, "No saved cities found in the cities folder.", col_gray);
    }

    int last = scroll_top + CITY_LIST_VISIBLE;
    if (last > entry_count) last = entry_count;

    for (int i = scroll_top; i < last; i++) {
        SDL_FRect row = CityRowRect(dx, dy, i - scroll_top);
        SDL_SetRenderDrawColor(renderer, 35, 45, 62, 255);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, 70, 90, 130, 255);
        SDL_RenderRect(renderer, &row);
        GUI_DrawTextCentered(renderer, row, entries[i].label, col_gold);
    }

    DrawNavButton(renderer, CityUpRect(dx, dy), "Up", scroll_top > 0);
    DrawNavButton(renderer, CityDownRect(dx, dy), "Down", scroll_top < CityMaxScroll());
    DrawNavButton(renderer, CityBackRect(dx, dy), "Back", true);

    char page[48];
    snprintf(page, sizeof(page), "%d-%d of %d", entry_count > 0 ? scroll_top + 1 : 0, last, entry_count);
    GUI_DrawText(renderer, (int)dx + 232, (int)dy + 421, page, col_gray);
}

bool CityList_HandleClick(float mx, float my, float dx, float dy)
{
    if (PointIn(CityUpRect(dx, dy), mx, my)) {
        if (scroll_top > 0) scroll_top--;
        return true;
    }
    if (PointIn(CityDownRect(dx, dy), mx, my)) {
        if (scroll_top < CityMaxScroll()) scroll_top++;
        return true;
    }
    if (PointIn(CityBackRect(dx, dy), mx, my)) {
        CityList_Close();
        return true;
    }

    for (int i = 0; i < CITY_LIST_VISIBLE; i++) {
        int index = scroll_top + i;
        if (index >= entry_count) break;
        if (PointIn(CityRowRect(dx, dy, i), mx, my)) {
            GUI_Dialogs_OpenChooseCity(CityListConfirmed, index);
            return true;
        }
    }
    return true;
}
