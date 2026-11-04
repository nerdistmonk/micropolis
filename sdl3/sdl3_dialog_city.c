/* sdl3_dialog_city.c: City Hall Desk modal (Budget, Taxes, Evaluation, Graphs, City Info).
 */

#include "sdl3_dialog_common.h"

static CityMgmtTab active_city_tab = CITY_TAB_BUDGET;
static bool graph_120_year_mode = false;
static bool graph_metric_visible[HISTORIES] = { true, true, true, true, true, true };
static bool is_editing_city_name = false;
static char city_name_input_buffer[64] = {0};

void Dialog_City_Open(CityMgmtTab tab)
{
    active_city_tab = tab;
    Dialog_City_Reset();
}

void Dialog_City_Reset(void)
{
    if (is_editing_city_name) {
        SDL_StopTextInput(Video_GetWindow());
        is_editing_city_name = false;
    }
}

bool Dialog_City_HandleTextInput(const char *text)
{
    if (!text || !is_editing_city_name) return false;

    size_t cur = strlen(city_name_input_buffer);
    size_t add = strlen(text);
    if (cur + add < sizeof(city_name_input_buffer) - 1) {
        strcat(city_name_input_buffer, text);
    }
    return true;
}

bool Dialog_City_HandleKeyDown(SDL_Keycode key)
{
    if (!is_editing_city_name) return false;

    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (strlen(city_name_input_buffer) > 0) {
            strncpy(CityName, city_name_input_buffer, sizeof(CityName) - 1);
            CityName[sizeof(CityName) - 1] = '\0';
        }
        SDL_StopTextInput(Video_GetWindow());
        is_editing_city_name = false;
        return true;
    } else if (key == SDLK_BACKSPACE) {
        size_t len = strlen(city_name_input_buffer);
        if (len > 0) {
            city_name_input_buffer[len - 1] = '\0';
        }
        return true;
    }
    return true;
}

bool Dialog_City_HandleClick(float mx, float my)
{
    float dx = 220.0f;
    float dy = 110.0f;
    float dw = 580.0f;

    if (mx >= dx + dw - 32.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 32.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    float tab_x = dx + 14.0f;
    float tab_y = dy + 42.0f;
    float tab_w = 104.0f;
    float tab_h = 28.0f;

    for (int i = 0; i < CITY_TAB_COUNT; i++) {
        if (mx >= tab_x && mx <= tab_x + tab_w && my >= tab_y && my <= tab_y + tab_h) {
            active_city_tab = (CityMgmtTab)i;
            if (active_city_tab != CITY_TAB_CITY_INFO && is_editing_city_name) {
                SDL_StopTextInput(Video_GetWindow());
                is_editing_city_name = false;
            }
            return true;
        }
        tab_x += tab_w + 6.0f;
    }

    if (active_city_tab == CITY_TAB_BUDGET) {
        if (my >= dy + 115.0f && my <= dy + 137.0f) {
            if (mx >= dx + 400.0f && mx <= dx + 430.0f) {
                roadPercent -= 0.10f;
                if (roadPercent < 0.0f) roadPercent = 0.0f;
                RoadEffect = (int16_t)(roadPercent * 32.0f);
                return true;
            } else if (mx >= dx + 440.0f && mx <= dx + 470.0f) {
                roadPercent += 0.10f;
                if (roadPercent > 1.0f) roadPercent = 1.0f;
                RoadEffect = (int16_t)(roadPercent * 32.0f);
                return true;
            }
        }
        if (my >= dy + 165.0f && my <= dy + 187.0f) {
            if (mx >= dx + 400.0f && mx <= dx + 430.0f) {
                policePercent -= 0.10f;
                if (policePercent < 0.0f) policePercent = 0.0f;
                PoliceEffect = (int16_t)(policePercent * 1000.0f);
                return true;
            } else if (mx >= dx + 440.0f && mx <= dx + 470.0f) {
                policePercent += 0.10f;
                if (policePercent > 1.0f) policePercent = 1.0f;
                PoliceEffect = (int16_t)(policePercent * 1000.0f);
                return true;
            }
        }
        if (my >= dy + 215.0f && my <= dy + 237.0f) {
            if (mx >= dx + 400.0f && mx <= dx + 430.0f) {
                firePercent -= 0.10f;
                if (firePercent < 0.0f) firePercent = 0.0f;
                FireEffect = (int16_t)(firePercent * 1000.0f);
                return true;
            } else if (mx >= dx + 440.0f && mx <= dx + 470.0f) {
                firePercent += 0.10f;
                if (firePercent > 1.0f) firePercent = 1.0f;
                FireEffect = (int16_t)(firePercent * 1000.0f);
                return true;
            }
        }

        SDL_FRect commit_btn = { dx + 215.0f, dy + 370.0f, 150.0f, 32.0f };
        if (mx >= commit_btn.x && mx <= commit_btn.x + commit_btn.w &&
            my >= commit_btn.y && my <= commit_btn.y + commit_btn.h) {
            GUI_Dialogs_Close();
            return true;
        }
    }

    if (active_city_tab == CITY_TAB_TAXES) {
        if (my >= dy + 125.0f && my <= dy + 155.0f) {
            if (mx >= dx + 240.0f && mx <= dx + 280.0f) {
                if (CityTax > 0) CityTax--;
                return true;
            } else if (mx >= dx + 300.0f && mx <= dx + 340.0f) {
                if (CityTax < 20) CityTax++;
                return true;
            }
        }
    }

    if (active_city_tab == CITY_TAB_GRAPHS) {
        SDL_FRect btn_10y = { dx + 26.0f, dy + 88.0f, 96.0f, 26.0f };
        SDL_FRect btn_120y = { dx + 130.0f, dy + 88.0f, 96.0f, 26.0f };

        if (mx >= btn_10y.x && mx <= btn_10y.x + btn_10y.w &&
            my >= btn_10y.y && my <= btn_10y.y + btn_10y.h) {
            graph_120_year_mode = false;
            return true;
        }
        if (mx >= btn_120y.x && mx <= btn_120y.x + btn_120y.w &&
            my >= btn_120y.y && my <= btn_120y.y + btn_120y.h) {
            graph_120_year_mode = true;
            return true;
        }

        float leg_x = dx + 444.0f;
        float leg_y = dy + 124.0f;
        for (int m = 0; m < HISTORIES; m++) {
            SDL_FRect m_btn = { leg_x, leg_y + (m * 28.0f), 110.0f, 24.0f };
            if (mx >= m_btn.x && mx <= m_btn.x + m_btn.w &&
                my >= m_btn.y && my <= m_btn.y + m_btn.h) {
                graph_metric_visible[m] = !graph_metric_visible[m];
                return true;
            }
        }
    }

    if (active_city_tab == CITY_TAB_CITY_INFO) {
        float input_x = dx + 130.0f;
        float input_y = dy + 115.0f;
        float input_w = 260.0f;
        float input_h = 30.0f;

        if (mx >= input_x && mx <= input_x + input_w && my >= input_y && my <= input_y + input_h) {
            if (!is_editing_city_name) {
                is_editing_city_name = true;
                strncpy(city_name_input_buffer, CityName, sizeof(city_name_input_buffer) - 1);
                city_name_input_buffer[sizeof(city_name_input_buffer) - 1] = '\0';
                SDL_StartTextInput(Video_GetWindow());
            }
            return true;
        } else if (is_editing_city_name) {
            if (strlen(city_name_input_buffer) > 0) {
                strncpy(CityName, city_name_input_buffer, sizeof(CityName) - 1);
                CityName[sizeof(CityName) - 1] = '\0';
            }
            SDL_StopTextInput(Video_GetWindow());
            is_editing_city_name = false;
            return true;
        }
    }

    return true;
}

static void RenderGraphsTab(SDL_Renderer *renderer, float dx, float dy, float dw, float dh)
{
    (void)dw;
    (void)dh;
    int cx = (int)dx + 30;
    int cy = (int)dy + 88;

    SDL_FRect btn_10y = { (float)cx, (float)cy, 96.0f, 26.0f };
    SDL_FRect btn_120y = { (float)cx + 104.0f, (float)cy, 96.0f, 26.0f };

    SDL_SetRenderDrawColor(renderer, !graph_120_year_mode ? 48 : 28, !graph_120_year_mode ? 85 : 36, !graph_120_year_mode ? 140 : 48, 255);
    SDL_RenderFillRect(renderer, &btn_10y);
    SDL_SetRenderDrawColor(renderer, !graph_120_year_mode ? 255 : 70, !graph_120_year_mode ? 215 : 85, !graph_120_year_mode ? 0 : 110, 255);
    SDL_RenderRect(renderer, &btn_10y);
    GUI_DrawText(renderer, cx + 18, cy + 5, "10 Years", !graph_120_year_mode ? col_gold : col_gray);

    SDL_SetRenderDrawColor(renderer, graph_120_year_mode ? 48 : 28, graph_120_year_mode ? 85 : 36, graph_120_year_mode ? 140 : 48, 255);
    SDL_RenderFillRect(renderer, &btn_120y);
    SDL_SetRenderDrawColor(renderer, graph_120_year_mode ? 255 : 70, graph_120_year_mode ? 215 : 85, graph_120_year_mode ? 0 : 110, 255);
    SDL_RenderRect(renderer, &btn_120y);
    GUI_DrawText(renderer, cx + 118, cy + 5, "120 Years", graph_120_year_mode ? col_gold : col_gray);

    float gx = dx + 30.0f;
    float gy = dy + 124.0f;
    float gw = 390.0f;
    float gh = 186.0f;

    SDL_FRect gbox = { gx, gy, gw, gh };
    SDL_SetRenderDrawColor(renderer, 10, 12, 16, 255);
    SDL_RenderFillRect(renderer, &gbox);
    SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
    SDL_RenderRect(renderer, &gbox);

    SDL_SetRenderDrawColor(renderer, 24, 30, 42, 255);
    for (int step = 1; step <= 3; step++) {
        float y_line = gy + ((gh / 4.0f) * (float)step);
        SDL_RenderLine(renderer, gx, y_line, gx + gw, y_line);
    }
    for (int step = 1; step <= 5; step++) {
        float x_line = gx + ((gw / 6.0f) * (float)step);
        SDL_RenderLine(renderer, x_line, gy, x_line, gy + gh);
    }

    static const char *metric_labels[HISTORIES] = {
        "Residential", "Commercial", "Industrial", "Cash/Funds", "Crime", "Pollution"
    };
    static const SDL_Color metric_colors[HISTORIES] = {
        { 245, 215, 60, 255 },
        { 80, 150, 255, 255 },
        { 255, 140, 50, 255 },
        { 80, 240, 120, 255 },
        { 240, 60, 60, 255 },
        { 180, 100, 240, 255 }
    };

    int num_points = graph_120_year_mode ? MISCHISTLEN : (HISTLEN / 4);
    if (num_points < 10) num_points = 120;

    for (int m = 0; m < HISTORIES; m++) {
        if (!graph_metric_visible[m]) continue;

        uint8_t *h_buf = graph_120_year_mode ? History120[m] : History10[m];
        if (!h_buf) continue;

        SDL_SetRenderDrawColor(renderer, metric_colors[m].r, metric_colors[m].g, metric_colors[m].b, metric_colors[m].a);

        float prev_px = gx;
        float prev_py = gy + gh - ((float)h_buf[0] / 255.0f * gh);

        for (int p = 1; p < num_points; p++) {
            float cur_px = gx + (((float)p / (float)(num_points - 1)) * gw);
            float cur_val = (float)h_buf[p];
            float cur_py = gy + gh - ((cur_val / 255.0f) * gh);
            if (cur_py < gy) cur_py = gy;
            if (cur_py > gy + gh) cur_py = gy + gh;

            SDL_RenderLine(renderer, prev_px, prev_py, cur_px, cur_py);
            prev_px = cur_px;
            prev_py = cur_py;
        }
    }

    float leg_x = dx + 444.0f;
    float leg_y = dy + 124.0f;

    for (int m = 0; m < HISTORIES; m++) {
        SDL_FRect m_btn = { leg_x, leg_y + (m * 28.0f), 110.0f, 24.0f };
        bool on = graph_metric_visible[m];

        SDL_SetRenderDrawColor(renderer, on ? 34 : 20, on ? 42 : 24, on ? 58 : 30, 255);
        SDL_RenderFillRect(renderer, &m_btn);

        SDL_SetRenderDrawColor(renderer, metric_colors[m].r, metric_colors[m].g, metric_colors[m].b, on ? 255 : 90);
        SDL_RenderRect(renderer, &m_btn);

        SDL_FRect pip = { leg_x + 6.0f, m_btn.y + 6.0f, 10.0f, 12.0f };
        SDL_SetRenderDrawColor(renderer, metric_colors[m].r, metric_colors[m].g, metric_colors[m].b, on ? 255 : 60);
        SDL_RenderFillRect(renderer, &pip);

        GUI_DrawText(renderer, (int)leg_x + 22, (int)m_btn.y + 4, metric_labels[m], on ? col_white : col_gray);
    }

    GUI_DrawText(renderer, (int)gx, (int)gy + (int)gh + 8, "Range: Year 0 to present | Scale normalized (0 - 255)", col_gray);
}

void Dialog_City_Render(SDL_Renderer *renderer)
{
    float dx = 220.0f;
    float dy = 110.0f;
    float dw = 580.0f;
    float dh = 430.0f;

    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 24, 28, 38, 250);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
    SDL_RenderRect(renderer, &win_rect);

    GUI_DrawText(renderer, (int)dx + 16, (int)dy + 14, "CITY HALL DESK", col_gold);

    SDL_FRect close_rect = { dx + dw - 28.0f, dy + 8.0f, 20.0f, 20.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 6, (int)close_rect.y + 4, "X", col_white);

    const char *tabs[] = { "Budget", "Taxes", "Evaluation", "Graphs", "City Info" };
    float tab_x = dx + 14.0f;
    float tab_y = dy + 42.0f;
    float tab_w = 104.0f;
    float tab_h = 28.0f;

    for (int i = 0; i < CITY_TAB_COUNT; i++) {
        SDL_FRect tr = { tab_x, tab_y, tab_w, tab_h };
        if (active_city_tab == (CityMgmtTab)i) {
            SDL_SetRenderDrawColor(renderer, 50, 85, 140, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
            SDL_RenderRect(renderer, &tr);
            GUI_DrawText(renderer, (int)tr.x + 10, (int)tr.y + 6, tabs[i], col_white);
        } else {
            SDL_SetRenderDrawColor(renderer, 32, 38, 52, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 60, 72, 92, 255);
            SDL_RenderRect(renderer, &tr);
            GUI_DrawText(renderer, (int)tr.x + 10, (int)tr.y + 6, tabs[i], col_gray);
        }
        tab_x += tab_w + 6.0f;
    }

    SDL_FRect content_box = { dx + 14.0f, dy + 78.0f, dw - 28.0f, dh - 92.0f };
    SDL_SetRenderDrawColor(renderer, 18, 22, 30, 255);
    SDL_RenderFillRect(renderer, &content_box);
    SDL_SetRenderDrawColor(renderer, 50, 60, 80, 255);
    SDL_RenderRect(renderer, &content_box);

    int cx = (int)content_box.x + 16;
    int cy = (int)content_box.y + 16;

    if (active_city_tab == CITY_TAB_BUDGET) {
        char buf[128];
        GUI_DrawText(renderer, cx, cy, "ANNUAL BUDGET & DEPARTMENT FUNDING", col_gold);

        snprintf(buf, sizeof(buf), "Roads: %3d%% Cost: $%d", (int)(roadPercent * 100.0f), (int)(RoadTotal * 5 * roadPercent));
        GUI_DrawText(renderer, cx, cy + 30, buf, col_white);
        SDL_FRect r_dn = { dx + 400.0f, dy + 115.0f, 30.0f, 22.0f };
        SDL_FRect r_up = { dx + 440.0f, dy + 115.0f, 30.0f, 22.0f };
        SDL_SetRenderDrawColor(renderer, 40, 50, 70, 255);
        SDL_RenderFillRect(renderer, &r_dn);
        SDL_RenderFillRect(renderer, &r_up);
        GUI_DrawText(renderer, (int)r_dn.x + 10, (int)r_dn.y + 3, "-", col_white);
        GUI_DrawText(renderer, (int)r_up.x + 10, (int)r_up.y + 3, "+", col_white);

        snprintf(buf, sizeof(buf), "Police: %3d%% Cost: $%d", (int)(policePercent * 100.0f), (int)(PolicePop * 100 * policePercent));
        GUI_DrawText(renderer, cx, cy + 80, buf, col_white);
        SDL_FRect p_dn = { dx + 400.0f, dy + 165.0f, 30.0f, 22.0f };
        SDL_FRect p_up = { dx + 440.0f, dy + 165.0f, 30.0f, 22.0f };
        SDL_SetRenderDrawColor(renderer, 40, 50, 70, 255);
        SDL_RenderFillRect(renderer, &p_dn);
        SDL_RenderFillRect(renderer, &p_up);
        GUI_DrawText(renderer, (int)p_dn.x + 10, (int)p_dn.y + 3, "-", col_white);
        GUI_DrawText(renderer, (int)p_up.x + 10, (int)p_up.y + 3, "+", col_white);

        snprintf(buf, sizeof(buf), "Fire: %3d%% Cost: $%d", (int)(firePercent * 100.0f), (int)(FireStPop * 100 * firePercent));
        GUI_DrawText(renderer, cx, cy + 130, buf, col_white);
        SDL_FRect f_dn = { dx + 400.0f, dy + 215.0f, 30.0f, 22.0f };
        SDL_FRect f_up = { dx + 440.0f, dy + 215.0f, 30.0f, 22.0f };
        SDL_SetRenderDrawColor(renderer, 40, 50, 70, 255);
        SDL_RenderFillRect(renderer, &f_dn);
        SDL_RenderFillRect(renderer, &f_up);
        GUI_DrawText(renderer, (int)f_dn.x + 10, (int)f_dn.y + 3, "-", col_white);
        GUI_DrawText(renderer, (int)f_up.x + 10, (int)f_up.y + 3, "+", col_white);

        int32_t expenses = (int32_t)((RoadTotal * 5 * roadPercent) + (PolicePop * 100 * policePercent) + (FireStPop * 100 * firePercent));
        int32_t net = TaxFund - expenses;
        snprintf(buf, sizeof(buf), "Tax Revenue: $%d Expenses: $%d Net: $%d", TaxFund, expenses, net);
        GUI_DrawText(renderer, cx, cy + 195, buf, (net >= 0) ? col_green : col_red);

        SDL_FRect commit_btn = { dx + 215.0f, dy + 370.0f, 150.0f, 32.0f };
        SDL_SetRenderDrawColor(renderer, 40, 90, 70, 255);
        SDL_RenderFillRect(renderer, &commit_btn);
        SDL_SetRenderDrawColor(renderer, 70, 180, 120, 255);
        SDL_RenderRect(renderer, &commit_btn);
        GUI_DrawText(renderer, (int)commit_btn.x + 18, (int)commit_btn.y + 8, "Apply & Close", col_white);

    } else if (active_city_tab == CITY_TAB_TAXES) {
        char buf[128];
        GUI_DrawText(renderer, cx, cy, "TAXATION & FISCAL POLICY", col_gold);

        snprintf(buf, sizeof(buf), "Current Tax Rate: %d%%", CityTax);
        GUI_DrawText(renderer, cx, cy + 40, buf, col_white);

        SDL_FRect t_dn = { dx + 240.0f, dy + 125.0f, 40.0f, 28.0f };
        SDL_FRect t_up = { dx + 300.0f, dy + 125.0f, 40.0f, 28.0f };
        SDL_SetRenderDrawColor(renderer, 40, 50, 70, 255);
        SDL_RenderFillRect(renderer, &t_dn);
        SDL_RenderFillRect(renderer, &t_up);
        GUI_DrawText(renderer, (int)t_dn.x + 15, (int)t_dn.y + 6, "-", col_white);
        GUI_DrawText(renderer, (int)t_up.x + 15, (int)t_up.y + 6, "+", col_white);

        snprintf(buf, sizeof(buf), "Projected Annual Receipts: $%d", TaxFund);
        GUI_DrawText(renderer, cx, cy + 90, buf, col_green);

        if (CityTax > 12) {
            GUI_DrawText(renderer, cx, cy + 130, "Warning: High taxes will stifle development!", col_red);
        } else if (CityTax <= 5) {
            GUI_DrawText(renderer, cx, cy + 130, "Notice: Low taxes strongly attract new residents.", col_cyan);
        } else {
            GUI_DrawText(renderer, cx, cy + 130, "Status: Balanced tax rate.", col_gray);
        }

    } else if (active_city_tab == CITY_TAB_EVALUATION) {
        char buf[128];
        GUI_DrawText(renderer, cx, cy, "MAYORAL PERFORMANCE & VOTER SURVEY", col_gold);

        snprintf(buf, sizeof(buf), "Approval Rating: %d%% Yes %d%% No", CityYes, CityNo);
        GUI_DrawText(renderer, cx, cy + 30, buf, col_white);

        snprintf(buf, sizeof(buf), "Overall City Score: %d / 1000", CityScore);
        GUI_DrawText(renderer, cx, cy + 55, buf, col_gold);

        GUI_DrawText(renderer, cx, cy + 95, "Key Citizen Grievances:", col_cyan);

        static const char *problem_names[] = {
            "Crime", "Pollution", "Housing Costs", "Taxes",
            "Traffic", "Unemployment", "Fires", "None"
        };

        for (int p = 0; p < 4; p++) {
            int prob_idx = ProblemOrder[p];
            if (prob_idx < 0 || prob_idx > 7) prob_idx = 7;
            snprintf(buf, sizeof(buf), "%d. %s (%d votes)", p + 1, problem_names[prob_idx], ProblemVotes[prob_idx]);
            GUI_DrawText(renderer, cx + 16, cy + 125 + (p * 24), buf, col_white);
        }

    } else if (active_city_tab == CITY_TAB_GRAPHS) {
        RenderGraphsTab(renderer, dx, dy, dw, dh);

    } else if (active_city_tab == CITY_TAB_CITY_INFO) {
        char buf[128];
        GUI_DrawText(renderer, cx, cy, "MUNICIPAL REGISTRATION & DEMOGRAPHICS", col_gold);

        GUI_DrawText(renderer, cx, cy + 35, "City Name:", col_white);

        SDL_FRect input_rect = { dx + 130.0f, dy + 115.0f, 260.0f, 28.0f };
        SDL_SetRenderDrawColor(renderer, is_editing_city_name ? 30 : 20, is_editing_city_name ? 40 : 25, is_editing_city_name ? 60 : 35, 255);
        SDL_RenderFillRect(renderer, &input_rect);

        SDL_SetRenderDrawColor(renderer, is_editing_city_name ? 255 : 80, is_editing_city_name ? 215 : 95, is_editing_city_name ? 0 : 120, 255);
        SDL_RenderRect(renderer, &input_rect);

        const char *display_str = is_editing_city_name ? city_name_input_buffer : CityName;
        char edit_text[70];
        if (is_editing_city_name && (flagBlink > 0)) {
            snprintf(edit_text, sizeof(edit_text), "%s_", display_str);
        } else {
            snprintf(edit_text, sizeof(edit_text), "%s", display_str);
        }
        GUI_DrawText(renderer, (int)input_rect.x + 8, (int)input_rect.y + 6, edit_text, col_white);

        if (is_editing_city_name) {
            GUI_DrawText(renderer, cx, cy + 70, "[Press Enter to confirm, Escape to cancel]", col_gold);
        } else {
            GUI_DrawText(renderer, cx, cy + 70, "[Click name field to rename your city]", col_gray);
        }

        static const char *classes[] = { "Village", "Town", "City", "Capital", "Metropolis", "Megalopolis" };
        int cl = CityClass;
        if (cl < 0) cl = 0;
        if (cl > 5) cl = 5;
        snprintf(buf, sizeof(buf), "Classification: %s", classes[cl]);
        GUI_DrawText(renderer, cx, cy + 105, buf, col_cyan);

        snprintf(buf, sizeof(buf), "Population: %d citizens", (int)CityPop);
        GUI_DrawText(renderer, cx, cy + 130, buf, col_white);

        snprintf(buf, sizeof(buf), "Assessed Value: $%d", CityAssValue);
        GUI_DrawText(renderer, cx, cy + 155, buf, col_green);
    }
}