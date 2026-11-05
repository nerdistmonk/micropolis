/* sdl3_dialog_scenario.c: Scenario win/loss modal.
 */

#include "sdl3_dialog_common.h"

static bool scenario_won = false;
static char scenario_result_msg[256] = {0};
static SDL_Texture *key_texture = NULL;
static int key_w = 0;
static int key_h = 0;

static float ScenarioDialogHeight(void)
{
    return scenario_won ? 240.0f : 200.0f;
}

void Dialog_Scenario_Show(bool won, const char *msg)
{
    scenario_won = won;
    strncpy(scenario_result_msg, msg ? msg : (won ? "Scenario Victory!" : "Scenario Failed"), sizeof(scenario_result_msg) - 1);
    scenario_result_msg[sizeof(scenario_result_msg) - 1] = '\0';
    SetSimSpeed(0);

    if (won) {
        if (!key_texture) {
            const char *prefixes[] = { "", "images/", "../images/", "../../images/" };
            char path[256];
            for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]) && !key_texture; i++) {
                snprintf(path, sizeof(path), "%skey2city.xpm", prefixes[i]);
                key_texture = Video_LoadTexture(path, &key_w, &key_h);
            }
        }
        Audio_MakeSound("city", "stadium");
    } else {
        Audio_MakeSound("city", "sorry");
    }
}

static SDL_FRect ScenarioOkRect(float dx, float dy, float dw, float dh)
{
    float w = GUI_ButtonWidth("OK", 28.0f);
    SDL_FRect r = { dx + (dw - w) / 2.0f, dy + dh - 46.0f, w, 32.0f };
    return r;
}

bool Dialog_Scenario_HandleClick(float mx, float my)
{
    float dx = DialogOriginX(290.0f);
    float dy = DialogOriginY(240.0f);
    float dw = 444.0f;
    float dh = ScenarioDialogHeight();

    SDL_FRect ok_btn = ScenarioOkRect(dx, dy, dw, dh);
    if (mx >= ok_btn.x && mx <= ok_btn.x + ok_btn.w &&
        my >= ok_btn.y && my <= ok_btn.y + ok_btn.h) {
        GUI_Dialogs_Close();
        return true;
    }
    return true;
}

void Dialog_Scenario_Render(SDL_Renderer *renderer)
{
    float dx = DialogOriginX(290.0f);
    float dy = DialogOriginY(240.0f);
    float dw = 444.0f;
    float dh = ScenarioDialogHeight();

    SDL_FRect scrim = { 0.0f, 0.0f, (float)Video_GetCanvasWidth(), (float)Video_GetCanvasHeight() };
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

    if (scenario_won && key_texture) {
        SDL_FRect key_dst = { dx + (dw - (float)key_w) / 2.0f, dy + 118.0f, (float)key_w, (float)key_h };
        SDL_RenderTexture(renderer, key_texture, NULL, &key_dst);
    }

    SDL_FRect ok_btn = ScenarioOkRect(dx, dy, dw, dh);
    SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
    SDL_RenderFillRect(renderer, &ok_btn);

    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &ok_btn);

    GUI_DrawTextCentered(renderer, ok_btn, "OK", col_white);
}