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
static char inspect_stat_lvfactors[7][72];
static int inspect_lvfactor_lines = 0;
static char inspect_stat_services[2][80] = {{0}};
static float inspect_dh = 512.0f;
static float inspect_dw = 404.0f;
static char inspect_stat_growth[64] = {0};
static char inspect_stat_water[64] = {0};
static char inspect_stat_supply[96] = {0};
static char inspect_stat_network[96] = {0};

/* Widens the window to fit the longest line. Stat lines start at +24 and breakdown lines at +40. */
static void InspectorFitWidth(void)
{
    const char *lines[] = {
        inspect_stat_density, inspect_stat_power, inspect_stat_pollution, inspect_stat_crime,
        inspect_stat_landvalue, inspect_stat_growth, inspect_stat_water, inspect_stat_supply,
        inspect_stat_network, inspect_stat_services[0], inspect_stat_services[1],
    };
    int widest = 0;
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        int w = GUI_GetTextWidth(lines[i]);
        if (w > widest) widest = w;
    }
    for (int i = 0; i < inspect_lvfactor_lines; i++) {
        int w = GUI_GetTextWidth(inspect_stat_lvfactors[i]) + 16;
        if (w > widest) widest = w;
    }
    float need = (float)widest + 24.0f + 24.0f;
    inspect_dw = need > 404.0f ? need : 404.0f;
}

/* Offset of the land-value block from the stats origin (cy). It keeps its usual row unless the network and service rows push it lower. */
static int InspectorLandValueOffset(void)
{
    int row = 192;
    if (inspect_stat_network[0]) row += 24;
    for (int i = 0; i < 2; i++) {
        if (inspect_stat_services[i][0]) row += 20;
    }
    return row + 8 > 240 ? row + 8 : 240;
}

/* Grows the dialog so the last land-value line clears the OK button. */
static void InspectorFitHeight(void)
{
    float bottom = 82.0f + (float)InspectorLandValueOffset() + 20.0f * (float)(inspect_lvfactor_lines - 1) + 16.0f;
    float need = bottom + 50.0f;
    inspect_dh = need > 512.0f ? need : 512.0f;
}

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
    } else if (low >= RESBASE && low < HOSPITAL - 4) {
        strncpy(inspect_title, "Residential Zone", sizeof(inspect_title));
    } else if (low >= HOSPITAL - 4 && low <= HOSPITAL + 4) {
        /* The hospital is a 3x3 block; only its centre is HOSPITAL. */
        strncpy(inspect_title, "Municipal Hospital", sizeof(inspect_title));
    } else if (low >= CHURCH - 4 && low <= CHURCH + 4) {
        strncpy(inspect_title, "Parish Church", sizeof(inspect_title));
    } else if (low >= COMBASE && low < INDBASE) {
        strncpy(inspect_title, "Commercial Zone", sizeof(inspect_title));
    } else if (low >= INDBASE && low < PORTBASE) {
        strncpy(inspect_title, "Industrial Zone", sizeof(inspect_title));
    } else if (low >= PORTBASE && low <= LASTPORT) {
        strncpy(inspect_title, "Commercial Seaport", sizeof(inspect_title));
    } else if (low >= AIRPORTBASE && low < (AIRPORTBASE + 36)) {
        strncpy(inspect_title, "Municipal Airport", sizeof(inspect_title));
    } else if (low >= COALBASE && low <= LASTPOWERPLANT) {
        strncpy(inspect_title, "Coal Power Plant", sizeof(inspect_title));
    } else if (low >= NUCLEARBASE && low < (NUCLEARBASE + 16)) {
        strncpy(inspect_title, "Nuclear Power Plant", sizeof(inspect_title));
    } else if (low >= STADIUMBASE && low < (STADIUMBASE + 32)) {
        strncpy(inspect_title, "Sports Stadium", sizeof(inspect_title));
    } else if (low >= FIRESTBASE && low < (FIRESTBASE + 9)) {
        strncpy(inspect_title, "Fire Department", sizeof(inspect_title));
    } else if (low >= POLICESTBASE && low < (POLICESTBASE + 9)) {
        strncpy(inspect_title, "Police Headquarters", sizeof(inspect_title));
    } else if (low >= FIRE && low <= LASTFIRE) {
        strncpy(inspect_title, "Raging Firestorm!", sizeof(inspect_title));
    } else if (low == RADTILE) {
        strncpy(inspect_title, "Radioactive Contamination!", sizeof(inspect_title));
    } else if (low >= WATERTOWER && low < WATERTOWER + WATERTOWER_TILES) {
        strncpy(inspect_title, "Water Tower", sizeof(inspect_title));
    } else if (low >= FOUNTAIN && low < FOUNTAIN + FOUNTAIN_FRAMES) {
        strncpy(inspect_title, "Municipal Park", sizeof(inspect_title));
    } else if (low >= LANDFILL && low < LANDFILL + LANDFILL_TILES) {
        strncpy(inspect_title, "Landfill", sizeof(inspect_title));
    } else if (TILE_IS_GARAGE(tile)) {
        strncpy(inspect_title, "Parking Garage", sizeof(inspect_title));
    } else if (TILE_IS_STATION(tile)) {
        strncpy(inspect_title, "Passenger Station", sizeof(inspect_title));
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
                 WaterTowerHasSource(tile_x, tile_y) ? "Pipe to river" : "No river pipe");
    } else if (WaterIsSupplied(tile_x, tile_y)) {
        snprintf(inspect_stat_water, sizeof(inspect_stat_water), "Water Supply: Watered");
    } else if (TILE_IS_POWER_PLANT(tile)) {
        snprintf(inspect_stat_water, sizeof(inspect_stat_water), "Water Supply: None (plant offline)");
    } else {
        snprintf(inspect_stat_water, sizeof(inspect_stat_water), "Water Supply: None");
    }

    if (TILE_IS_LANDFILL(tile)) {
        snprintf(inspect_stat_power, sizeof(inspect_stat_power), "Power Grid: Not required");
        snprintf(inspect_stat_water, sizeof(inspect_stat_water), "Water Supply: Not required");
    }

    inspect_stat_supply[0] = '\0';
    int32_t demand = 0;
    int32_t capacity = 0;
    if (TILE_IS_POWER_PLANT(tile)) {
        bool found = false;
        for (int32_t cy = tile_y - 2; cy <= tile_y + 1 && !found; cy++) {
            for (int32_t cx = tile_x - 2; cx <= tile_x + 1 && !found; cx++) {
                if (!TestBounds(cx, cy)) continue;
                uint16_t center = Map[cx][cy] & LOMASK;
                if (center != POWERPLANT && center != NUCLEAR) continue;
                found = PowerGridStats(cx, cy, &demand, &capacity);
            }
        }
        if (found) {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Power Demand: %d zones | Capacity: %d zones",
                     (int)demand, (int)capacity);
        } else {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Power Demand: none (plant offline)");
        }
    } else if (low >= WATERTOWER && low < WATERTOWER + WATERTOWER_TILES) {
        if (WaterNetworkStats(tile_x, tile_y, &demand, &capacity)) {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Water Demand: %d zones | Capacity: %d zones",
                     (int)demand, (int)capacity);
        } else {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Water Demand: none (no river in range)");
        }
    } else if (low >= POLICESTBASE && low < (POLICESTBASE + 9)) {
        PoliceSystemStats(&capacity, &demand);
        snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Police Capacity: %d buildings | Demand: %d buildings (citywide)",
                 (int)capacity, (int)demand);
    } else if (low >= FIRESTBASE && low < (FIRESTBASE + 9)) {
        FireSystemStats(&capacity, &demand);
        snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Fire Capacity: %d buildings | Demand: %d buildings (citywide)",
                 (int)capacity, (int)demand);
    } else if (low >= HOSPITAL - 4 && low <= HOSPITAL + 4) {
        HospitalSystemStats(&capacity, &demand);
        snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Hospital Capacity: %d buildings | Demand: %d buildings (citywide)",
                 (int)capacity, (int)demand);
    } else if ((low >= FOUNTAIN && low < FOUNTAIN + FOUNTAIN_FRAMES) || (low >= STADIUMBASE && low < STADIUMBASE + 32)) {
        RecSystemStats(&capacity, &demand);
        snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Recreation Capacity: %d buildings | Demand: %d buildings (citywide)",
                 (int)capacity, (int)demand);
    } else if ((low >= PORTBASE && low <= LASTPORT) || (low >= AIRPORTBASE && low < (AIRPORTBASE + 36))) {
        int32_t port_cap = 0;
        int32_t port_demand = 0;
        PortSystemStats(&port_cap, &port_demand);
        int32_t served = port_demand < port_cap ? port_demand : port_cap;
        snprintf(inspect_stat_supply, sizeof(inspect_stat_supply),
                 "%s Service: %d zones each | Citywide: %d of %d zones served",
                 (low >= AIRPORTBASE) ? "Airport" : "Port",
                 (low >= AIRPORTBASE) ? AIRPORT_SERVICE_ZONES : PORT_SERVICE_ZONES,
                 (int)served, (int)port_demand);
    } else if (TILE_IS_LANDFILL(tile)) {
        LandfillSystemStats(&capacity, &demand);
        snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Landfill Capacity: %d zones | Demand: %d zones (citywide)",
                 (int)capacity, (int)demand);
    } else if (TILE_IS_GARAGE(tile)) {
        if (!CapacityBlockHasAccess(tile_x, tile_y)) {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Road Capacity: none (no adjacent road)");
        } else if (!CapacityBlockPowered(tile_x, tile_y)) {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Road Capacity: none (no power)");
        } else if (!CapacityBlockWatered(tile_x, tile_y)) {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Road Capacity: none (no water)");
        } else {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Road Capacity: +%d zones", GARAGE_ZONES);
        }
    } else if (TILE_IS_STATION(tile)) {
        if (!CapacityBlockHasAccess(tile_x, tile_y)) {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Road Capacity: none (no adjacent rail)");
        } else if (!CapacityBlockPowered(tile_x, tile_y)) {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Road Capacity: none (no power)");
        } else if (!CapacityBlockWatered(tile_x, tile_y)) {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Road Capacity: none (no water)");
        } else {
            snprintf(inspect_stat_supply, sizeof(inspect_stat_supply), "Road Capacity: +%d zones", STATION_ZONES);
        }
    }

    inspect_stat_network[0] = '\0';
    if ((low >= ROADBASE && low <= LASTROAD) || (low >= RAILBASE && low <= LASTRAIL) || TILE_IS_TRAFFIC_BLOCK(tile)) {
        int32_t net_demand = 0;
        int32_t net_capacity = 0;
        TrafNetworkStats(&net_demand, &net_capacity);
        snprintf(inspect_stat_network, sizeof(inspect_stat_network), "Network Demand: %d zones | Capacity: %d zones",
                 (int)net_demand, (int)net_capacity);
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

    LandValueFactors lvf;
    LandValueFactorsOf(hx, hy, &lvf);
    snprintf(inspect_stat_lvfactors[0], sizeof(inspect_stat_lvfactors[0]), "Land value breakdown:");
    if (lvf.access) {
        snprintf(inspect_stat_lvfactors[1], sizeof(inspect_stat_lvfactors[1]),
                 "Near city centre: %+d (max +136)", (int)lvf.centre);
        snprintf(inspect_stat_lvfactors[2], sizeof(inspect_stat_lvfactors[2]),
                 "Nearby water and trees: %+d", (int)lvf.terrain);
        snprintf(inspect_stat_lvfactors[3], sizeof(inspect_stat_lvfactors[3]),
                 "%s: %+d", LandfillTrashPenalty() > 0 ? "Pollution, incl. trash" : "Pollution nearby", (int)lvf.pollution);
        snprintf(inspect_stat_lvfactors[4], sizeof(inspect_stat_lvfactors[4]),
                 "Crime nearby: %+d (-20 when very high)", (int)lvf.crime);
        snprintf(inspect_stat_lvfactors[5], sizeof(inspect_stat_lvfactors[5]),
                 "Neighbor links: +%d%%", Neighbor_LandValueBonusPercent());
        snprintf(inspect_stat_lvfactors[6], sizeof(inspect_stat_lvfactors[6]),
                 "Total: %d, shown as $%d", (int)LandValueWithNeighbors(lvf.value),
                 (int)LandValueWithNeighbors(lvf.value) * 50);
        inspect_lvfactor_lines = 7;
    } else {
        snprintf(inspect_stat_lvfactors[1], sizeof(inspect_stat_lvfactors[1]), "%s",
                 lvf.zoned ? "No road touching this zone, so land value is 0"
                           : "Not a zone, so land value is 0");
        inspect_lvfactor_lines = 2;
    }

    int16_t rog = RateOGMem[sx][sy];
    snprintf(inspect_stat_growth, sizeof(inspect_stat_growth), "Growth Trend: %s (%+d)", (rog > 10) ? "Booming" : (rog < -10 ? "Declining" : "Stable"), rog);

    inspect_stat_services[0][0] = '\0';
    inspect_stat_services[1][0] = '\0';
    if (PoliceZoneState(tile_x, tile_y) != POLICE_ZONE_NONE) {
        snprintf(inspect_stat_services[0], sizeof(inspect_stat_services[0]), "Police: %s    Fire: %s",
                 PoliceZoneState(tile_x, tile_y) == POLICE_ZONE_COVERED ? "Covered" : "Uncovered",
                 FireZoneState(tile_x, tile_y) == FIRE_ZONE_COVERED ? "Covered" : "Uncovered");
        snprintf(inspect_stat_services[1], sizeof(inspect_stat_services[1]), "Health: %s    Parks: %s",
                 HealthIsCovered(tile_x, tile_y) ? "Covered" : "Uncovered",
                 RecIsCovered(tile_x, tile_y) ? "Covered" : "Uncovered");
    }

    InspectorFitWidth();
    InspectorFitHeight();
}

static SDL_FRect InspectorOkRect(float dx, float dy, float dw, float dh)
{
    float w = GUI_ButtonWidth("OK", 24.0f);
    SDL_FRect r = { dx + (dw - w) / 2.0f, dy + dh - 42.0f, w, 28.0f };
    return r;
}

bool Dialog_Inspector_HandleClick(float mx, float my)
{
    float dw = inspect_dw;
    float dh = inspect_dh;
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
    float dw = inspect_dw;
    float dh = inspect_dh;
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
    if (inspect_stat_supply[0]) {
        GUI_DrawText(renderer, (int)dx + 24, cy + 168, inspect_stat_supply, col_gold);
    }
    int row = cy + 192;
    if (inspect_stat_network[0]) {
        GUI_DrawText(renderer, (int)dx + 24, row, inspect_stat_network, col_gold);
        row += 24;
    }
    for (int i = 0; i < 2; i++) {
        if (inspect_stat_services[i][0]) {
            GUI_DrawText(renderer, (int)dx + 24, row, inspect_stat_services[i], col_white);
            row += 20;
        }
    }
    int lv_y = cy + InspectorLandValueOffset();
    for (int i = 0; i < inspect_lvfactor_lines; i++) {
        SDL_Color c = (i == 0 || i == inspect_lvfactor_lines - 1) ? col_gold : col_white;
        GUI_DrawText(renderer, (int)dx + 40, lv_y + 20 * i, inspect_stat_lvfactors[i], c);
    }

    SDL_FRect ok_btn = InspectorOkRect(dx, dy, dw, dh);
    SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
    SDL_RenderFillRect(renderer, &ok_btn);
    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &ok_btn);
    GUI_DrawTextCentered(renderer, ok_btn, "OK", col_white);
}