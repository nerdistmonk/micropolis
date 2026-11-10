/* sdl3_dialog_overlays.c: Map Overlays & Survey Layers modal.
 */

#include "sdl3_dialog_common.h"

#define OVERLAY_COLS 2
#define OVERLAY_BTN_PAD 16.0f
#define OVERLAY_BTN_GAP 20.0f
#define OVERLAY_DLG_W 460.0f
#define OVERLAY_BTN_TOP 60.0f
#define OVERLAY_ROW_PITCH 45.0f
#define OVERLAY_BTN_H 36.0f

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
    { OVERLAY_VIEW_HEALTH_COVERAGE, "Healthcare Coverage" },
    { OVERLAY_VIEW_RECREATION_COVERAGE, "Recreation Coverage" },
    { OVERLAY_VIEW_WATER_SUPPLY, "Water Supply" },
    { OVERLAY_VIEW_RES_ZONE, "Residential Zone" },
    { OVERLAY_VIEW_COM_ZONE, "Commercial Zone" },
    { OVERLAY_VIEW_IND_ZONE, "Industrial Zone" },
    { OVERLAY_VIEW_TRANSPORT, "Transportation" },
    { OVERLAY_VIEW_UNDERGROUND, "Underground Pipes" }
};

#define OVERLAY_CHOICE_COUNT (sizeof(overlay_choices) / sizeof(overlay_choices[0]))
#define OVERLAY_ROWS ((OVERLAY_CHOICE_COUNT + OVERLAY_COLS - 1) / OVERLAY_COLS)
#define OVERLAY_FOOTER_Y (OVERLAY_BTN_TOP + (float)OVERLAY_ROWS * OVERLAY_ROW_PITCH + 12.0f)
#define OVERLAY_DLG_H (OVERLAY_FOOTER_Y + 36.0f)

static float OverlayColumnWidth(size_t col)
{
    float w = 0.0f;
    for (size_t i = col; i < OVERLAY_CHOICE_COUNT; i += OVERLAY_COLS) {
        float bw = GUI_ButtonWidth(overlay_choices[i].name, OVERLAY_BTN_PAD);
        if (bw > w) w = bw;
    }
    return w;
}

static SDL_FRect OverlayButtonRect(float dx, float dy, size_t i)
{
    size_t col = i % OVERLAY_COLS;
    float grid_w = OverlayColumnWidth(0) + OVERLAY_BTN_GAP + OverlayColumnWidth(1);
    float x = dx + (OVERLAY_DLG_W - grid_w) / 2.0f;
    if (col == 1) x += OverlayColumnWidth(0) + OVERLAY_BTN_GAP;

    SDL_FRect r = {
        x,
        dy + OVERLAY_BTN_TOP + (float)(i / OVERLAY_COLS) * OVERLAY_ROW_PITCH,
        OverlayColumnWidth(col),
        OVERLAY_BTN_H
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

bool Dialog_Overlays_HandleClick(float mx, float my)
{
    float dw = OVERLAY_DLG_W;
    float dx = DialogCenteredX(dw);
    float dy = DialogCenteredY(OVERLAY_DLG_H);

    if (mx >= dx + dw - 32.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 32.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    for (size_t i = 0; i < OVERLAY_CHOICE_COUNT; i++) {
        SDL_FRect r = OverlayButtonRect(dx, dy, i);
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
    float dw = OVERLAY_DLG_W;
    float dh = OVERLAY_DLG_H;
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

    GUI_DrawText(renderer, (int)dx + 16, (int)dy + 16, "MAP OVERLAYS & SURVEY LAYERS", col_gold);

    SDL_FRect close_rect = { dx + dw - 28.0f, dy + 8.0f, 20.0f, 20.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 6, (int)close_rect.y + 4, "X", col_white);

    OverlayViewMode active_overlay = GUI_Dialogs_GetActiveOverlay();

    for (size_t i = 0; i < OVERLAY_CHOICE_COUNT; i++) {
        bool selected = (active_overlay == overlay_choices[i].mode);
        SDL_FRect r = OverlayButtonRect(dx, dy, i);

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

        GUI_DrawTextCentered(renderer, r, overlay_choices[i].name, selected ? col_gold : col_white);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Active Layer: [%s]", OverlayName(active_overlay));
    GUI_DrawText(renderer, (int)dx + 30, (int)dy + (int)OVERLAY_FOOTER_Y, buf, col_cyan);
}