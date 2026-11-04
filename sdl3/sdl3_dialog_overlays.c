/* sdl3_dialog_overlays.c: Map Overlays & Survey Layers modal.
 */

#include "sdl3_dialog_common.h"

#define OVERLAY_COLS 2

static const struct {
    OverlayViewMode mode;
    const char *name;
} overlay_choices[] = {
    { OVERLAY_VIEW_NORMAL, "Normal View" },
    { OVERLAY_VIEW_POWER, "Power Grid" },
    { OVERLAY_VIEW_TRAFFIC, "Traffic Density" },
    { OVERLAY_VIEW_POLLUTION, "Pollution Map" },
    { OVERLAY_VIEW_CRIME, "Crime Rate" },
    { OVERLAY_VIEW_LAND_VALUE, "Land Value" },
    { OVERLAY_VIEW_POP_DENSITY, "Pop Density" },
    { OVERLAY_VIEW_RATE_OF_GROWTH, "Growth Rate" },
    { OVERLAY_VIEW_FIRE_COVERAGE, "Fire Coverage" },
    { OVERLAY_VIEW_POLICE_COVERAGE, "Police Coverage" },
    { OVERLAY_VIEW_RES_ZONE, "Residential Zone" },
    { OVERLAY_VIEW_COM_ZONE, "Commercial Zone" },
    { OVERLAY_VIEW_IND_ZONE, "Industrial Zone" },
    { OVERLAY_VIEW_TRANSPORT, "Transportation" }
};

#define OVERLAY_CHOICE_COUNT (sizeof(overlay_choices) / sizeof(overlay_choices[0]))

static SDL_FRect OverlayButtonRect(size_t i)
{
    SDL_FRect r = {
        280.0f + 30.0f + (float)(i % OVERLAY_COLS) * 200.0f,
        150.0f + 60.0f + (float)(i / OVERLAY_COLS) * 45.0f,
        180.0f,
        36.0f
    };
    return r;
}

static const char *OverlayName(OverlayViewMode mode)
{
    for (size_t i = 0; i < OVERLAY_CHOICE_COUNT; i++) {
        if (overlay_choices[i].mode == mode) return overlay_choices[i].name;
    }
    return "Normal View";
}

void Dialog_Overlays_Open(void)
{
}

bool Dialog_Overlays_HandleClick(float mx, float my)
{
    float dx = 280.0f;
    float dy = 150.0f;
    float dw = 460.0f;

    if (mx >= dx + dw - 32.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 32.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    for (size_t i = 0; i < OVERLAY_CHOICE_COUNT; i++) {
        SDL_FRect r = OverlayButtonRect(i);
        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
            GUI_Dialogs_SetActiveOverlay(overlay_choices[i].mode);
            GUI_Dialogs_Close();
            return true;
        }
    }

    return true;
}

void Dialog_Overlays_Render(SDL_Renderer *renderer)
{
    float dx = 280.0f;
    float dy = 150.0f;
    float dw = 460.0f;
    float dh = 400.0f;

    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 24, 28, 38, 250);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
    SDL_RenderRect(renderer, &win_rect);

    GUI_DrawText(renderer, (int)dx + 16, (int)dy + 16, "MAP OVERLAYS & SURVEY LAYERS", col_gold);

    SDL_FRect close_rect = { dx + dw - 28.0f, dy + 8.0f, 20.0f, 20.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 6, (int)close_rect.y + 4, "X", col_white);

    OverlayViewMode active_overlay = GUI_Dialogs_GetActiveOverlay();

    for (size_t i = 0; i < OVERLAY_CHOICE_COUNT; i++) {
        bool selected = (active_overlay == overlay_choices[i].mode);
        SDL_FRect r = OverlayButtonRect(i);

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 48, 85, 140, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 32, 38, 50, 255);
        }
        SDL_RenderFillRect(renderer, &r);

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 65, 78, 100, 255);
        }
        SDL_RenderRect(renderer, &r);

        GUI_DrawText(renderer, (int)r.x + 16, (int)r.y + 10, overlay_choices[i].name, selected ? col_gold : col_white);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Active Layer: [%s]", OverlayName(active_overlay));
    GUI_DrawText(renderer, (int)dx + 30, (int)dy + 378, buf, col_cyan);
}