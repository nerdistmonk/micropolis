/* sdl3_dialog_overlays.c: Map Overlays & Survey Layers modal.
 */

#include "sdl3_dialog_common.h"

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

    struct {
        OverlayViewMode mode;
        const char *name;
        SDL_FRect rect;
    } overlay_btns[] = {
        { OVERLAY_VIEW_NORMAL, "Normal View", { dx + 30.0f, dy + 60.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_POWER, "Power Grid", { dx + 230.0f, dy + 60.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_TRAFFIC, "Traffic Density", { dx + 30.0f, dy + 115.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_POLLUTION, "Pollution Map", { dx + 230.0f, dy + 115.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_CRIME, "Crime Rate", { dx + 30.0f, dy + 170.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_LAND_VALUE, "Land Value", { dx + 230.0f, dy + 170.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_POP_DENSITY, "Pop Density", { dx + 30.0f, dy + 225.0f, 180.0f, 36.0f } }
    };

    for (size_t i = 0; i < sizeof(overlay_btns) / sizeof(overlay_btns[0]); i++) {
        SDL_FRect r = overlay_btns[i].rect;
        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
            GUI_Dialogs_SetActiveOverlay(overlay_btns[i].mode);
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
    float dh = 360.0f;

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

    struct {
        OverlayViewMode mode;
        const char *name;
        SDL_FRect rect;
    } overlay_btns[] = {
        { OVERLAY_VIEW_NORMAL, "Normal View", { dx + 30.0f, dy + 60.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_POWER, "Power Grid", { dx + 230.0f, dy + 60.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_TRAFFIC, "Traffic Density", { dx + 30.0f, dy + 115.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_POLLUTION, "Pollution Map", { dx + 230.0f, dy + 115.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_CRIME, "Crime Rate", { dx + 30.0f, dy + 170.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_LAND_VALUE, "Land Value", { dx + 230.0f, dy + 170.0f, 180.0f, 36.0f } },
        { OVERLAY_VIEW_POP_DENSITY, "Pop Density", { dx + 30.0f, dy + 225.0f, 180.0f, 36.0f } }
    };

    OverlayViewMode active_overlay = GUI_Dialogs_GetActiveOverlay();

    for (size_t i = 0; i < sizeof(overlay_btns) / sizeof(overlay_btns[0]); i++) {
        bool selected = (active_overlay == overlay_btns[i].mode);
        SDL_FRect r = overlay_btns[i].rect;

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

        GUI_DrawText(renderer, (int)r.x + 16, (int)r.y + 10, overlay_btns[i].name, selected ? col_gold : col_white);
    }

    char buf[128];
    static const char *layer_names[] = {
        "Normal View", "Power Grid", "Traffic Density",
        "Pollution Map", "Crime Rate", "Land Value", "Pop Density"
    };
    snprintf(buf, sizeof(buf), "Active Layer: [%s]", layer_names[active_overlay]);
    GUI_DrawText(renderer, (int)dx + 30, (int)dy + 300, buf, col_cyan);
}