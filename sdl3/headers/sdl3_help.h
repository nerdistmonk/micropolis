/* sdl3_help.h: Context help mode (F1). Click a control to see a short explanation.
 */

#ifndef SDL3_HELP_H
#define SDL3_HELP_H

#include <stdbool.h>
#include <SDL3/SDL.h>

/* HELP_TOOL_RESIDENTIAL..HELP_TOOL_PARK must stay in the same order as tool_buttons in sdl3_gui.c */
typedef enum {
    HELP_NONE = 0,
    HELP_MAP,
    HELP_FUNDS,
    HELP_CITY,
    HELP_OVERLAYS,
    HELP_DISASTERS,
    HELP_FILE,
    HELP_SPEED,
    HELP_TICKER,
    HELP_QUERY,
    HELP_RCI,
    HELP_MINIMAP,
    HELP_TOOL_RESIDENTIAL,
    HELP_TOOL_COMMERCIAL,
    HELP_TOOL_INDUSTRIAL,
    HELP_TOOL_FIRE,
    HELP_TOOL_POLICE,
    HELP_TOOL_WIRE,
    HELP_TOOL_DOZER,
    HELP_TOOL_ROAD,
    HELP_TOOL_RAIL,
    HELP_TOOL_COAL,
    HELP_TOOL_NUCLEAR,
    HELP_TOOL_STADIUM,
    HELP_TOOL_SEAPORT,
    HELP_TOOL_AIRPORT,
    HELP_TOOL_PARK,
    HELP_TOOL_WATER_TOWER,
    HELP_TOOL_PIPE,
    HELP_TOOL_HOSPITAL,
    HELP_TOPIC_COUNT
} HelpTopic;

void Help_Init(void);
void Help_Shutdown(void);
bool Help_IsModeActive(void);
bool Help_HandleKey(SDL_Keycode key);
bool Help_HandleClick(float mouse_x, float mouse_y, HelpTopic topic);
void Help_Render(SDL_Renderer *renderer);

#endif /* SDL3_HELP_H */
