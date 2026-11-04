/* sdl3_dialog_choose.c: "Choose Another City" confirmation shown before New City or a scenario load.
 */

#include "sdl3_dialog_common.h"

#define CHOOSE_W 460.0f
#define CHOOSE_H 150.0f
#define CHOOSE_X ((WINDOW_DEFAULT_WIDTH - CHOOSE_W) / 2.0f)
#define CHOOSE_Y 200.0f
#define CHOOSE_BTN_H 32.0f
#define CHOOSE_BTN_PAD 16.0f
#define CHOOSE_BTN_GAP 20.0f

static const char *const choose_labels[2] = { "Keep playing.", "Another city!" };

static void (*choose_on_confirm)(int32_t) = NULL;
static int32_t choose_arg = 0;

static SDL_FRect ChooseButtonRect(int index)
{
    float row_w = 0.0f;
    for (int i = 0; i < 2; i++) row_w += GUI_ButtonWidth(choose_labels[i], CHOOSE_BTN_PAD);
    row_w += CHOOSE_BTN_GAP;

    float x = CHOOSE_X + (CHOOSE_W - row_w) / 2.0f;
    for (int i = 0; i < index; i++) x += GUI_ButtonWidth(choose_labels[i], CHOOSE_BTN_PAD) + CHOOSE_BTN_GAP;

    SDL_FRect r = { x, CHOOSE_Y + CHOOSE_H - 46.0f, GUI_ButtonWidth(choose_labels[index], CHOOSE_BTN_PAD), CHOOSE_BTN_H };
    return r;
}

void Dialog_ChooseCity_Show(void (*on_confirm)(int32_t), int32_t arg)
{
    choose_on_confirm = on_confirm;
    choose_arg = arg;
}

void Dialog_ChooseCity_Reset(void)
{
    choose_on_confirm = NULL;
    choose_arg = 0;
}

bool Dialog_ChooseCity_HandleClick(float mx, float my)
{
    SDL_FRect keep = ChooseButtonRect(0);
    if (mx >= keep.x && mx <= keep.x + keep.w && my >= keep.y && my <= keep.y + keep.h) {
        GUI_Dialogs_Close();
        return true;
    }

    SDL_FRect another = ChooseButtonRect(1);
    if (mx >= another.x && mx <= another.x + another.w && my >= another.y && my <= another.y + another.h) {
        /* Capture before Close(), which clears the stored callback. */
        void (*on_confirm)(int32_t) = choose_on_confirm;
        int32_t arg = choose_arg;
        GUI_Dialogs_Close();
        if (on_confirm) on_confirm(arg);
        return true;
    }
    return true;
}

void Dialog_ChooseCity_Render(SDL_Renderer *renderer)
{
    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 210);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect box = { CHOOSE_X, CHOOSE_Y, CHOOSE_W, CHOOSE_H };
    SDL_SetRenderDrawColor(renderer, 20, 24, 34, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &box);

    int tx = (int)CHOOSE_X + 20;
    int ty = (int)CHOOSE_Y + 20;
    GUI_DrawText(renderer, tx, ty, "CHOOSE ANOTHER CITY", col_gold);
    GUI_DrawText(renderer, tx, ty + 40, "Do you want to abandon this city and", col_white);
    GUI_DrawText(renderer, tx, ty + 64, "choose another one?", col_white);

    for (int i = 0; i < 2; i++) {
        SDL_FRect r = ChooseButtonRect(i);
        SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
        SDL_RenderRect(renderer, &r);
        GUI_DrawTextCentered(renderer, r, choose_labels[i], col_white);
    }
}
