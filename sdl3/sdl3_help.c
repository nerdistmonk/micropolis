/* sdl3_help.c: Context help mode. F1 turns on click-to-explain; the popup wraps text to a fixed width.
 */

#include "sdl3_help.h"
#include "sdl3_gui_text.h"
#include "sdl3_video.h"
#include <stdio.h>
#include <string.h>

#define HELP_TEXT_WIDTH 240
#define HELP_PAD 10
#define HELP_MAX_LINES 12
#define HELP_LINE_LEN 128
#define HELP_OFFSET 12

typedef struct {
    const char *title;
    const char *body;
} HelpEntry;

static const HelpEntry help_entries[HELP_TOPIC_COUNT] = {
    [HELP_NONE] = { NULL, NULL },
    [HELP_MAP] = { "Map", "Left-click or drag to apply the selected tool. Right-drag or the arrow keys pan the view. Click the radar map to jump to a spot." },
    [HELP_FUNDS] = { "Treasury", "Current city funds. Click to open the Budget tab for the annual budget and tax rates." },
    [HELP_CITY] = { "City Info", "Population and city statistics. Click to open the City dialog." },
    [HELP_OVERLAYS] = { "Overlays", "Color the map by power, traffic, pollution, crime, land value, population density, growth, or fire and police coverage. Click to choose a view." },
    [HELP_DISASTERS] = { "Disasters", "Trigger a disaster by hand: firestorm, flood, earthquake, tornado, monster, or meltdown. Spontaneous disasters can be turned off in the File dialog Options tab." },
    [HELP_FILE] = { "File", "Save, load, and manage cities and scenarios. F7 quicksaves and F8 quickloads." },
    [HELP_SPEED] = { "Simulation Speed", "Click to cycle through the speed settings. Space pauses or resumes. Keys 1, 2, and 3 set the speed directly." },
    [HELP_TICKER] = { "News Ticker", "The latest city message. Click the box to dismiss it." },
    [HELP_QUERY] = { "Query Tool", "Click a tile to inspect its zone, power, pollution, land value, and crime." },
    [HELP_RCI] = { "RCI Demand", "Residential, commercial, and industrial demand. A bar above the center means that zone type is in demand." },
    [HELP_MINIMAP] = { "Radar Map", "An overview of the whole city. Click to center the main view on that spot." },
    [HELP_TOOL_RESIDENTIAL] = { "Residential Zone", "Place housing. Zones need power and road access to grow." },
    [HELP_TOOL_COMMERCIAL] = { "Commercial Zone", "Place shops and offices. Zones need power and road access to grow." },
    [HELP_TOOL_INDUSTRIAL] = { "Industrial Zone", "Place factories. Creates jobs and some pollution." },
    [HELP_TOOL_FIRE] = { "Fire Station", "Covers the nearby area against fires." },
    [HELP_TOOL_POLICE] = { "Police Department", "Covers the nearby area against crime." },
    [HELP_TOOL_WIRE] = { "Power Lines", "Carry power from plants to zones and buildings." },
    [HELP_TOOL_DOZER] = { "Bulldozer", "Clears land, roads, rail, and buildings." },
    [HELP_TOOL_ROAD] = { "Roadway", "Lays road for traffic and access." },
    [HELP_TOOL_RAIL] = { "Railroad", "Lays track for rail transit and access." },
    [HELP_TOOL_COAL] = { "Coal Power Plant", "Generates power and produces pollution." },
    [HELP_TOOL_NUCLEAR] = { "Nuclear Power Plant", "Generates power with little pollution. Carries a meltdown risk." },
    [HELP_TOOL_STADIUM] = { "Sports Stadium", "Adds entertainment and jobs to nearby areas." },
    [HELP_TOOL_SEAPORT] = { "Seaport", "Adds shipping and jobs." },
    [HELP_TOOL_AIRPORT] = { "Municipal Airport", "Adds air travel and jobs." }
};

static const SDL_Color help_title_color = { 255, 215, 90, 255 };
static const SDL_Color help_body_color = { 235, 235, 240, 255 };
static const SDL_Color help_bg_color = { 20, 28, 48, 255 };

static bool mode_active = false;
static HelpTopic popup_topic = HELP_NONE;
static float popup_x = 0.0f;
static float popup_y = 0.0f;
static char popup_lines[HELP_MAX_LINES][HELP_LINE_LEN];
static int popup_line_count = 0;
static SDL_Cursor *default_cursor = NULL;
static SDL_Cursor *help_cursor = NULL;

static int WrapBody(const char *text)
{
    int count = 0;
    char current[HELP_LINE_LEN] = "";
    const char *p = text;

    while (*p && count < HELP_MAX_LINES) {
        char word[HELP_LINE_LEN] = "";
        size_t wl = 0;
        while (*p && *p != ' ' && wl < sizeof(word) - 1) word[wl++] = *p++;
        word[wl] = '\0';
        while (*p == ' ') p++;

        char candidate[2 * HELP_LINE_LEN];
        if (current[0]) {
            snprintf(candidate, sizeof(candidate), "%s %s", current, word);
        } else {
            snprintf(candidate, sizeof(candidate), "%s", word);
        }

        if (current[0] && GUI_GetTextWidth(candidate) > HELP_TEXT_WIDTH) {
            snprintf(popup_lines[count], sizeof(popup_lines[0]), "%s", current);
            count++;
            snprintf(current, sizeof(current), "%s", word);
        } else {
            snprintf(current, sizeof(current), "%.*s", HELP_LINE_LEN - 1, candidate);
        }
    }

    if (current[0] && count < HELP_MAX_LINES) {
        snprintf(popup_lines[count], sizeof(popup_lines[0]), "%s", current);
        count++;
    }
    return count;
}

static void SetMode(bool on)
{
    mode_active = on;
    if (!on) popup_topic = HELP_NONE;
    if (default_cursor) SDL_SetCursor(on && help_cursor ? help_cursor : default_cursor);
}

static void OpenPopup(HelpTopic topic, float x, float y)
{
    popup_topic = topic;
    popup_x = x;
    popup_y = y;
    popup_line_count = WrapBody(help_entries[topic].body);
}

void Help_Init(void)
{
    default_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    help_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    mode_active = false;
    popup_topic = HELP_NONE;
}

void Help_Shutdown(void)
{
    if (help_cursor) SDL_DestroyCursor(help_cursor);
    if (default_cursor) SDL_DestroyCursor(default_cursor);
    help_cursor = NULL;
    default_cursor = NULL;
}

bool Help_IsModeActive(void)
{
    return mode_active;
}

bool Help_HandleKey(SDL_Keycode key)
{
    if (key == SDLK_F1) {
        SetMode(!mode_active);
        return true;
    }
    if (mode_active && key == SDLK_ESCAPE) {
        SetMode(false);
        return true;
    }
    return false;
}

bool Help_HandleClick(float mouse_x, float mouse_y, HelpTopic topic)
{
    if (!mode_active) return false;

    if (popup_topic != HELP_NONE) {
        popup_topic = HELP_NONE;
        return true;
    }
    if (topic != HELP_NONE) {
        OpenPopup(topic, mouse_x, mouse_y);
    }
    return true;
}

static void RenderPopup(SDL_Renderer *renderer)
{
    const int line_h = GUI_GetLineHeight();
    const int title_h = line_h;
    const float box_w = (float)(HELP_TEXT_WIDTH + 2 * HELP_PAD);
    const float box_h = (float)(2 * HELP_PAD + title_h + 6 + popup_line_count * line_h);

    float x = popup_x + HELP_OFFSET;
    float y = popup_y + HELP_OFFSET;
    if (x + box_w > (float)WINDOW_DEFAULT_WIDTH) x = popup_x - HELP_OFFSET - box_w;
    if (y + box_h > (float)WINDOW_DEFAULT_HEIGHT) y = popup_y - HELP_OFFSET - box_h;
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;

    SDL_FRect box = { x, y, box_w, box_h };
    SDL_SetRenderDrawColor(renderer, help_bg_color.r, help_bg_color.g, help_bg_color.b, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, help_title_color.r, help_title_color.g, help_title_color.b, 255);
    SDL_RenderRect(renderer, &box);

    GUI_DrawText(renderer, (int)x + HELP_PAD, (int)y + HELP_PAD, help_entries[popup_topic].title, help_title_color);
    int text_y = (int)y + HELP_PAD + title_h + 6;
    for (int i = 0; i < popup_line_count; i++) {
        GUI_DrawText(renderer, (int)x + HELP_PAD, text_y + i * line_h, popup_lines[i], help_body_color);
    }
}

static void RenderBanner(SDL_Renderer *renderer)
{
    const char *text = "HELP MODE: click a control for help. F1 or Esc exits.";
    const float x = 300.0f;
    const float y = (float)WINDOW_DEFAULT_HEIGHT - 26.0f;
    SDL_FRect bg = { x, y, (float)GUI_GetTextWidth(text) + 16.0f, 20.0f };

    SDL_SetRenderDrawColor(renderer, help_bg_color.r, help_bg_color.g, help_bg_color.b, 255);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, help_title_color.r, help_title_color.g, help_title_color.b, 255);
    SDL_RenderRect(renderer, &bg);
    GUI_DrawText(renderer, (int)x + 8, (int)y + 3, text, help_title_color);
}

void Help_Render(SDL_Renderer *renderer)
{
    if (!mode_active) return;
    RenderBanner(renderer);
    if (popup_topic != HELP_NONE) {
        RenderPopup(renderer);
    }
}
