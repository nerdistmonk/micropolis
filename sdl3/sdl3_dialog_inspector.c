/* sdl3_dialog_inspector.c: Zone Query Inspector dialog.
 */

#include "sdl3_dialog_common.h"

static int32_t inspect_tx = 0;
static int32_t inspect_ty = 0;
static char inspect_title[64] = {0};
static char inspect_stat_density[64] = {0};
static char inspect_stat_power[64] = {0};
static char inspect_stat_pollution[64] = {0};
static char inspect_stat_crime[64] = {0};
static char inspect_stat_landvalue[64] = {0};
static char inspect_stat_growth[64] = {0};
static char inspect_stat_water[64] = {0};

void Dialog_Inspector_Show(int32_t tile_x, int32_t tile_y)
{
    if (!TestBounds(tile_x, tile_y)) return;

    inspect_tx = tile_x;
    inspect_ty = tile_y;

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
    } else if (low >= WATERTOWER && low < WATERTOWER + WATERTOWER_TILES) {
        strncpy(inspect_title, "Water Tower", sizeof(inspect_title));
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

    if (low >= WATERTOWER && low < WATERTOWER + WATERTOWER_TILES) {
        snprintf(inspect_stat_water, sizeof(inspect_stat_water), "Water Source: %s",
                 WaterTowerHasSource(tile_x, tile_y) ? "River in range" : "No water in range");
    } else if (WaterIsSupplied(tile_x, tile_y)) {
        snprintf(inspect_stat_water, sizeof(inspect_stat_water), "Water Supply: Watered");
    } else if (TILE_IS_POWER_PLANT(tile)) {
        snprintf(inspect_stat_water, sizeof(inspect_stat_water), "Water Supply: None (plant offline)");
    } else {
        snprintf(inspect_stat_water, sizeof(inspect_stat_water), "Water Supply: None");
    }

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

static SDL_FRect InspectorOkRect(float dx, float dy, float dw, float dh)
{
    float w = GUI_ButtonWidth("OK", 24.0f);
    SDL_FRect r = { dx + (dw - w) / 2.0f, dy + dh - 42.0f, w, 28.0f };
    return r;
}

bool Dialog_Inspector_HandleClick(float mx, float my)
{
    float dw = 404.0f;
    float dh = 300.0f;
    float dx = DialogCenteredX(dw);
    float dy = DialogCenteredY(dh);

    if (mx >= dx + dw - 28.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 28.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    SDL_FRect ok_btn = InspectorOkRect(dx, dy, dw, dh);
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

void Dialog_Inspector_Render(SDL_Renderer *renderer)
{
    float dw = 404.0f;
    float dh = 300.0f;
    float dx = DialogCenteredX(dw);
    float dy = DialogCenteredY(dh);

    SDL_FRect scrim = { 0.0f, 0.0f, (float)Video_GetCanvasWidth(), (float)Video_GetCanvasHeight() };
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
    GUI_DrawText(renderer, (int)dx + 24, cy + 144, inspect_stat_water, col_cyan);

    SDL_FRect ok_btn = InspectorOkRect(dx, dy, dw, dh);
    SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
    SDL_RenderFillRect(renderer, &ok_btn);
    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &ok_btn);
    GUI_DrawTextCentered(renderer, ok_btn, "OK", col_white);
}