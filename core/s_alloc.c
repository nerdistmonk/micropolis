/* s_alloc.c: Simulation buffer memory allocation and global state definitions.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"
#include "animtab.h"

/* Simulation Coordinate State */
int16_t SMapX = 0;
int16_t SMapY = 0;
int16_t CChr = 0;
int16_t CChr9 = 0;

/* Transport & Infrastructure Population Counts */
int16_t RoadTotal = 0;
int16_t RailTotal = 0;
int16_t FirePop = 0;

/* Demographic Counts */
int16_t ResPop = 0;
int16_t ComPop = 0;
int16_t IndPop = 0;
int16_t TotalPop = 0;
int16_t LastTotalPop = 0;

int16_t ResZPop = 0;
int16_t ComZPop = 0;
int16_t IndZPop = 0;
int16_t TotalZPop = 0;

int16_t HospPop = 0;
int16_t ChurchPop = 0;
int16_t StadiumPop = 0;
int16_t PolicePop = 0;
int16_t FireStPop = 0;
int16_t CoalPop = 0;
int16_t NuclearPop = 0;
int16_t PortPop = 0;
int16_t APortPop = 0;

int16_t NeedChurch = 0;
int16_t CrimeAverage = 0;
int16_t PolluteAverage = 0;
int16_t LVAverage = 0;

/* Temporal & City State */
int32_t CityTime = 0;
int16_t StartingYear = 1900;
int16_t GameLevel = 0;
int16_t Cycle = 0;
char *MicropolisVersion = "4.0";
char CityName[64] = "Micropolis";
char *CityFileName = NULL;

/* Primary Map Data Grid (120 x 100) */
uint16_t *Map[WORLD_X];

/* Historical Census Data */
int32_t CensusChanged = 0;

int16_t *ResHis = NULL;
int16_t *ComHis = NULL;
int16_t *IndHis = NULL;
int16_t *MoneyHis = NULL;
int16_t *PollutionHis = NULL;
int16_t *CrimeHis = NULL;
int16_t *MiscHis = NULL;
uint16_t *PowerMap = NULL;

/* Incident & Crash Coordinates */
int16_t CrashX = 0;
int16_t CrashY = 0;
int32_t absDist = 0;

/* Sprite Slot State */
SimSprite *GlobalSprites[OBJN] = { NULL };

/* Message Dispatch Variables */
int16_t MessagePort = 0;
int16_t MesNum = 0;
int16_t MesX = 0;
int16_t MesY = 0;
int16_t HaveLastMessage = 0;
char LastMessage[256] = "";
uint32_t MessageSerial = 0;
int16_t MessageTargetX = 0;
int16_t MessageTargetY = 0;

/* Tool State */
int32_t OverRide = 0;
int32_t PendingTool = -1;

/* Budgetary Controls */
float roadPercent = 1.0f;
float policePercent = 1.0f;
float firePercent = 1.0f;
float healthPercent = 1.0f;
float recreationPercent = 1.0f;

int32_t RoadSpend = 0;
int32_t PoliceSpend = 0;
int32_t FireSpend = 0;
int32_t HealthSpend = 0;
int32_t RecSpend = 0;
int32_t RoadFund = 0;
int32_t PoliceFund = 0;
int32_t FireFund = 0;
int32_t HealthFund = 0;
int32_t RecFund = 0;
int16_t RoadEffect = 32;
int16_t PoliceEffect = 1000;
int16_t FireEffect = 1000;
int16_t HealthEffect = 1000;
int16_t RecEffect = 1000;
int32_t TaxFund = 0;
int32_t DealsIncome = 0;
int16_t ResTax = 7;
int16_t ComTax = 7;
int16_t IndTax = 7;
int32_t ResTaxPop = 0;
int32_t ResUncoveredPop = 0;
int32_t ResUnrecPop = 0;
int16_t TaxFlag = 0;
int32_t TotalFunds = 20000;

/* Tool Configurations */
int32_t CostOf[] = {
    100, 100, 100, 500,
    0, 500, 5, 1,
    20, 10, 0, 0,
    6000, 200, 3000, 3000,
    6000, 10000, 100, 500,
    2, 500, 100, 0,
    GARAGE_COST, STATION_COST
};

int16_t toolSize[] = {
    3, 3, 3, 3,
    1, 3, 1, 1,
    1, 1, 0, 0,
    4, 1, 4, 4,
    4, 6, 1, 3,
    1, 3, 3,
    0, 3, 3
};

int16_t toolOffset[] = {
    1, 1, 1, 1,
    0, 1, 0, 0,
    0, 0, 0, 0,
    1, 0, 1, 1,
    1, 1, 0, 1,
    0, 1, 1,
    0, 1, 1
};

/* Graphical Synchronization and Animation Tables */
uint8_t tileSynch = 0x01;
int32_t TilesAnimated = 0;
int32_t DoAnimation = 1;
int32_t DoMessages = 1;
int16_t flagBlink = 0;

/* 2x2 Resolution Simulation Matrices (60 x 50) */
Byte *PopDensity[HWLDX];
Byte *TrfDensity[HWLDX];
Byte *PollutionMem[HWLDX];
Byte *LandValueMem[HWLDX];
Byte *CrimeMem[HWLDX];
Byte *tem[HWLDX];
Byte *tem2[HWLDX];

/* 4x4 Resolution Simulation Matrices (30 x 25) */
Byte *TerrainMem[QWX];
Byte *Qtem[QWX];

/* 8x8 Resolution Simulation Matrices (15 x 13) */
int16_t RateOGMem[SmX][SmY];
int16_t ComRate[SmX][SmY];
int16_t STem[SmX][SmY];

/* Internal Base Storage Pointers */
static uint16_t *map_base = NULL;
static Byte *pop_base = NULL;
static Byte *trf_base = NULL;
static Byte *pol_base = NULL;
static Byte *land_base = NULL;
static Byte *crime_base = NULL;
static Byte *tem1_base = NULL;
static Byte *tem2_base = NULL;
static Byte *terrain_base = NULL;
static Byte *qtem_base = NULL;

/* Default Headless Callback Fallbacks */
static void default_update_funds(int32_t funds) { (void)funds; }
static void default_update_date(int32_t year, int32_t month) { (void)year; (void)month; }
static void default_show_message(int16_t msg, int16_t x, int16_t y) { (void)msg; (void)x; (void)y; }
static void default_auto_goto(int16_t x, int16_t y, const char *msg) { (void)x; (void)y; (void)msg; }
static void default_invalidate(void) {}
static void default_play_sound(int16_t id, int16_t x, int16_t y) { (void)id; (void)x; (void)y; }
static void default_make_sound(const char *c, const char *s) { (void)c; (void)s; }
static void default_request_budget_dialog(void) {}
static void default_news_event(int16_t event_id) { (void)event_id; }
static void default_news_reset(void) {}

SimCallbacks EngineCallbacks = {
    .update_funds = default_update_funds,
    .update_date = default_update_date,
    .show_message = default_show_message,
    .auto_goto = default_auto_goto,
    .invalidate_maps = default_invalidate,
    .invalidate_editors = default_invalidate,
    .play_sound = default_play_sound,
    .make_sound = default_make_sound,
    .request_budget_dialog = default_request_budget_dialog,
    .news_event = default_news_event,
    .news_reset = default_news_reset
};

void Sim_RegisterCallbacks(const SimCallbacks *cb)
{
    if (!cb) return;
    if (cb->update_funds) EngineCallbacks.update_funds = cb->update_funds;
    if (cb->update_date) EngineCallbacks.update_date = cb->update_date;
    if (cb->show_message) EngineCallbacks.show_message = cb->show_message;
    if (cb->auto_goto) EngineCallbacks.auto_goto = cb->auto_goto;
    if (cb->invalidate_maps) EngineCallbacks.invalidate_maps = cb->invalidate_maps;
    if (cb->invalidate_editors) EngineCallbacks.invalidate_editors = cb->invalidate_editors;
    if (cb->play_sound) EngineCallbacks.play_sound = cb->play_sound;
    if (cb->make_sound) EngineCallbacks.make_sound = cb->make_sound;
    if (cb->request_budget_dialog) EngineCallbacks.request_budget_dialog = cb->request_budget_dialog;
    if (cb->news_event) EngineCallbacks.news_event = cb->news_event;
    if (cb->news_reset) EngineCallbacks.news_reset = cb->news_reset;
}

void AllocateMapBuffers(void)
{
    map_base = (uint16_t *)calloc((size_t)(WORLD_X * WORLD_Y), sizeof(uint16_t));
    for (int32_t x = 0; x < WORLD_X; x++) {
        Map[x] = &map_base[x * WORLD_Y];
    }

    pop_base = (Byte *)calloc((size_t)(HWLDX * HWLDY), sizeof(Byte));
    trf_base = (Byte *)calloc((size_t)(HWLDX * HWLDY), sizeof(Byte));
    pol_base = (Byte *)calloc((size_t)(HWLDX * HWLDY), sizeof(Byte));
    land_base = (Byte *)calloc((size_t)(HWLDX * HWLDY), sizeof(Byte));
    crime_base = (Byte *)calloc((size_t)(HWLDX * HWLDY), sizeof(Byte));
    tem1_base = (Byte *)calloc((size_t)(HWLDX * HWLDY), sizeof(Byte));
    tem2_base = (Byte *)calloc((size_t)(HWLDX * HWLDY), sizeof(Byte));

    for (int32_t x = 0; x < HWLDX; x++) {
        PopDensity[x] = &pop_base[x * HWLDY];
        TrfDensity[x] = &trf_base[x * HWLDY];
        PollutionMem[x] = &pol_base[x * HWLDY];
        LandValueMem[x] = &land_base[x * HWLDY];
        CrimeMem[x] = &crime_base[x * HWLDY];
        tem[x] = &tem1_base[x * HWLDY];
        tem2[x] = &tem2_base[x * HWLDY];
    }

    terrain_base = (Byte *)calloc((size_t)(QWX * QWY), sizeof(Byte));
    qtem_base = (Byte *)calloc((size_t)(QWX * QWY), sizeof(Byte));
    for (int32_t x = 0; x < QWX; x++) {
        TerrainMem[x] = &terrain_base[x * QWY];
        Qtem[x] = &qtem_base[x * QWY];
    }

    ResHis = (int16_t *)calloc(HISTLEN, sizeof(int16_t));
    ComHis = (int16_t *)calloc(HISTLEN, sizeof(int16_t));
    IndHis = (int16_t *)calloc(HISTLEN, sizeof(int16_t));
    MoneyHis = (int16_t *)calloc(HISTLEN, sizeof(int16_t));
    PollutionHis = (int16_t *)calloc(HISTLEN, sizeof(int16_t));
    CrimeHis = (int16_t *)calloc(HISTLEN, sizeof(int16_t));
    MiscHis = (int16_t *)calloc(MISCHISTLEN, sizeof(int16_t));

    PowerMap = (uint16_t *)calloc(PWRMAPSIZE, sizeof(uint16_t));
}

void FreeMapBuffers(void)
{
    if (map_base) { free(map_base); map_base = NULL; }
    if (pop_base) { free(pop_base); pop_base = NULL; }
    if (trf_base) { free(trf_base); trf_base = NULL; }
    if (pol_base) { free(pol_base); pol_base = NULL; }
    if (land_base) { free(land_base); land_base = NULL; }
    if (crime_base) { free(crime_base); crime_base = NULL; }
    if (tem1_base) { free(tem1_base); tem1_base = NULL; }
    if (tem2_base) { free(tem2_base); tem2_base = NULL; }
    if (terrain_base) { free(terrain_base); terrain_base = NULL; }
    if (qtem_base) { free(qtem_base); qtem_base = NULL; }

    if (ResHis) { free(ResHis); ResHis = NULL; }
    if (ComHis) { free(ComHis); ComHis = NULL; }
    if (IndHis) { free(IndHis); IndHis = NULL; }
    if (MoneyHis) { free(MoneyHis); MoneyHis = NULL; }
    if (PollutionHis) { free(PollutionHis); PollutionHis = NULL; }
    if (CrimeHis) { free(CrimeHis); CrimeHis = NULL; }
    if (MiscHis) { free(MiscHis); MiscHis = NULL; }

    if (PowerMap) { free(PowerMap); PowerMap = NULL; }
    if (CityFileName) { free(CityFileName); CityFileName = NULL; }
}
