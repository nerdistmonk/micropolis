/* sdl3_dialog_quit.c: Quit confirmation shown on Escape or window close.
 */

#include "sdl3_dialog_common.h"

#define QUIT_W 440.0f
#define QUIT_H 180.0f
#define QUIT_X ((Video_GetCanvasWidth() - QUIT_W) / 2.0f)
#define QUIT_Y ((Video_GetCanvasHeight() - QUIT_H) / 2.0f)
#define QUIT_BTN_H 32.0f
#define QUIT_BTN_PAD 16.0f
#define QUIT_BTN_GAP 20.0f

enum { QUIT_BTN_SAVE = 0, QUIT_BTN_QUIT, QUIT_BTN_CANCEL, QUIT_BTN_COUNT };
enum { QUIT_NAME_BTN_SAVE = 0, QUIT_NAME_BTN_BACK, QUIT_NAME_BTN_COUNT };

static const char *const quit_labels[QUIT_BTN_COUNT] = { "Save & Quit", "Quit", "Cancel" };
static const char *const quit_name_labels[QUIT_NAME_BTN_COUNT] = { "Save", "Back" };

static bool quit_confirmed = false;

static SDL_FRect QuitButtonRect(const char *const *labels, int count, int index)
{
    float row_w = (float)(count - 1) * QUIT_BTN_GAP;
    for (int i = 0; i < count; i++) row_w += GUI_ButtonWidth(labels[i], QUIT_BTN_PAD);

    float x = QUIT_X + (QUIT_W - row_w) / 2.0f;
    for (int i = 0; i < index; i++) x += GUI_ButtonWidth(labels[i], QUIT_BTN_PAD) + QUIT_BTN_GAP;

    SDL_FRect r = { x, QUIT_Y + QUIT_H - 46.0f, GUI_ButtonWidth(labels[index], QUIT_BTN_PAD), QUIT_BTN_H };
    return r;
}

void Dialog_Quit_Show(void)
{
    CitySave_Cancel();
}

bool Dialog_Quit_TakeConfirmed(void)
{
    bool confirmed = quit_confirmed;
    quit_confirmed = false;
    return confirmed;
}

bool Dialog_Quit_HandleClick(float mx, float my)
{
    if (CitySave_IsActive()) {
        for (int i = 0; i < QUIT_NAME_BTN_COUNT; i++) {
            SDL_FRect r = QuitButtonRect(quit_name_labels, QUIT_NAME_BTN_COUNT, i);
            if (mx < r.x || mx > r.x + r.w || my < r.y || my > r.y + r.h) continue;

            if (i == QUIT_NAME_BTN_SAVE) {
                CitySave_Submit();
            } else {
                CitySave_Cancel();
            }
            return true;
        }
        return true;
    }

    for (int i = 0; i < QUIT_BTN_COUNT; i++) {
        SDL_FRect r = QuitButtonRect(quit_labels, QUIT_BTN_COUNT, i);
        if (mx < r.x || mx > r.x + r.w || my < r.y || my > r.y + r.h) continue;

        if (i == QUIT_BTN_CANCEL) {
            GUI_Dialogs_Close();
        } else if (i == QUIT_BTN_QUIT) {
            quit_confirmed = true;
            GUI_Dialogs_Close();
        } else {
            CitySave_Begin(CITY_SAVE_FROM_QUIT);
        }
        return true;
    }
    return true;
}

/* Called after a city has been saved from the quit dialog. */
void Dialog_Quit_SaveFinished(void)
{
    quit_confirmed = true;
    GUI_Dialogs_Close();
}

void Dialog_Quit_Render(SDL_Renderer *renderer)
{
    SDL_FRect scrim = { 0.0f, 0.0f, (float)Video_GetCanvasWidth(), (float)Video_GetCanvasHeight() };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 210);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect box = { QUIT_X, QUIT_Y, QUIT_W, QUIT_H };
    SDL_SetRenderDrawColor(renderer, 20, 24, 34, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &box);

    int tx = (int)QUIT_X + 20;
    int ty = (int)QUIT_Y + 16;

    if (CitySave_IsActive()) {
        GUI_DrawText(renderer, tx, ty, "SAVE CITY BEFORE QUITTING", col_gold);
        GUI_DrawText(renderer, tx, ty + 28, "City name:", col_white);

        SDL_FRect field = { QUIT_X + 20.0f, QUIT_Y + 64.0f, QUIT_W - 40.0f, 28.0f };
        CitySave_DrawField(renderer, field);

        SDL_Color status_color;
        const char *status = CityIO_Status(&status_color);
        if (status[0] != '\0') {
            GUI_DrawText(renderer, tx, (int)QUIT_Y + 100, status, status_color);
        }

        for (int i = 0; i < QUIT_NAME_BTN_COUNT; i++) {
            SDL_FRect r = QuitButtonRect(quit_name_labels, QUIT_NAME_BTN_COUNT, i);
            SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
            SDL_RenderFillRect(renderer, &r);
            SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
            SDL_RenderRect(renderer, &r);
            GUI_DrawTextCentered(renderer, r, quit_name_labels[i], col_white);
        }
        return;
    }

    GUI_DrawText(renderer, tx, ty + 4, "QUIT MICROPOLIS?", col_gold);
    GUI_DrawText(renderer, tx, ty + 44, "Save your city before quitting?", col_white);
    GUI_DrawText(renderer, tx, ty + 68, "Quitting without saving loses your progress.", col_white);

    for (int i = 0; i < QUIT_BTN_COUNT; i++) {
        SDL_FRect r = QuitButtonRect(quit_labels, QUIT_BTN_COUNT, i);
        SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
        SDL_RenderRect(renderer, &r);
        GUI_DrawTextCentered(renderer, r, quit_labels[i], col_white);
    }
}
