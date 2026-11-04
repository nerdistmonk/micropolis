/* sdl3_dialog_disasters.c: Disaster activation trigger menu.
 */

#include "sdl3_dialog_common.h"

void Dialog_Disasters_Open(void)
{
}

bool Dialog_Disasters_HandleClick(float mx, float my)
{
    float dx = 300.0f;
    float dy = 160.0f;
    float dw = 424.0f;

    if (mx >= dx + dw - 28.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 28.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    struct {
        void (*fn)(void);
        SDL_FRect rect;
    } d_btns[] = {
        { MakeFire, { dx + 30.0f, dy + 65.0f, 170.0f, 34.0f } },
        { MakeFlood, { dx + 224.0f, dy + 65.0f, 170.0f, 34.0f } },
        { MakeEarthquake, { dx + 30.0f, dy + 115.0f, 170.0f, 34.0f } },
        { MakeTornado, { dx + 224.0f, dy + 115.0f, 170.0f, 34.0f } },
        { MakeMonster, { dx + 30.0f, dy + 165.0f, 170.0f, 34.0f } },
        { MakeMeltdown, { dx + 224.0f, dy + 165.0f, 170.0f, 34.0f } }
    };

    for (size_t i = 0; i < sizeof(d_btns) / sizeof(d_btns[0]); i++) {
        SDL_FRect r = d_btns[i].rect;
        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
            d_btns[i].fn();
            GUI_Dialogs_Close();
            return true;
        }
    }

    return true;
}

void Dialog_Disasters_Render(SDL_Renderer *renderer)
{
    float dx = 300.0f;
    float dy = 160.0f;
    float dw = 424.0f;
    float dh = 240.0f;

    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 30, 22, 26, 252);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, 220, 70, 70, 255);
    SDL_RenderRect(renderer, &win_rect);

    GUI_DrawText(renderer, (int)dx + 16, (int)dy + 16, "DISASTER ACTIVATION MENU", col_gold);

    SDL_FRect close_rect = { dx + dw - 28.0f, dy + 8.0f, 20.0f, 20.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 6, (int)close_rect.y + 4, "X", col_white);

    struct {
        const char *name;
        SDL_FRect rect;
    } d_display[] = {
        { "Firestorm (F1)", { dx + 30.0f, dy + 65.0f, 170.0f, 34.0f } },
        { "Flood Waters (F2)", { dx + 224.0f, dy + 65.0f, 170.0f, 34.0f } },
        { "Earthquake (F3)", { dx + 30.0f, dy + 115.0f, 170.0f, 34.0f } },
        { "Tornado (F4)", { dx + 224.0f, dy + 115.0f, 170.0f, 34.0f } },
        { "Monster (F5)", { dx + 30.0f, dy + 165.0f, 170.0f, 34.0f } },
        { "Meltdown (F6)", { dx + 224.0f, dy + 165.0f, 170.0f, 34.0f } }
    };

    for (size_t i = 0; i < sizeof(d_display) / sizeof(d_display[0]); i++) {
        SDL_FRect r = d_display[i].rect;
        SDL_SetRenderDrawColor(renderer, 48, 24, 28, 255);
        SDL_RenderFillRect(renderer, &r);

        SDL_SetRenderDrawColor(renderer, 180, 60, 70, 255);
        SDL_RenderRect(renderer, &r);

        GUI_DrawText(renderer, (int)r.x + 14, (int)r.y + 8, d_display[i].name, col_white);
    }
}