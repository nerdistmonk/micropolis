/* sdl3_dialog_scenario.c: Scenario win/loss modal.
 */

#include "sdl3_dialog_common.h"

static bool scenario_won = false;
static char scenario_result_msg[256] = {0};

void Dialog_Scenario_Show(bool won, const char *msg)
{
    scenario_won = won;
    strncpy(scenario_result_msg, msg ? msg : (won ? "Scenario Victory!" : "Scenario Failed"), sizeof(scenario_result_msg) - 1);
    scenario_result_msg[sizeof(scenario_result_msg) - 1] = '\0';
    SetSimSpeed(0);

    if (won) {
        Audio_MakeSound("city", "stadium");
    } else {
        Audio_MakeSound("city", "sorry");
    }
}

bool Dialog_Scenario_HandleClick(float mx, float my)
{
    float dx = 290.0f;
    float dy = 240.0f;
    float dw = 444.0f;
    float dh = 200.0f;

    SDL_FRect ok_btn = { dx + (dw - 120.0f) / 2.0f, dy + dh - 46.0f, 120.0f, 32.0f };
    if (mx >= ok_btn.x && mx <= ok_btn.x + ok_btn.w &&
        my >= ok_btn.y && my <= ok_btn.y + ok_btn.h) {
        GUI_Dialogs_Close();
        return true;
    }
    return true;
}

void Dialog_Scenario_Render(SDL_Renderer *renderer)
{
    float dx = 290.0f;
    float dy = 240.0f;
    float dw = 444.0f;
    float dh = 200.0f;

    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 210);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 20, 24, 34, 255);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, scenario_won ? 70 : 200, scenario_won ? 200 : 50, 60, 255);
    SDL_RenderRect(renderer, &win_rect);

    const char *title = scenario_won ? "SCENARIO ACCOMPLISHED!" : "SCENARIO FAILED";
    SDL_Color header_col = scenario_won ? col_gold : col_red;
    GUI_DrawText(renderer, (int)dx + 20, (int)dy + 20, title, header_col);

    GUI_DrawText(renderer, (int)dx + 20, (int)dy + 65, scenario_result_msg, col_white);

    char score_buf[64];
    snprintf(score_buf, sizeof(score_buf), "Final City Score: %d / 1000", CityScore);
    GUI_DrawText(renderer, (int)dx + 20, (int)dy + 95, score_buf, col_cyan);

    SDL_FRect ok_btn = { dx + (dw - 120.0f) / 2.0f, dy + dh - 46.0f, 120.0f, 32.0f };
    SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
    SDL_RenderFillRect(renderer, &ok_btn);

    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &ok_btn);

    GUI_DrawText(renderer, (int)ok_btn.x + 44, (int)ok_btn.y + 8, "OK", col_white);
}