/* sdl3_dialog_file.c: File persistence, historical scenarios, and preference toggles.
 */

#include "sdl3_dialog_common.h"

static FileDialogTab active_file_tab = FILE_TAB_SAVE_LOAD;
static bool is_editing_filename = false;
static char file_name_input_buffer[64] = "mycity.cty";
static char file_status_message[128] = {0};
static SDL_Color file_status_color = { 245, 245, 250, 255 };

void Dialog_File_Open(FileDialogTab tab)
{
    active_file_tab = tab;
    Dialog_File_Reset();
    file_status_message[0] = '\0';
}

void Dialog_File_Reset(void)
{
    if (is_editing_filename) {
        SDL_StopTextInput(Video_GetWindow());
        is_editing_filename = false;
    }
}

bool Dialog_File_HandleTextInput(const char *text)
{
    if (!text || !is_editing_filename) return false;

    size_t cur = strlen(file_name_input_buffer);
    size_t add = strlen(text);
    if (cur + add < sizeof(file_name_input_buffer) - 1) {
        strcat(file_name_input_buffer, text);
    }
    return true;
}

bool Dialog_File_HandleKeyDown(SDL_Keycode key)
{
    if (!is_editing_filename) return false;

    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        SDL_StopTextInput(Video_GetWindow());
        is_editing_filename = false;
        return true;
    } else if (key == SDLK_BACKSPACE) {
        size_t len = strlen(file_name_input_buffer);
        if (len > 0) {
            file_name_input_buffer[len - 1] = '\0';
        }
        return true;
    }
    return true;
}

bool Dialog_File_HandleClick(float mx, float my)
{
    float dx = 240.0f;
    float dy = 120.0f;
    float dw = 540.0f;

    if (mx >= dx + dw - 32.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 32.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    float tab_x = dx + 16.0f;
    float tab_y = dy + 42.0f;
    float tab_w = 120.0f;
    float tab_h = 28.0f;

    for (int i = 0; i < FILE_TAB_COUNT; i++) {
        if (mx >= tab_x && mx <= tab_x + tab_w && my >= tab_y && my <= tab_y + tab_h) {
            active_file_tab = (FileDialogTab)i;
            if (is_editing_filename) {
                SDL_StopTextInput(Video_GetWindow());
                is_editing_filename = false;
            }
            return true;
        }
        tab_x += tab_w + 8.0f;
    }

    if (active_file_tab == FILE_TAB_SAVE_LOAD) {
        SDL_FRect input_rect = { dx + 130.0f, dy + 138.0f, 280.0f, 28.0f };
        if (mx >= input_rect.x && mx <= input_rect.x + input_rect.w &&
            my >= input_rect.y && my <= input_rect.y + input_rect.h) {
            if (!is_editing_filename) {
                is_editing_filename = true;
                SDL_StartTextInput(Video_GetWindow());
            }
            return true;
        } else if (is_editing_filename) {
            SDL_StopTextInput(Video_GetWindow());
            is_editing_filename = false;
        }

        SDL_FRect save_btn = { dx + 60.0f, dy + 185.0f, 180.0f, 34.0f };
        if (mx >= save_btn.x && mx <= save_btn.x + save_btn.w &&
            my >= save_btn.y && my <= save_btn.y + save_btn.h) {
            if (strlen(file_name_input_buffer) > 0) {
                SaveCity(file_name_input_buffer);
                snprintf(file_status_message, sizeof(file_status_message), "Successfully saved city to %s", file_name_input_buffer);
                file_status_color = col_green;
            } else {
                snprintf(file_status_message, sizeof(file_status_message), "Error: Filename cannot be empty");
                file_status_color = col_red;
            }
            return true;
        }

        SDL_FRect load_btn = { dx + 300.0f, dy + 185.0f, 180.0f, 34.0f };
        if (mx >= load_btn.x && mx <= load_btn.x + load_btn.w &&
            my >= load_btn.y && my <= load_btn.y + load_btn.h) {
            if (strlen(file_name_input_buffer) > 0) {
                if (LoadCity(file_name_input_buffer)) {
                    snprintf(file_status_message, sizeof(file_status_message), "Loaded %s successfully", file_name_input_buffer);
                    file_status_color = col_green;
                } else {
                    snprintf(file_status_message, sizeof(file_status_message), "Failed to load %s", file_name_input_buffer);
                    file_status_color = col_red;
                }
            }
            return true;
        }

        SDL_FRect new_btn = { dx + 180.0f, dy + 235.0f, 180.0f, 34.0f };
        if (mx >= new_btn.x && mx <= new_btn.x + new_btn.w &&
            my >= new_btn.y && my <= new_btn.y + new_btn.h) {
            Sim_InitEngine();
            GenerateMap((uint32_t)SDL_GetTicks());
            SetSimSpeed(3);
            snprintf(file_status_message, sizeof(file_status_message), "New virgin map generated!");
            file_status_color = col_cyan;
            return true;
        }
    } else if (active_file_tab == FILE_TAB_SCENARIOS) {
        struct {
            int id;
            SDL_FRect rect;
        } sc_buttons[] = {
            { 1, { dx + 30.0f, dy + 95.0f, 230.0f, 34.0f } },
            { 2, { dx + 280.0f, dy + 95.0f, 230.0f, 34.0f } },
            { 3, { dx + 30.0f, dy + 145.0f, 230.0f, 34.0f } },
            { 4, { dx + 280.0f, dy + 145.0f, 230.0f, 34.0f } },
            { 5, { dx + 30.0f, dy + 195.0f, 230.0f, 34.0f } },
            { 6, { dx + 280.0f, dy + 195.0f, 230.0f, 34.0f } },
            { 7, { dx + 30.0f, dy + 245.0f, 230.0f, 34.0f } }
        };

        for (size_t i = 0; i < sizeof(sc_buttons) / sizeof(sc_buttons[0]); i++) {
            SDL_FRect r = sc_buttons[i].rect;
            if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                LoadScenario(sc_buttons[i].id);
                GUI_Dialogs_Close();
                return true;
            }
        }
    } else if (active_file_tab == FILE_TAB_OPTIONS) {
        float opt_x = dx + 340.0f;
        float opt_y = dy + 105.0f;

        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y && my <= opt_y + 26.0f) {
            autoBudget = !autoBudget;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 35.0f && my <= opt_y + 61.0f) {
            autoBulldoze = !autoBulldoze;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 70.0f && my <= opt_y + 96.0f) {
            autoGo = !autoGo;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 105.0f && my <= opt_y + 131.0f) {
            NoDisasters = !NoDisasters;
            return true;
        }
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 140.0f && my <= opt_y + 166.0f) {
            UserSoundOn = !UserSoundOn;
            return true;
        }
    }

    return true;
}

void Dialog_File_Render(SDL_Renderer *renderer)
{
    float dx = 240.0f;
    float dy = 120.0f;
    float dw = 540.0f;
    float dh = 400.0f;

    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 24, 28, 38, 250);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
    SDL_RenderRect(renderer, &win_rect);

    GUI_DrawText(renderer, (int)dx + 16, (int)dy + 14, "FILE & SCENARIO MANAGER", col_gold);

    SDL_FRect close_rect = { dx + dw - 28.0f, dy + 8.0f, 20.0f, 20.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 6, (int)close_rect.y + 4, "X", col_white);

    const char *tabs[] = { "Save / Load", "Scenarios", "Options" };
    float tab_x = dx + 16.0f;
    float tab_y = dy + 42.0f;
    float tab_w = 120.0f;
    float tab_h = 28.0f;

    for (int i = 0; i < FILE_TAB_COUNT; i++) {
        SDL_FRect tr = { tab_x, tab_y, tab_w, tab_h };
        if (active_file_tab == (FileDialogTab)i) {
            SDL_SetRenderDrawColor(renderer, 50, 85, 140, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
            SDL_RenderRect(renderer, &tr);
            GUI_DrawText(renderer, (int)tr.x + 14, (int)tr.y + 6, tabs[i], col_white);
        } else {
            SDL_SetRenderDrawColor(renderer, 32, 38, 52, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 60, 72, 92, 255);
            SDL_RenderRect(renderer, &tr);
            GUI_DrawText(renderer, (int)tr.x + 14, (int)tr.y + 6, tabs[i], col_gray);
        }
        tab_x += tab_w + 8.0f;
    }

    SDL_FRect content_box = { dx + 16.0f, dy + 80.0f, dw - 32.0f, dh - 96.0f };
    SDL_SetRenderDrawColor(renderer, 18, 22, 30, 255);
    SDL_RenderFillRect(renderer, &content_box);
    SDL_SetRenderDrawColor(renderer, 50, 60, 80, 255);
    SDL_RenderRect(renderer, &content_box);

    int cx = (int)content_box.x + 16;
    int cy = (int)content_box.y + 16;

    if (active_file_tab == FILE_TAB_SAVE_LOAD) {
        GUI_DrawText(renderer, cx, cy, "CITY PERSISTENCE & FILE DISK", col_gold);
        GUI_DrawText(renderer, cx, cy + 46, "Filename:", col_white);

        SDL_FRect input_rect = { dx + 130.0f, dy + 138.0f, 280.0f, 28.0f };
        SDL_SetRenderDrawColor(renderer, is_editing_filename ? 30 : 20, is_editing_filename ? 40 : 25, is_editing_filename ? 60 : 35, 255);
        SDL_RenderFillRect(renderer, &input_rect);
        SDL_SetRenderDrawColor(renderer, is_editing_filename ? 255 : 80, is_editing_filename ? 215 : 95, is_editing_filename ? 0 : 120, 255);
        SDL_RenderRect(renderer, &input_rect);

        char edit_text[70];
        if (is_editing_filename && (flagBlink > 0)) {
            snprintf(edit_text, sizeof(edit_text), "%s_", file_name_input_buffer);
        } else {
            snprintf(edit_text, sizeof(edit_text), "%s", file_name_input_buffer);
        }
        GUI_DrawText(renderer, (int)input_rect.x + 8, (int)input_rect.y + 6, edit_text, col_white);

        SDL_FRect save_btn = { dx + 60.0f, dy + 185.0f, 180.0f, 34.0f };
        SDL_SetRenderDrawColor(renderer, 40, 70, 110, 255);
        SDL_RenderFillRect(renderer, &save_btn);
        SDL_SetRenderDrawColor(renderer, 80, 140, 220, 255);
        SDL_RenderRect(renderer, &save_btn);
        GUI_DrawText(renderer, (int)save_btn.x + 44, (int)save_btn.y + 8, "Save City", col_white);

        SDL_FRect load_btn = { dx + 300.0f, dy + 185.0f, 180.0f, 34.0f };
        SDL_SetRenderDrawColor(renderer, 40, 90, 70, 255);
        SDL_RenderFillRect(renderer, &load_btn);
        SDL_SetRenderDrawColor(renderer, 70, 160, 120, 255);
        SDL_RenderRect(renderer, &load_btn);
        GUI_DrawText(renderer, (int)load_btn.x + 44, (int)load_btn.y + 8, "Load City", col_white);

        SDL_FRect new_btn = { dx + 180.0f, dy + 235.0f, 180.0f, 34.0f };
        SDL_SetRenderDrawColor(renderer, 100, 70, 30, 255);
        SDL_RenderFillRect(renderer, &new_btn);
        SDL_SetRenderDrawColor(renderer, 200, 140, 50, 255);
        SDL_RenderRect(renderer, &new_btn);
        GUI_DrawText(renderer, (int)new_btn.x + 36, (int)new_btn.y + 8, "New City Map", col_gold);

        if (file_status_message[0]) {
            GUI_DrawText(renderer, cx, cy + 180, file_status_message, file_status_color);
        }

    } else if (active_file_tab == FILE_TAB_SCENARIOS) {
        GUI_DrawText(renderer, cx, cy, "HISTORICAL CRISIS SCENARIOS", col_gold);

        struct {
            const char *title;
            SDL_FRect rect;
        } sc_display[] = {
            { "1. Dullsville (1900)", { dx + 30.0f, dy + 95.0f, 230.0f, 34.0f } },
            { "2. San Francisco (1906)", { dx + 280.0f, dy + 95.0f, 230.0f, 34.0f } },
            { "3. Hamburg (1944)", { dx + 30.0f, dy + 145.0f, 230.0f, 34.0f } },
            { "4. Bern (1965)", { dx + 280.0f, dy + 145.0f, 230.0f, 34.0f } },
            { "5. Tokyo (1957)", { dx + 30.0f, dy + 195.0f, 230.0f, 34.0f } },
            { "6. Detroit (1972)", { dx + 280.0f, dy + 195.0f, 230.0f, 34.0f } },
            { "7. Boston (2010)", { dx + 30.0f, dy + 245.0f, 230.0f, 34.0f } }
        };

        for (size_t i = 0; i < sizeof(sc_display) / sizeof(sc_display[0]); i++) {
            SDL_FRect r = sc_display[i].rect;
            SDL_SetRenderDrawColor(renderer, 35, 45, 62, 255);
            SDL_RenderFillRect(renderer, &r);
            SDL_SetRenderDrawColor(renderer, 70, 90, 130, 255);
            SDL_RenderRect(renderer, &r);
            GUI_DrawText(renderer, (int)r.x + 12, (int)r.y + 8, sc_display[i].title, col_white);
        }
    } else if (active_file_tab == FILE_TAB_OPTIONS) {
        GUI_DrawText(renderer, cx, cy, "SIMULATION PREFERENCES & TOGGLES", col_gold);

        struct {
            const char *label;
            bool state;
        } opts[] = {
            { "Auto-Budget Processing", (bool)autoBudget },
            { "Auto-Bulldoze Confirmation", (bool)autoBulldoze },
            { "Auto-Goto Incident Coordinates", (bool)autoGo },
            { "Spontaneous Disasters Enabled", (bool)(!NoDisasters) },
            { "Sound Effects & Audio Playback", (bool)UserSoundOn }
        };

        float opt_x = dx + 340.0f;
        float opt_y = dy + 105.0f;

        for (int i = 0; i < 5; i++) {
            int row_y = (int)opt_y + (i * 35);
            GUI_DrawText(renderer, cx + 10, row_y + 4, opts[i].label, col_white);

            SDL_FRect toggle_btn = { opt_x, (float)row_y, 90.0f, 26.0f };
            SDL_SetRenderDrawColor(renderer, opts[i].state ? 30 : 45, opts[i].state ? 70 : 25, opts[i].state ? 40 : 25, 255);
            SDL_RenderFillRect(renderer, &toggle_btn);

            SDL_SetRenderDrawColor(renderer, opts[i].state ? 80 : 180, opts[i].state ? 220 : 60, opts[i].state ? 100 : 60, 255);
            SDL_RenderRect(renderer, &toggle_btn);

            const char *status_str = opts[i].state ? "[ ON ]" : "[ OFF ]";
            GUI_DrawText(renderer, (int)toggle_btn.x + 18, (int)toggle_btn.y + 5, status_str, opts[i].state ? col_green : col_red);
        }
    }
}