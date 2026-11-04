/* s_fileio.c: City map persistence, serialization, and scenario file loading.
 *
 * Modernized C11 implementation ported from Micropolis.
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Audio_MakeSound(const char *channel, const char *sound_name);
extern void ClearMes(void);

/* MiscHis[64] holds this tag when NoDisasters was on at save time. Any other value means off. */
#define NODISASTERS_SAVE_TAG 0x4E44

static void SwapShortBuffer(int16_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint16_t v = (uint16_t)buf[i];
        buf[i] = (int16_t)(((v & 0x00FFu) << 8) | ((v & 0xFF00u) >> 8));
    }
}

static void SwapMapBuffer(uint16_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint16_t v = buf[i];
        buf[i] = (uint16_t)(((v & 0x00FFu) << 8) | ((v & 0xFF00u) >> 8));
    }
}

static int32_t LoadShortStream(int16_t *buf, size_t len, FILE *f)
{
    if (fread(buf, sizeof(int16_t), len, f) != len) {
        return 0;
    }
    SwapShortBuffer(buf, len);
    return 1;
}

static int32_t SaveShortStream(const int16_t *buf, size_t len, FILE *f)
{
    int16_t *tmp = (int16_t *)malloc(len * sizeof(int16_t));
    if (!tmp) return 0;

    memcpy(tmp, buf, len * sizeof(int16_t));
    SwapShortBuffer(tmp, len);

    size_t written = fwrite(tmp, sizeof(int16_t), len, f);
    free(tmp);

    return (written == len) ? 1 : 0;
}

static int32_t LoadFileInternal(const char *filename)
{
    FILE *f = NULL;
    char path[512];

    const char *prefixes[] = {
        "",
        "scenarios/",
        "../scenarios/",
        "../../scenarios/",
        "cities/",
        "../cities/",
        "res/",
        "../res/"
    };

    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], filename);
        f = fopen(path, "rb");
        if (f) break;
    }

    if (!f) return 0;

    fseek(f, 0L, SEEK_END);
    long size = ftell(f);
    fseek(f, 0L, SEEK_SET);

    if (size != 27120 && size != 99120 && size != 219120) {
        fclose(f);
        return 0;
    }

    if (!LoadShortStream(ResHis, HISTLEN / 2, f) ||
        !LoadShortStream(ComHis, HISTLEN / 2, f) ||
        !LoadShortStream(IndHis, HISTLEN / 2, f) ||
        !LoadShortStream(CrimeHis, HISTLEN / 2, f) ||
        !LoadShortStream(PollutionHis, HISTLEN / 2, f) ||
        !LoadShortStream(MoneyHis, HISTLEN / 2, f) ||
        !LoadShortStream(MiscHis, MISCHISTLEN / 2, f)) {
        fclose(f);
        return 0;
    }

    /* Column-by-column read: 120 columns of 100 tiles each */
    uint16_t col[WORLD_Y];
    for (int32_t x = 0; x < WORLD_X; x++) {
        if (fread(col, sizeof(uint16_t), WORLD_Y, f) != WORLD_Y) {
            fclose(f);
            return 0;
        }
        SwapMapBuffer(col, WORLD_Y);
        for (int32_t y = 0; y < WORLD_Y; y++) {
            Map[x][y] = col[y];
        }
    }

    fclose(f);
    return 1;
}

int32_t LoadCity(const char *filename)
{
    if (!filename) return 0;

    if (!LoadFileInternal(filename)) {
        return 0;
    }

    uint16_t f0 = (uint16_t)MiscHis[50];
    uint16_t f1 = (uint16_t)MiscHis[51];
    TotalFunds = (int32_t)((f0 << 16) | f1);

    uint16_t t0 = (uint16_t)MiscHis[8];
    uint16_t t1 = (uint16_t)MiscHis[9];
    CityTime = (int32_t)((t0 << 16) | t1);

    autoBulldoze = MiscHis[52];
    autoBudget = MiscHis[53];
    autoGo = MiscHis[54];
    UserSoundOn = MiscHis[55];
    CityTax = MiscHis[56];
    SimSpeed = MiscHis[57];

    uint16_t p0 = (uint16_t)MiscHis[58];
    uint16_t p1 = (uint16_t)MiscHis[59];
    policePercent = (float)((int32_t)((p0 << 16) | p1)) / 65536.0f;

    uint16_t fi0 = (uint16_t)MiscHis[60];
    uint16_t fi1 = (uint16_t)MiscHis[61];
    firePercent = (float)((int32_t)((fi0 << 16) | fi1)) / 65536.0f;

    uint16_t r0 = (uint16_t)MiscHis[62];
    uint16_t r1 = (uint16_t)MiscHis[63];
    roadPercent = (float)((int32_t)((r0 << 16) | r1)) / 65536.0f;

    if (CityTime < 0) CityTime = 0;
    if (CityTax > 20 || CityTax < 0) CityTax = 7;
    if (SimSpeed < 0 || SimSpeed > 3) SimSpeed = 3;
    if (roadPercent <= 0.0f || roadPercent > 1.0f) roadPercent = 1.0f;
    if (policePercent <= 0.0f || policePercent > 1.0f) policePercent = 1.0f;
    if (firePercent <= 0.0f || firePercent > 1.0f) firePercent = 1.0f;

    NoDisasters = (MiscHis[64] == NODISASTERS_SAVE_TAG) ? 1 : 0;

    int16_t level = MiscHis[15];
    if (level < 0 || level > 2) level = 0;
    SetGameLevel(level);

    ScenarioID = 0;
    DisasterEvent = 0;
    DisasterWait = 0;
    ScoreType = 0;
    ScoreWait = 0;
    InitSimLoad = 1;
    DoInitialEval = 0;

    InitSprites();

    /* Pre-scan the loaded city so infrastructure counts are accurate */
    RoadTotal = 0;
    RailTotal = 0;
    CoalPop = 0;
    NuclearPop = 0;
    PolicePop = 0;
    FireStPop = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y];
            uint16_t low = tile & LOMASK;
            if (low >= ROADBASE && low < POWERBASE) RoadTotal++;
            else if (low >= RAILBASE && low < RESBASE) RailTotal++;
            else if (low == POWERPLANT) CoalPop++;
            else if (low == NUCLEAR) NuclearPop++;
            else if (low == POLICESTATION) PolicePop++;
            else if (low == FIRESTATION) FireStPop++;
        }
    }

    RoadFund = CalcRoadFund(GameLevel);
    RoadSpend = (int32_t)((float)RoadFund * roadPercent);
    PoliceFund = PolicePop * 100;
    PoliceSpend = (int32_t)((float)PoliceFund * policePercent);
    FireFund = FireStPop * 100;
    FireSpend = (int32_t)((float)FireFund * firePercent);

    RoadEffect = (int16_t)(roadPercent * 32.0f);
    PoliceEffect = (int16_t)(policePercent * 1000.0f);
    FireEffect = (int16_t)(firePercent * 1000.0f);

    DoPowerScan();
    DoNilPower();
    PTLScan();
    CrimeScan();
    PopDenScan();
    FireAnalysis();

    if (CityFileName) free(CityFileName);
    CityFileName = strdup(filename);

    if (EngineCallbacks.update_funds) EngineCallbacks.update_funds(TotalFunds);
    if (EngineCallbacks.invalidate_maps) EngineCallbacks.invalidate_maps();
    if (EngineCallbacks.invalidate_editors) EngineCallbacks.invalidate_editors();

    Audio_MakeSound("city", "build");
    return 1;
}

int32_t SaveCity(const char *filename)
{
    FILE *f;

    if (!filename) {
        if (!CityFileName) return 0;
        filename = CityFileName;
    }

    f = fopen(filename, "wb");
    if (!f) return 0;

    uint32_t tf = (uint32_t)TotalFunds;
    MiscHis[50] = (int16_t)((tf >> 16) & 0xFFFFu);
    MiscHis[51] = (int16_t)(tf & 0xFFFFu);

    uint32_t ct = (uint32_t)CityTime;
    MiscHis[8] = (int16_t)((ct >> 16) & 0xFFFFu);
    MiscHis[9] = (int16_t)(ct & 0xFFFFu);

    MiscHis[52] = autoBulldoze;
    MiscHis[53] = autoBudget;
    MiscHis[54] = autoGo;
    MiscHis[55] = UserSoundOn;
    MiscHis[56] = CityTax;
    MiscHis[57] = SimSpeed;

    uint32_t pol = (uint32_t)(int32_t)(policePercent * 65536.0f);
    MiscHis[58] = (int16_t)((pol >> 16) & 0xFFFFu);
    MiscHis[59] = (int16_t)(pol & 0xFFFFu);

    uint32_t fir = (uint32_t)(int32_t)(firePercent * 65536.0f);
    MiscHis[60] = (int16_t)((fir >> 16) & 0xFFFFu);
    MiscHis[61] = (int16_t)(fir & 0xFFFFu);

    uint32_t rd = (uint32_t)(int32_t)(roadPercent * 65536.0f);
    MiscHis[62] = (int16_t)((rd >> 16) & 0xFFFFu);
    MiscHis[63] = (int16_t)(rd & 0xFFFFu);

    MiscHis[15] = GameLevel;
    MiscHis[64] = NoDisasters ? NODISASTERS_SAVE_TAG : 0;

    if (!SaveShortStream(ResHis, HISTLEN / 2, f) ||
        !SaveShortStream(ComHis, HISTLEN / 2, f) ||
        !SaveShortStream(IndHis, HISTLEN / 2, f) ||
        !SaveShortStream(CrimeHis, HISTLEN / 2, f) ||
        !SaveShortStream(PollutionHis, HISTLEN / 2, f) ||
        !SaveShortStream(MoneyHis, HISTLEN / 2, f) ||
        !SaveShortStream(MiscHis, MISCHISTLEN / 2, f)) {
        fclose(f);
        return 0;
    }

    /* Column-by-column write to match LoadFileInternal */
    uint16_t col[WORLD_Y];
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            col[y] = Map[x][y];
        }
        SwapMapBuffer(col, WORLD_Y);
        if (fwrite(col, sizeof(uint16_t), WORLD_Y, f) != WORLD_Y) {
            fclose(f);
            return 0;
        }
    }

    fclose(f);

    if (CityFileName && CityFileName != filename) {
        free(CityFileName);
        CityFileName = strdup(filename);
    } else if (!CityFileName) {
        CityFileName = strdup(filename);
    }

    Audio_MakeSound("city", "save");
    return 1;
}

int32_t LoadScenario(int16_t s)
{
    const char *name;
    const char *fname;
    int32_t initial_funds = 20000;
    int32_t start_year = 1900;
    int16_t dis_event = 0;
    int16_t dis_wait = 0;
    int16_t score_wait = 0;

    if (s < 1 || s > 8) s = 1;

    switch (s) {
        case 1:
            name = "Dullsville";
            fname = "snro.111";
            start_year = 1900;
            initial_funds = 5000;
            dis_event = 0;
            dis_wait = 0;
            score_wait = 48 * 30;
            break;
        case 2:
            name = "San Francisco";
            fname = "snro.222";
            start_year = 1906;
            dis_event = 2;
            dis_wait = 40;
            score_wait = 48 * 10;
            break;
        case 3:
            name = "Hamburg";
            fname = "snro.333";
            start_year = 1944;
            dis_event = 3;
            dis_wait = 100;
            score_wait = 48 * 10;
            break;
        case 4:
            name = "Bern";
            fname = "snro.444";
            start_year = 1965;
            dis_event = 4;
            dis_wait = 0;
            score_wait = 48 * 10;
            break;
        case 5:
            name = "Tokyo";
            fname = "snro.555";
            start_year = 1957;
            dis_event = 5;
            dis_wait = 30;
            score_wait = 48 * 10;
            break;
        case 6:
            name = "Detroit";
            fname = "snro.666";
            start_year = 1972;
            dis_event = 6;
            dis_wait = 0;
            score_wait = 48 * 10;
            break;
        case 7:
            name = "Boston";
            fname = "snro.777";
            start_year = 2010;
            dis_event = 7;
            dis_wait = 50;
            score_wait = 48 * 10;
            break;
        case 8:
            name = "Rio de Janeiro";
            fname = "snro.888";
            start_year = 2047;
            dis_event = 8;
            dis_wait = 48;
            score_wait = 48 * 10;
            break;
        default:
            name = "City";
            fname = "snro.111";
            dis_event = 0;
            dis_wait = 0;
            score_wait = 48 * 10;
            break;
    }

    if (!LoadFileInternal(fname)) {
        return 0;
    }

    if (CityFileName != NULL) {
        free(CityFileName);
        CityFileName = NULL;
    }
    SetGameLevel(0);

    ScenarioID = s;
    DisasterEvent = dis_event;
    DisasterWait = dis_wait;
    ScoreType = s;
    ScoreWait = score_wait;
    CityTime = (int32_t)(((start_year - 1900) * 48) + 2);
    TotalFunds = initial_funds;

    strncpy(CityName, name, sizeof(CityName) - 1);
    CityName[sizeof(CityName) - 1] = '\0';

    SimSpeed = 3;
    CityTax = 7;
    roadPercent = 1.0f;
    policePercent = 1.0f;
    firePercent = 1.0f;

    InitSimLoad = 1;
    DoInitialEval = 0;

    InitSprites();

    /* Count infrastructure from the loaded scenario */
    RoadTotal = 0;
    RailTotal = 0;
    CoalPop = 0;
    NuclearPop = 0;
    PolicePop = 0;
    FireStPop = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y];
            uint16_t low = tile & LOMASK;
            if (low >= ROADBASE && low < POWERBASE) RoadTotal++;
            else if (low >= RAILBASE && low < RESBASE) RailTotal++;
            else if (low == POWERPLANT) CoalPop++;
            else if (low == NUCLEAR) NuclearPop++;
            else if (low == POLICESTATION) PolicePop++;
            else if (low == FIRESTATION) FireStPop++;
        }
    }

    RoadFund = CalcRoadFund(GameLevel);
    RoadSpend = (int32_t)((float)RoadFund * roadPercent);
    PoliceFund = PolicePop * 100;
    PoliceSpend = (int32_t)((float)PoliceFund * policePercent);
    FireFund = FireStPop * 100;
    FireSpend = (int32_t)((float)FireFund * firePercent);

    RoadEffect = (int16_t)(roadPercent * 32.0f);
    PoliceEffect = (int16_t)(policePercent * 1000.0f);
    FireEffect = (int16_t)(firePercent * 1000.0f);

    DoPowerScan();
    DoNilPower();
    PTLScan();
    CrimeScan();
    PopDenScan();
    FireAnalysis();

    if (EngineCallbacks.update_funds) EngineCallbacks.update_funds(TotalFunds);
    if (EngineCallbacks.invalidate_maps) EngineCallbacks.invalidate_maps();
    if (EngineCallbacks.invalidate_editors) EngineCallbacks.invalidate_editors();

    Audio_MakeSound("city", "build");
    return 1;
}