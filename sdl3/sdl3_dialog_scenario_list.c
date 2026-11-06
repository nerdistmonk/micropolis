/* sdl3_dialog_scenario_list.c: Load Scenario list shown in place of the Save / Load tab body.
 * Lists the .scn files in the scenarios folder. Entries are read when the list opens.
 */

#include "sdl3_dialog_common.h"
#include "scenario.h"
#include <stdlib.h>
#include <string.h>

#define SCN_LIST_MAX     128
#define SCN_LIST_VISIBLE 6
#define SCN_FILE_BYTES   256
#define SCN_PATH_BYTES   (1024 + SCN_FILE_BYTES)

typedef struct {
    char file[SCN_FILE_BYTES];
    char name[SCN_NAME_BYTES];
    char description[SCN_DESC_BYTES];
} ScnListEntry;

static bool list_open = false;
static char list_dir[1024];
static ScnListEntry entries[SCN_LIST_MAX];
static int entry_count = 0;
static int scroll_top = 0;

static bool HasScnExtension(const char *name)
{
    size_t len = strlen(name);
    return len > 4 && SDL_strcasecmp(name + len - 4, ".scn") == 0;
}

static SDL_EnumerationResult SDLCALL CollectScenario(void *userdata, const char *dirname, const char *fname)
{
    (void)userdata;
    (void)dirname;
    if (entry_count >= SCN_LIST_MAX) return SDL_ENUM_SUCCESS;
    if (!HasScnExtension(fname) || strlen(fname) >= SCN_FILE_BYTES) return SDL_ENUM_CONTINUE;

    char path[SCN_PATH_BYTES];
    if (snprintf(path, sizeof(path), "%s%s", list_dir, fname) >= (int)sizeof(path)) return SDL_ENUM_CONTINUE;

    ScenarioDef def;
    if (!Scenario_ReadDef(path, &def)) return SDL_ENUM_CONTINUE;

    ScnListEntry *e = &entries[entry_count++];
    snprintf(e->file, sizeof(e->file), "%s", fname);
    snprintf(e->name, sizeof(e->name), "%s", def.name);
    snprintf(e->description, sizeof(e->description), "%s", def.description);
    return SDL_ENUM_CONTINUE;
}

static int CompareEntries(const void *a, const void *b)
{
    const ScnListEntry *ea = (const ScnListEntry *)a;
    const ScnListEntry *eb = (const ScnListEntry *)b;
    int c = SDL_strcasecmp(ea->name, eb->name);
    return c != 0 ? c : SDL_strcasecmp(ea->file, eb->file);
}

static int MaxScroll(void)
{
    return entry_count > SCN_LIST_VISIBLE ? entry_count - SCN_LIST_VISIBLE : 0;
}

static bool PointIn(SDL_FRect r, float mx, float my)
{
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

static SDL_FRect RowRect(float dx, float dy, int visible_index)
{
    SDL_FRect r = { dx + 32.0f, dy + 142.0f + (float)visible_index * 44.0f, 476.0f, 40.0f };
    return r;
}

static SDL_FRect UpRect(float dx, float dy)
{
    SDL_FRect r = { dx + 32.0f, dy + 414.0f, 90.0f, 28.0f };
    return r;
}

static SDL_FRect DownRect(float dx, float dy)
{
    SDL_FRect r = { dx + 132.0f, dy + 414.0f, 90.0f, 28.0f };
    return r;
}

static SDL_FRect BackRect(float dx, float dy)
{
    SDL_FRect r = { dx + 418.0f, dy + 414.0f, 90.0f, 28.0f };
    return r;
}

/* Shortens text with "..." so it fits within max_w pixels. */
static void FitText(char *text, size_t cap, int max_w)
{
    char trial[SCN_DESC_BYTES + 4];
    if (GUI_GetTextWidth(text) <= max_w) return;

    size_t len = strlen(text);
    while (len > 0) {
        len--;
        snprintf(trial, sizeof(trial), "%.*s...", (int)len, text);
        if (GUI_GetTextWidth(trial) <= max_w) break;
    }
    snprintf(text, cap, "%s", trial);
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
static void ScnListConfirmed(int32_t index)
{
    if (index < 0 || index >= entry_count) return;

    const ScnListEntry *e = &entries[index];
    char path[SCN_PATH_BYTES];
    snprintf(path, sizeof(path), "%s%s", list_dir, e->file);
    ScnList_Close();

    char msg[SCN_FILE_BYTES + 64];
    if (Scenario_LoadFile(path)) {
        snprintf(msg, sizeof(msg), "Scenario loaded: %s", e->name);
    } else {
        snprintf(msg, sizeof(msg), "Could not load scenario: %s", e->file);
    }
    GUI_ShowStatus(msg);
}

void ScnList_Open(void)
{
    list_open = true;
    entry_count = 0;
    scroll_top = 0;
    Scenario_GetDirectory(list_dir, sizeof(list_dir));
    (void)SDL_EnumerateDirectory(list_dir, CollectScenario, NULL);
    if (entry_count > 1) {
        qsort(entries, (size_t)entry_count, sizeof(entries[0]), CompareEntries);
    }
}

void ScnList_Close(void)
{
    list_open = false;
}

bool ScnList_IsOpen(void)
{
    return list_open;
}

void ScnList_Render(SDL_Renderer *renderer, float dx, float dy)
{
    int cx = (int)dx + 32;
    GUI_DrawText(renderer, cx, (int)dy + 96, "LOAD SCENARIO", col_gold);
    GUI_DrawText(renderer, cx, (int)dy + 118, "Choosing a scenario abandons the current city.", col_white);

    if (entry_count == 0) {
        GUI_DrawText(renderer, cx + 10, (int)dy + 142, "No scenarios found in the scenarios folder.", col_gray);
    }

    int last = scroll_top + SCN_LIST_VISIBLE;
    if (last > entry_count) last = entry_count;
    const int text_w = 456;

    for (int i = scroll_top; i < last; i++) {
        SDL_FRect row = RowRect(dx, dy, i - scroll_top);
        char name[SCN_NAME_BYTES];
        char desc[SCN_DESC_BYTES];
        snprintf(name, sizeof(name), "%s", entries[i].name);
        snprintf(desc, sizeof(desc), "%s", entries[i].description);
        FitText(name, sizeof(name), text_w);
        FitText(desc, sizeof(desc), text_w);

        SDL_SetRenderDrawColor(renderer, 35, 45, 62, 255);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, 70, 90, 130, 255);
        SDL_RenderRect(renderer, &row);
        GUI_DrawText(renderer, (int)row.x + 10, (int)row.y + 4, name, col_gold);
        GUI_DrawText(renderer, (int)row.x + 10, (int)row.y + 21, desc, col_white);
    }

    DrawNavButton(renderer, UpRect(dx, dy), "Up", scroll_top > 0);
    DrawNavButton(renderer, DownRect(dx, dy), "Down", scroll_top < MaxScroll());
    DrawNavButton(renderer, BackRect(dx, dy), "Back", true);

    char page[48];
    snprintf(page, sizeof(page), "%d-%d of %d", entry_count > 0 ? scroll_top + 1 : 0, last, entry_count);
    GUI_DrawText(renderer, (int)dx + 232, (int)dy + 421, page, col_gray);
}

bool ScnList_HandleClick(float mx, float my, float dx, float dy)
{
    if (PointIn(UpRect(dx, dy), mx, my)) {
        if (scroll_top > 0) scroll_top--;
        return true;
    }
    if (PointIn(DownRect(dx, dy), mx, my)) {
        if (scroll_top < MaxScroll()) scroll_top++;
        return true;
    }
    if (PointIn(BackRect(dx, dy), mx, my)) {
        ScnList_Close();
        return true;
    }

    for (int i = 0; i < SCN_LIST_VISIBLE; i++) {
        int index = scroll_top + i;
        if (index >= entry_count) break;
        if (PointIn(RowRect(dx, dy, i), mx, my)) {
            GUI_Dialogs_OpenChooseCity(ScnListConfirmed, index);
            return true;
        }
    }
    return true;
}
