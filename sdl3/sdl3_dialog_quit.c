/* sdl3_dialog_quit.c: Quit confirmation shown on Escape or window close.
 */

#include "sdl3_dialog_common.h"

#define QUIT_W 440.0f
#define QUIT_H 180.0f
#define QUIT_X ((WINDOW_DEFAULT_WIDTH - QUIT_W) / 2.0f)
#define QUIT_Y 200.0f
#define QUIT_BTN_W 120.0f
#define QUIT_BTN_H 32.0f
#define QUIT_BTN_STEP 140.0f

enum { QUIT_BTN_SAVE = 0, QUIT_BTN_QUIT, QUIT_BTN_CANCEL, QUIT_BTN_COUNT };

static const char *const quit_labels[QUIT_BTN_COUNT] = { "Save & Quit", "Quit", "Cancel" };

static bool quit_confirmed = false;
static char quit_status[128] = { 0 };
static SDL_Color quit_status_color = { 245, 245, 250, 255 };

static SDL_FRect QuitButtonRect(int index)
{
    SDL_FRect r = { QUIT_X + 20.0f + (float)index * QUIT_BTN_STEP, QUIT_Y + QUIT_H - 46.0f, QUIT_BTN_W, QUIT_BTN_H };
    return r;
}

void Dialog_Quit_Show(void)
{
    quit_status[0] = '\0';
}

bool Dialog_Quit_TakeConfirmed(void)
{
    bool confirmed = quit_confirmed;
    quit_confirmed = false;
    return confirmed;
}

bool Dialog_Quit_HandleClick(float mx, float my)
{
    for (int i = 0; i < QUIT_BTN_COUNT; i++) {
        SDL_FRect r = QuitButtonRect(i);
        if (mx < r.x || mx > r.x + r.w || my < r.y || my > r.y + r.h) continue;

        if (i == QUIT_BTN_CANCEL) {
            GUI_Dialogs_Close();
        } else if (i == QUIT_BTN_QUIT) {
            quit_confirmed = true;
            GUI_Dialogs_Close();
        } else {
            const char *name = Dialog_File_GetSaveName();
            if (name[0] == '\0') {
                snprintf(quit_status, sizeof(quit_status), "Error: Filename cannot be empty");
                quit_status_color = col_red;
            } else if (SaveCity(name)) {
                quit_confirmed = true;
                GUI_Dialogs_Close();
            } else {
                snprintf(quit_status, sizeof(quit_status), "Error: Could not save to %s", name);
                quit_status_color = col_red;
                Audio_MakeSound("warning", "Sorry -speed 85");
            }
        }
        return true;
    }
    return true;
}

void Dialog_Quit_Render(SDL_Renderer *renderer)
{
    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 210);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect box = { QUIT_X, QUIT_Y, QUIT_W, QUIT_H };
    SDL_SetRenderDrawColor(renderer, 20, 24, 34, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &box);

    int tx = (int)QUIT_X + 20;
    int ty = (int)QUIT_Y + 20;
    GUI_DrawText(renderer, tx, ty, "QUIT MICROPOLIS?", col_gold);
    GUI_DrawText(renderer, tx, ty + 40, "Save your city before quitting?", col_white);
    GUI_DrawText(renderer, tx, ty + 64, "Quitting without saving loses your progress.", col_white);

    if (quit_status[0] != '\0') {
        GUI_DrawText(renderer, tx, ty + 88, quit_status, quit_status_color);
    }

    for (int i = 0; i < QUIT_BTN_COUNT; i++) {
        SDL_FRect r = QuitButtonRect(i);
        SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
        SDL_RenderRect(renderer, &r);
        int label_x = (int)(r.x + (r.w - (float)GUI_GetTextWidth(quit_labels[i])) / 2.0f);
        GUI_DrawText(renderer, label_x, (int)r.y + 8, quit_labels[i], col_white);
    }
}
