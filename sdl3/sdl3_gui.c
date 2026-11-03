/* sdl3_gui.c: Main HUD orchestrator, toolbar icons, and floating tooltips.
 */

#include "sdl3_gui.h"
#include "sdl3_gui_text.h"
#include "sdl3_gui_dialogs.h"
#include "sdl3_input.h"
#include "sdl3_video.h"
#include "sim.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int16_t tool_id;
    const char *full_name;
    const char *icon_stem;
    int32_t cost;
    SDL_FRect rect;
    SDL_Texture *tex_normal;
    SDL_Texture *tex_hi;
} GuiToolButton;

static GuiToolButton tool_buttons[] = {
    { residentialState, "Residential Zone",     "icres",  100,   { 6.0f,  34.0f, 46.0f, 46.0f }, NULL, NULL },
    { commercialState,  "Commercial Zone",      "iccom",  100,   { 56.0f, 34.0f, 46.0f, 46.0f }, NULL, NULL },
    { industrialState,  "Industrial Zone",      "icind",  100,   { 6.0f,  84.0f, 46.0f, 46.0f }, NULL, NULL },
    { fireState,        "Fire Station",         "icfire", 500,   { 56.0f, 84.0f, 46.0f, 46.0f }, NULL, NULL },
    { policeState,      "Police Department",    "icpol",  500,   { 6.0f,  134.0f, 46.0f, 46.0f }, NULL, NULL },
    { wireState,        "Power Lines",          "icwire", 5,     { 56.0f, 134.0f, 46.0f, 46.0f }, NULL, NULL },
    { dozeState,        "Bulldozer",            "icdozr", 1,     { 6.0f,  184.0f, 46.0f, 46.0f }, NULL, NULL },
    { roadState,        "Roadway",              "icroad", 10,    { 56.0f, 184.0f, 46.0f, 46.0f }, NULL, NULL },
    { rrState,          "Railroad",             "icrail", 20,    { 6.0f,  234.0f, 46.0f, 46.0f }, NULL, NULL },
    { powerState,       "Coal Power Plant",     "iccoal", 3000,  { 56.0f, 234.0f, 46.0f, 46.0f }, NULL, NULL },
    { nuclearState,     "Nuclear Power Plant",  "icnuc",  5000,  { 6.0f,  284.0f, 46.0f, 46.0f }, NULL, NULL },
    { stadiumState,     "Sports Stadium",       "icstad", 5000,  { 56.0f, 284.0f, 46.0f, 46.0f }, NULL, NULL },
    { seaportState,     "Seaport",              "icseap", 3000,  { 6.0f,  334.0f, 46.0f, 46.0f }, NULL, NULL },
    { airportState,     "Municipal Airport",    "icairp", 10000, { 56.0f, 334.0f, 46.0f, 46.0f }, NULL, NULL }
};

#define NUM_TOOL_BUTTONS (sizeof(tool_buttons) / sizeof(tool_buttons[0]))

static int hovered_tool_idx = -1;
static float cursor_x = 0.0f;
static float cursor_y = 0.0f;

static const SDL_FRect rect_funds_btn = { 135.0f, 2.0f, 150.0f, 24.0f };
static const SDL_FRect rect_city_btn  = { 290.0f, 2.0f, 260.0f, 24.0f };
static const SDL_FRect rect_speed_btn = { 880.0f, 2.0f, 136.0f, 24.0f };

static const SDL_Color col_white = { 245, 245, 250, 255 };
static const SDL_Color col_gold  = { 255, 215, 0, 255 };
static const SDL_Color col_green = { 100, 255, 120, 255 };
static const SDL_Color col_cyan  = { 90, 210, 255, 255 };

static void LoadToolButtonIcons(void)
{
    const char *dirs[] = { "images/", "../images/", "../../images/", "" };
    char path[512];

    for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
        GuiToolButton *btn = &tool_buttons[i];

        for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
            snprintf(path, sizeof(path), "%s%s.xpm", dirs[d], btn->icon_stem);
            btn->tex_normal = Video_LoadTexture(path, NULL, NULL);
            if (btn->tex_normal) break;
        }

        for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
            snprintf(path, sizeof(path), "%s%shi.xpm", dirs[d], btn->icon_stem);
            btn->tex_hi = Video_LoadTexture(path, NULL, NULL);
            if (btn->tex_hi) break;
        }
    }
}

bool GUI_Init(void)
{
    GUI_Text_Init();
    LoadToolButtonIcons();
    hovered_tool_idx = -1;
    return true;
}

void GUI_Shutdown(void)
{
    GUI_Dialogs_Close();

    for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
        if (tool_buttons[i].tex_normal) {
            SDL_DestroyTexture(tool_buttons[i].tex_normal);
            tool_buttons[i].tex_normal = NULL;
        }
        if (tool_buttons[i].tex_hi) {
            SDL_DestroyTexture(tool_buttons[i].tex_hi);
            tool_buttons[i].tex_hi = NULL;
        }
    }

    GUI_Text_Shutdown();
}

bool GUI_IsModalOpen(void)
{
    return GUI_Dialogs_IsModalOpen();
}

void GUI_OpenCityMgmtModal(CityMgmtTab initial_tab)
{
    GUI_Dialogs_OpenCityMgmt(initial_tab);
}

void GUI_OpenOverlaysModal(void)
{
    GUI_Dialogs_OpenOverlays();
}

void GUI_CloseModal(void)
{
    GUI_Dialogs_Close();
}

OverlayViewMode GUI_GetActiveOverlayMode(void)
{
    return GUI_Dialogs_GetActiveOverlay();
}

void GUI_SetActiveOverlayMode(OverlayViewMode mode)
{
    GUI_Dialogs_SetActiveOverlay(mode);
}

void GUI_HandleMotion(float mouse_x, float mouse_y)
{
    cursor_x = mouse_x;
    cursor_y = mouse_y;

    if (GUI_IsModalOpen()) {
        hovered_tool_idx = -1;
        return;
    }

    hovered_tool_idx = -1;
    if (mouse_x <= (float)TOOLBAR_WIDTH && mouse_y > (float)STATUS_BAR_HEIGHT) {
        for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
            SDL_FRect r = tool_buttons[i].rect;
            if (mouse_x >= r.x && mouse_x <= (r.x + r.w) &&
                mouse_y >= r.y && mouse_y <= (r.y + r.h)) {
                hovered_tool_idx = (int)i;
                break;
            }
        }
    }
}

bool GUI_HandleTextInput(const char *text)
{
    return GUI_Dialogs_HandleTextInput(text);
}

bool GUI_HandleKeyDown(SDL_Keycode key)
{
    return GUI_Dialogs_HandleKeyDown(key);
}

bool GUI_HandleClick(float mouse_x, float mouse_y, uint8_t button)
{
    if (button != SDL_BUTTON_LEFT) return false;

    if (GUI_Dialogs_HandleClick(mouse_x, mouse_y)) {
        return true;
    }

    /* Radar Map Click */
    if (Video_HandleMinimapClick(mouse_x, mouse_y)) {
        return true;
    }

    /* Status Bar Buttons */
    if (mouse_y <= (float)STATUS_BAR_HEIGHT) {
        if (mouse_x >= rect_funds_btn.x && mouse_x <= (rect_funds_btn.x + rect_funds_btn.w)) {
            GUI_OpenCityMgmtModal(CITY_TAB_BUDGET);
            return true;
        }
        if (mouse_x >= rect_city_btn.x && mouse_x <= (rect_city_btn.x + rect_city_btn.w)) {
            GUI_OpenOverlaysModal();
            return true;
        }
        if (mouse_x >= rect_speed_btn.x && mouse_x <= (rect_speed_btn.x + rect_speed_btn.w)) {
            int16_t next_spd = (SimSpeed + 1) % 4;
            SetSimSpeed(next_spd);
            return true;
        }
        return true;
    }

    /* Tool Buttons */
    if (mouse_x <= (float)TOOLBAR_WIDTH) {
        for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
            SDL_FRect r = tool_buttons[i].rect;
            if (mouse_x >= r.x && mouse_x <= (r.x + r.w) &&
                mouse_y >= r.y && mouse_y <= (r.y + r.h)) {
                Input_SetActiveTool(tool_buttons[i].tool_id);
                return true;
            }
        }
        return true;
    }

    return false;
}

static void RenderTooltip(SDL_Renderer *renderer, int tool_idx)
{
    if (tool_idx < 0 || tool_idx >= (int)NUM_TOOL_BUTTONS) return;

    GuiToolButton *btn = &tool_buttons[tool_idx];
    char text[96];
    if (btn->cost > 0) {
        snprintf(text, sizeof(text), "%s ($%d)", btn->full_name, btn->cost);
    } else {
        snprintf(text, sizeof(text), "%s (Free)", btn->full_name);
    }

    int text_w = GUI_GetTextWidth(text);
    int pad_x = 10;
    int box_w = text_w + (pad_x * 2);
    int box_h = 26;

    float tx = cursor_x + 14.0f;
    float ty = cursor_y + 10.0f;

    if (tx + (float)box_w > (float)WINDOW_DEFAULT_WIDTH) {
        tx = (float)WINDOW_DEFAULT_WIDTH - (float)box_w - 6.0f;
    }
    if (ty + (float)box_h > (float)WINDOW_DEFAULT_HEIGHT) {
        ty = cursor_y - (float)box_h - 4.0f;
    }

    SDL_FRect box = { tx, ty, (float)box_w, (float)box_h };
    SDL_SetRenderDrawColor(renderer, 15, 18, 26, 245);
    SDL_RenderFillRect(renderer, &box);

    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 220);
    SDL_RenderRect(renderer, &box);

    GUI_DrawText(renderer, (int)tx + pad_x, (int)ty + 5, text, col_white);
}

void GUI_Render(SDL_Renderer *renderer)
{
    if (!renderer) return;

    /* 1. Status Bar */
    SDL_FRect status_rect = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)STATUS_BAR_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 24, 28, 36, 245);
    SDL_RenderFillRect(renderer, &status_rect);

    SDL_SetRenderDrawColor(renderer, 60, 72, 92, 255);
    SDL_RenderLine(renderer, 0.0f, (float)STATUS_BAR_HEIGHT, (float)WINDOW_DEFAULT_WIDTH, (float)STATUS_BAR_HEIGHT);

    static const char *months[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    int32_t current_year = StartingYear + (CityTime / 48);
    int32_t current_month = (CityTime % 48) / 4;
    if (current_month < 0) current_month = 0;
    if (current_month > 11) current_month = 11;

    char date_buf[64];
    snprintf(date_buf, sizeof(date_buf), "%s %d", months[current_month], current_year);
    GUI_DrawText(renderer, 12, 6, date_buf, col_white);

    /* Funds */
    char funds_buf[64];
    snprintf(funds_buf, sizeof(funds_buf), "Funds: $%d", TotalFunds);
    SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
    SDL_RenderFillRect(renderer, &rect_funds_btn);
    SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
    SDL_RenderRect(renderer, &rect_funds_btn);
    GUI_DrawText(renderer, (int)rect_funds_btn.x + 8, (int)rect_funds_btn.y + 4, funds_buf, col_gold);

    /* City Name & Pop */
    char city_buf[96];
    snprintf(city_buf, sizeof(city_buf), "%s | Pop: %d", CityName, (int)TotalPop * 10);
    SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
    SDL_RenderFillRect(renderer, &rect_city_btn);
    SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
    SDL_RenderRect(renderer, &rect_city_btn);
    GUI_DrawText(renderer, (int)rect_city_btn.x + 8, (int)rect_city_btn.y + 4, city_buf, col_cyan);

    /* Speed indicator */
    const char *spd_label = "Speed: [Paused]";
    if (SimSpeed == 1) spd_label = "Speed: [Slow]";
    else if (SimSpeed == 2) spd_label = "Speed: [Med]";
    else if (SimSpeed == 3) spd_label = "Speed: [Fast]";

    SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
    SDL_RenderFillRect(renderer, &rect_speed_btn);
    SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
    SDL_RenderRect(renderer, &rect_speed_btn);
    GUI_DrawText(renderer, (int)rect_speed_btn.x + 8, (int)rect_speed_btn.y + 4, spd_label, (SimSpeed == 0) ? col_gold : col_green);

    /* Alerts */
    if (HaveLastMessage && LastMessage[0]) {
        GUI_DrawText(renderer, 560, 6, LastMessage, col_gold);
    }

    /* 2. Tool Palette sidebar */
    SDL_FRect tool_bg = { 0.0f, (float)STATUS_BAR_HEIGHT, (float)TOOLBAR_WIDTH, (float)(WINDOW_DEFAULT_HEIGHT - STATUS_BAR_HEIGHT) };
    SDL_SetRenderDrawColor(renderer, 20, 24, 30, 240);
    SDL_RenderFillRect(renderer, &tool_bg);

    SDL_SetRenderDrawColor(renderer, 60, 72, 92, 255);
    SDL_RenderLine(renderer, (float)TOOLBAR_WIDTH, (float)STATUS_BAR_HEIGHT, (float)TOOLBAR_WIDTH, (float)WINDOW_DEFAULT_HEIGHT);

    /* Tool Buttons */
    int16_t cur_tool = Input_GetActiveTool();

    for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
        GuiToolButton *btn = &tool_buttons[i];
        bool selected = (btn->tool_id == cur_tool);
        bool hovered = (hovered_tool_idx == (int)i);

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 48, 85, 140, 255);
        } else if (hovered) {
            SDL_SetRenderDrawColor(renderer, 42, 52, 70, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 30, 36, 48, 255);
        }
        SDL_RenderFillRect(renderer, &btn->rect);

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
        } else if (hovered) {
            SDL_SetRenderDrawColor(renderer, 120, 160, 220, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 72, 90, 255);
        }
        SDL_RenderRect(renderer, &btn->rect);

        SDL_Texture *icon = (selected || hovered) ? (btn->tex_hi ? btn->tex_hi : btn->tex_normal) : btn->tex_normal;
        if (icon) {
            SDL_FRect icon_dst = { btn->rect.x + 7.0f, btn->rect.y + 7.0f, 32.0f, 32.0f };
            SDL_RenderTexture(renderer, icon, NULL, &icon_dst);
        }
    }

    /* 3. Modal Dialogs */
    GUI_Dialogs_Render(renderer);

    /* 4. Floating Tooltip */
    if (hovered_tool_idx >= 0 && !GUI_IsModalOpen()) {
        RenderTooltip(renderer, hovered_tool_idx);
    }
}