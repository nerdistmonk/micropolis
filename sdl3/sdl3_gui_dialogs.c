/* sdl3_gui_dialogs.c: Implementation of City Hall Desk, Map Overlays, Scenario Result, File Operations, Disasters, and Inspector modals.
 */

#include "sdl3_gui_dialogs.h"
#include "sdl3_gui_text.h"
#include "sdl3_audio.h"
#include "sdl3_video.h"
#include "sim.h"
#include <stdio.h>
#include <string.h>

static GuiModalId active_modal = GUI_MODAL_NONE;
static CityMgmtTab active_city_tab = CITY_TAB_BUDGET;
static FileDialogTab active_file_tab = FILE_TAB_SAVE_LOAD;
static OverlayViewMode active_overlay_mode = OVERLAY_VIEW_NORMAL;

static bool scenario_won = false;
static char scenario_result_msg[256] = {0};

/* Tracks if the budget modal was opened pending annual year-end collection */
static bool pending_annual_budget = false;

/* Graphs Tab State */
static bool graph_120_year_mode = false;
static bool graph_metric_visible[HISTORIES] = { true, true, true, true, true, true };

/* Editing City Name State */
static bool is_editing_city_name = false;
static char city_name_input_buffer[64] = {0};

/* File Modal Input State */
static bool is_editing_filename = false;
static char file_name_input_buffer[64] = "mycity.cty";
static char file_status_message[128] = {0};
static SDL_Color file_status_color = { 245, 245, 250, 255 };

/* Query Inspector State */
static int32_t inspect_tx = 0;
static int32_t inspect_ty = 0;
static char inspect_title[64] = {0};
static char inspect_stat_density[64] = {0};
static char inspect_stat_power[64] = {0};
static char inspect_stat_pollution[64] = {0};
static char inspect_stat_crime[64] = {0};
static char inspect_stat_landvalue[64] = {0};
static char inspect_stat_growth[64] = {0};

static const SDL_Color col_white = { 245, 245, 250, 255 };
static const SDL_Color col_gold = { 255, 215, 0, 255 };
static const SDL_Color col_green = { 100, 255, 120, 255 };
static const SDL_Color col_cyan = { 90, 210, 255, 255 };
static const SDL_Color col_red = { 255, 90, 90, 255 };
static const SDL_Color col_gray = { 160, 168, 185, 255 };

bool GUI_Dialogs_IsModalOpen(void)
{
    return (active_modal != GUI_MODAL_NONE);
}

void GUI_Dialogs_OpenCityMgmt(CityMgmtTab initial_tab)
{
    active_modal = GUI_MODAL_CITY_MGMT;
    active_city_tab = initial_tab;
    is_editing_city_name = false;
    is_editing_filename = false;
    if (initial_tab == CITY_TAB_BUDGET && autoBudget == 0) {
        pending_annual_budget = true;
    }
}

void GUI_Dialogs_OpenOverlays(void)
{
    active_modal = GUI_MODAL_OVERLAYS;
    is_editing_city_name = false;
    is_editing_filename = false;
}

void GUI_Dialogs_OpenFile(FileDialogTab initial_tab)
{
    active_modal = GUI_MODAL_FILE;
    active_file_tab = initial_tab;
    is_editing_city_name = false;
    is_editing_filename = false;
    file_status_message[0] = '\0';
}

void GUI_Dialogs_OpenDisasters(void)
{
    active_modal = GUI_MODAL_DISASTERS;
    is_editing_city_name = false;
    is_editing_filename = false;
}

void GUI_Dialogs_ShowScenarioResult(bool won, const char *msg)
{
    active_modal = GUI_MODAL_SCENARIO_RESULT;
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

void GUI_Dialogs_ShowQueryInspector(int32_t tile_x, int32_t tile_y)
{
    if (!TestBounds(tile_x, tile_y)) return;

    inspect_tx = tile_x;
    inspect_ty = tile_y;
    active_modal = GUI_MODAL_QUERY_INSPECTOR;

    uint16_t tile = Map[tile_x][tile_y];
    uint16_t low = tile & LOMASK;

    if (low == RIVER || low == CHANNEL || (low >= FIRSTRIVEDGE && low <= LASTRIVEDGE)) {
        strncpy(inspect_title, "Waterway / River", sizeof(inspect_title));
    } else if (low >= TREEBASE && low <= LASTTREE) {
        strncpy(inspect_title, "Woodlands / Parklands", sizeof(inspect_title));
    } else if (low >= ROADBASE && low <= LASTROAD) {
        strncpy(inspect_title, "Paved Roadway", sizeof(inspect_title));
    } else if (low >= RAILBASE && low <= LASTRAIL) {
        strncpy(inspect_title, "Rail Transit Line", sizeof(inspect_title));
    } else if (low >= POWERBASE && low <= LASTPOWER) {
        strncpy(inspect_title, "High-Voltage Power Line", sizeof(inspect_title));
    } else if (low >= RESBASE && low < HOSPITAL) {
        strncpy(inspect_title, "Residential Zone", sizeof(inspect_title));
    } else if (low == HOSPITAL) {
        strncpy(inspect_title, "Municipal Hospital", sizeof(inspect_title));
    } else if (low == CHURCH) {
        strncpy(inspect_title, "Parish Church", sizeof(inspect_title));
    } else if (low >= COMBASE && low < INDBASE) {
        strncpy(inspect_title, "Commercial Zone", sizeof(inspect_title));
    } else if (low >= INDBASE && low < PORTBASE) {
        strncpy(inspect_title, "Industrial Zone", sizeof(inspect_title));
    } else if (low >= PORT && low < (PORT + 16)) {
        strncpy(inspect_title, "Commercial Seaport", sizeof(inspect_title));
    } else if (low >= AIRPORT && low < (AIRPORT + 36)) {
        strncpy(inspect_title, "Municipal Airport", sizeof(inspect_title));
    } else if (low >= POWERPLANT && low < (POWERPLANT + 16)) {
        strncpy(inspect_title, "Coal Power Plant", sizeof(inspect_title));
    } else if (low >= NUCLEAR && low < (NUCLEAR + 16)) {
        strncpy(inspect_title, "Nuclear Power Plant", sizeof(inspect_title));
    } else if (low >= STADIUM && low < (STADIUM + 16)) {
        strncpy(inspect_title, "Sports Stadium", sizeof(inspect_title));
    } else if (low >= FIRESTATION && low < (FIRESTATION + 9)) {
        strncpy(inspect_title, "Fire Department", sizeof(inspect_title));
    } else if (low >= POLICESTATION && low < (POLICESTATION + 9)) {
        strncpy(inspect_title, "Police Headquarters", sizeof(inspect_title));
    } else if (low >= FIRE && low <= LASTFIRE) {
        strncpy(inspect_title, "Raging Firestorm!", sizeof(inspect_title));
    } else if (low == RADTILE) {
        strncpy(inspect_title, "Radioactive Contamination!", sizeof(inspect_title));
    } else if (low >= RUBBLE && low <= LASTRUBBLE) {
        strncpy(inspect_title, "Demolished Rubble", sizeof(inspect_title));
    } else {
        strncpy(inspect_title, "Undeveloped Land", sizeof(inspect_title));
    }

    int32_t hx = tile_x >> 1;
    int32_t hy = tile_y >> 1;
    int32_t sx = tile_x >> 3;
    int32_t sy = tile_y >> 3;

    uint8_t pd = PopDensity[hx][hy];
    snprintf(inspect_stat_density, sizeof(inspect_stat_density), "Density: %d citizens/block", (int)pd * 8);

    QUAD pw = POWERWORD(tile_x, tile_y);
    bool powered = (pw < PWRMAPSIZE) && (PowerMap[pw] & (uint16_t)(1u << (tile_x & 15)));
    snprintf(inspect_stat_power, sizeof(inspect_stat_power), "Power Grid: %s", powered ? "Energized (100%)" : "No Power");

    uint8_t pol = PollutionMem[hx][hy];
    const char *pol_desc = (pol < 30) ? "Clean Air" : (pol < 80 ? "Moderate Smog" : "Heavy Pollution");
    snprintf(inspect_stat_pollution, sizeof(inspect_stat_pollution), "Pollution: %s (%d)", pol_desc, pol);

    uint8_t crm = CrimeMem[hx][hy];
    const char *crm_desc = (crm < 30) ? "Very Safe" : (crm < 90 ? "Moderate" : "Dangerously High");
    snprintf(inspect_stat_crime, sizeof(inspect_stat_crime), "Crime Level: %s (%d)", crm_desc, crm);

    uint8_t lv = LandValueMem[hx][hy];
    const char *lv_desc = (lv < 50) ? "Low" : (lv < 150 ? "Medium" : (lv < 200 ? "High" : "Prime Estate"));
    snprintf(inspect_stat_landvalue, sizeof(inspect_stat_landvalue), "Land Value: %s ($%d)", lv_desc, lv * 50);

    int16_t rog = RateOGMem[sx][sy];
    snprintf(inspect_stat_growth, sizeof(inspect_stat_growth), "Growth Trend: %s (%+d)", (rog > 10) ? "Booming" : (rog < -10 ? "Declining" : "Stable"), rog);
}

void GUI_Dialogs_Close(void)
{
    if (active_modal == GUI_MODAL_CITY_MGMT && pending_annual_budget) {
        ApplyBudgetCommit();
        pending_annual_budget = false;
        if (SimSpeed == 0) {
            SetSimSpeed(3);
        }
    }

    active_modal = GUI_MODAL_NONE;
    if (is_editing_city_name || is_editing_filename) {
        SDL_StopTextInput(Video_GetWindow());
        is_editing_city_name = false;
        is_editing_filename = false;
    }
}

OverlayViewMode GUI_Dialogs_GetActiveOverlay(void)
{
    return active_overlay_mode;
}

void GUI_Dialogs_SetActiveOverlay(OverlayViewMode mode)
{
    active_overlay_mode = mode;
}

bool GUI_Dialogs_HandleTextInput(const char *text)
{
    if (!text) return false;

    if (is_editing_city_name) {
        size_t cur = strlen(city_name_input_buffer);
        size_t add = strlen(text);
        if (cur + add < sizeof(city_name_input_buffer) - 1) {
            strcat(city_name_input_buffer, text);
        }
        return true;
    }

    if (is_editing_filename) {
        size_t cur = strlen(file_name_input_buffer);
        size_t add = strlen(text);
        if (cur + add < sizeof(file_name_input_buffer) - 1) {
            strcat(file_name_input_buffer, text);
        }
        return true;
    }

    return false;
}

bool GUI_Dialogs_HandleKeyDown(SDL_Keycode key)
{
    if (active_modal == GUI_MODAL_NONE) return false;

    if (key == SDLK_ESCAPE) {
        GUI_Dialogs_Close();
        return true;
    }

    if (active_modal == GUI_MODAL_SCENARIO_RESULT || active_modal == GUI_MODAL_QUERY_INSPECTOR) {
        if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_KP_ENTER) {
            GUI_Dialogs_Close();
            return true;
        }
    }

    if (is_editing_city_name) {
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
    }

    if (is_editing_filename) {
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
    }

    return true;
}

static bool HandleCityMgmtClick(float mx, float my)
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

        /* Accept / Commit Budget button */
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

static bool HandleOverlaysClick(float mx, float my)
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
            active_overlay_mode = overlay_btns[i].mode;
            return true;
        }
    }

    return true;
}

static bool HandleFileModalClick(float mx, float my)
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

        /* Auto-Budget toggle */
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y && my <= opt_y + 26.0f) {
            autoBudget = !autoBudget;
            return true;
        }
        /* Auto-Bulldoze toggle */
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 35.0f && my <= opt_y + 61.0f) {
            autoBulldoze = !autoBulldoze;
            return true;
        }
        /* Auto-Goto Disaster toggle */
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 70.0f && my <= opt_y + 96.0f) {
            autoGo = !autoGo;
            return true;
        }
        /* Disasters Enabled toggle */
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 105.0f && my <= opt_y + 131.0f) {
            NoDisasters = !NoDisasters;
            return true;
        }
        /* User Sound toggle */
        if (mx >= opt_x && mx <= opt_x + 90.0f && my >= opt_y + 140.0f && my <= opt_y + 166.0f) {
            UserSoundOn = !UserSoundOn;
            return true;
        }
    }

    return true;
}

static bool HandleDisastersModalClick(float mx, float my)
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

static bool HandleScenarioResultClick(float mx, float my)
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

static bool HandleQueryInspectorClick(float mx, float my)
{
    float dx = 310.0f;
    float dy = 220.0f;
    float dw = 404.0f;
    float dh = 300.0f;

    if (mx >= dx + dw - 28.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 28.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    SDL_FRect ok_btn = { dx + (dw - 110.0f) / 2.0f, dy + dh - 42.0f, 110.0f, 28.0f };
    if (mx >= ok_btn.x && mx <= ok_btn.x + ok_btn.w &&
        my >= ok_btn.y && my <= ok_btn.y + ok_btn.h) {
        GUI_Dialogs_Close();
        return true;
    }

    if (mx < dx || mx > dx + dw || my < dy || my > dy + dh) {
        GUI_Dialogs_Close();
        return true;
    }

    return true;
}

bool GUI_Dialogs_HandleClick(float mx, float my)
{
    if (active_modal == GUI_MODAL_CITY_MGMT) {
        return HandleCityMgmtClick(mx, my);
    }
    if (active_modal == GUI_MODAL_OVERLAYS) {
        return HandleOverlaysClick(mx, my);
    }
    if (active_modal == GUI_MODAL_FILE) {
        return HandleFileModalClick(mx, my);
    }
    if (active_modal == GUI_MODAL_DISASTERS) {
        return HandleDisastersModalClick(mx, my);
    }
    if (active_modal == GUI_MODAL_SCENARIO_RESULT) {
        return HandleScenarioResultClick(mx, my);
    }
    if (active_modal == GUI_MODAL_QUERY_INSPECTOR) {
        return HandleQueryInspectorClick(mx, my);
    }
    return false;
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

static void RenderCityMgmtModal(SDL_Renderer *renderer)
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

        /* Commit / Continue Button */
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

static void RenderOverlaysModal(SDL_Renderer *renderer)
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

    for (size_t i = 0; i < sizeof(overlay_btns) / sizeof(overlay_btns[0]); i++) {
        bool selected = (active_overlay_mode == overlay_btns[i].mode);
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
    snprintf(buf, sizeof(buf), "Active Layer: [%s]", layer_names[active_overlay_mode]);
    GUI_DrawText(renderer, (int)dx + 30, (int)dy + 300, buf, col_cyan);
}

static void RenderFileModal(SDL_Renderer *renderer)
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

static void RenderDisastersModal(SDL_Renderer *renderer)
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

static void RenderScenarioResultModal(SDL_Renderer *renderer)
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

static void RenderQueryInspectorModal(SDL_Renderer *renderer)
{
    float dx = 310.0f;
    float dy = 220.0f;
    float dw = 404.0f;
    float dh = 300.0f;

    SDL_FRect scrim = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 22, 26, 36, 252);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    SDL_RenderRect(renderer, &win_rect);

    char header[96];
    snprintf(header, sizeof(header), "ZONE INSPECTOR (%d, %d)", inspect_tx, inspect_ty);
    GUI_DrawText(renderer, (int)dx + 16, (int)dy + 14, header, col_gold);

    SDL_FRect close_rect = { dx + dw - 26.0f, dy + 8.0f, 18.0f, 18.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 5, (int)close_rect.y + 3, "X", col_white);

    GUI_DrawText(renderer, (int)dx + 20, (int)dy + 46, inspect_title, col_cyan);

    SDL_SetRenderDrawColor(renderer, 50, 60, 80, 255);
    SDL_RenderLine(renderer, dx + 16.0f, dy + 70.0f, dx + dw - 16.0f, dy + 70.0f);

    int cy = (int)dy + 82;
    GUI_DrawText(renderer, (int)dx + 24, cy, inspect_stat_density, col_white);
    GUI_DrawText(renderer, (int)dx + 24, cy + 24, inspect_stat_power, col_white);
    GUI_DrawText(renderer, (int)dx + 24, cy + 48, inspect_stat_pollution, col_white);
    GUI_DrawText(renderer, (int)dx + 24, cy + 72, inspect_stat_crime, col_white);
    GUI_DrawText(renderer, (int)dx + 24, cy + 96, inspect_stat_landvalue, col_green);
    GUI_DrawText(renderer, (int)dx + 24, cy + 120, inspect_stat_growth, col_gold);

    SDL_FRect ok_btn = { dx + (dw - 110.0f) / 2.0f, dy + dh - 42.0f, 110.0f, 28.0f };
    SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
    SDL_RenderFillRect(renderer, &ok_btn);
    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &ok_btn);
    GUI_DrawText(renderer, (int)ok_btn.x + 40, (int)ok_btn.y + 6, "OK", col_white);
}

void GUI_Dialogs_Render(SDL_Renderer *renderer)
{
    if (active_modal == GUI_MODAL_CITY_MGMT) {
        RenderCityMgmtModal(renderer);
    } else if (active_modal == GUI_MODAL_OVERLAYS) {
        RenderOverlaysModal(renderer);
    } else if (active_modal == GUI_MODAL_FILE) {
        RenderFileModal(renderer);
    } else if (active_modal == GUI_MODAL_DISASTERS) {
        RenderDisastersModal(renderer);
    } else if (active_modal == GUI_MODAL_SCENARIO_RESULT) {
        RenderScenarioResultModal(renderer);
    } else if (active_modal == GUI_MODAL_QUERY_INSPECTOR) {
        RenderQueryInspectorModal(renderer);
    }
}