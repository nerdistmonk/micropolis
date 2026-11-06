/* s_fileio.c: City map persistence, serialization, and scenario file loading.
 *
 * Modernized C11 implementation ported from Micropolis.
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "sim.h"
#include "scenario.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern void Audio_MakeSound(const char *channel, const char *sound_name);
extern void ClearMes(void);

/* MiscHis[64] holds this tag when NoDisasters was on at save time. Any other value means off. */
#define NODISASTERS_SAVE_TAG 0x4E44

/* Fixed on-disk size of a city image (header streams plus 120x100 map). */
#define CITY_FILE_BYTES 27120

static float ClampPercent(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

/* The .cty image is big-endian throughout, so these helpers decode and encode explicitly on any host. */
static void DecodeShortStream(int16_t *dst, const uint8_t *src, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        dst[i] = (int16_t)(uint16_t)(((uint16_t)src[2 * i] << 8) | src[2 * i + 1]);
    }
}

static void EncodeShortStream(uint8_t *dst, const int16_t *src, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        uint16_t v = (uint16_t)src[i];
        dst[2 * i] = (uint8_t)(v >> 8);
        dst[2 * i + 1] = (uint8_t)(v & 0xFFu);
    }
}

static uint16_t DecodeU16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void EncodeU16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

/* Copies scalar game state into MiscHis so it is included in the next encode. */
static void SyncMiscHis(void)
{
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
}

static void ParseCityBlob(const uint8_t *blob)
{
    const size_t hist_count = HISTLEN / 2;
    const size_t hist_bytes = hist_count * sizeof(int16_t);
    const size_t misc_count = MISCHISTLEN / 2;
    const size_t misc_bytes = misc_count * sizeof(int16_t);

    DecodeShortStream(ResHis, blob + 0 * hist_bytes, hist_count);
    DecodeShortStream(ComHis, blob + 1 * hist_bytes, hist_count);
    DecodeShortStream(IndHis, blob + 2 * hist_bytes, hist_count);
    DecodeShortStream(CrimeHis, blob + 3 * hist_bytes, hist_count);
    DecodeShortStream(PollutionHis, blob + 4 * hist_bytes, hist_count);
    DecodeShortStream(MoneyHis, blob + 5 * hist_bytes, hist_count);
    DecodeShortStream(MiscHis, blob + 6 * hist_bytes, misc_count);

    /* Column-by-column layout: 120 columns of 100 tiles each */
    const uint8_t *map_src = blob + 6 * hist_bytes + misc_bytes;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            Map[x][y] = DecodeU16(map_src + ((size_t)x * WORLD_Y + (size_t)y) * sizeof(uint16_t));
        }
    }
}

static void EncodeCityBlob(uint8_t *blob)
{
    SyncMiscHis();

    const size_t hist_count = HISTLEN / 2;
    const size_t hist_bytes = hist_count * sizeof(int16_t);
    const size_t misc_count = MISCHISTLEN / 2;
    const size_t misc_bytes = misc_count * sizeof(int16_t);

    EncodeShortStream(blob + 0 * hist_bytes, ResHis, hist_count);
    EncodeShortStream(blob + 1 * hist_bytes, ComHis, hist_count);
    EncodeShortStream(blob + 2 * hist_bytes, IndHis, hist_count);
    EncodeShortStream(blob + 3 * hist_bytes, CrimeHis, hist_count);
    EncodeShortStream(blob + 4 * hist_bytes, PollutionHis, hist_count);
    EncodeShortStream(blob + 5 * hist_bytes, MoneyHis, hist_count);
    EncodeShortStream(blob + 6 * hist_bytes, MiscHis, misc_count);

    uint8_t *map_dst = blob + 6 * hist_bytes + misc_bytes;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            EncodeU16(map_dst + ((size_t)x * WORLD_Y + (size_t)y) * sizeof(uint16_t), Map[x][y]);
        }
    }
}

/* Writes to <path>.tmp, syncs, then renames over <path>, so a failed write never clobbers the existing file. */
static int32_t WriteFileAtomic(const char *path, const uint8_t *data, size_t len)
{
    size_t path_len = strlen(path);
    char *tmp = (char *)malloc(path_len + 5);
    if (!tmp) return 0;
    memcpy(tmp, path, path_len);
    memcpy(tmp + path_len, ".tmp", 5);

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        free(tmp);
        return 0;
    }

    int32_t ok = (fwrite(data, 1, len, f) == len) && (fflush(f) == 0) && (fsync(fileno(f)) == 0);
    if (fclose(f) != 0) ok = 0;

    if (ok && rename(tmp, path) == 0) {
        free(tmp);
        return 1;
    }
    remove(tmp);
    free(tmp);
    return 0;
}

static char data_root[512];

void SetDataRoot(const char *root)
{
    snprintf(data_root, sizeof(data_root), "%s", root ? root : "");
}

static int32_t LoadFileInternal(const char *filename)
{
    FILE *f = NULL;
    char path[1024];

    const char *prefixes[] = {
        "",
        "scenarios/",
        "../scenarios/",
        "../../scenarios/",
        "cities/",
        "../cities/"
    };

    /* Data root (the executable's directory) comes first so loading works from any cwd. */
    if (data_root[0]) {
        snprintf(path, sizeof(path), "%sscenarios/%s", data_root, filename);
        f = fopen(path, "rb");
        if (!f) {
            snprintf(path, sizeof(path), "%scities/%s", data_root, filename);
            f = fopen(path, "rb");
        }
    }

    for (size_t i = 0; !f && i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], filename);
        f = fopen(path, "rb");
    }

    if (!f) return 0;

    fseek(f, 0L, SEEK_END);
    long size = ftell(f);
    fseek(f, 0L, SEEK_SET);

    if (size != 27120 && size != 99120 && size != 219120) {
        fclose(f);
        return 0;
    }

    /* Read the whole image before touching any globals, so a short read leaves the current city intact. */
    uint8_t *blob = (uint8_t *)malloc(CITY_FILE_BYTES);
    if (!blob) {
        fclose(f);
        return 0;
    }
    size_t got = fread(blob, 1, CITY_FILE_BYTES, f);
    fclose(f);
    if (got != CITY_FILE_BYTES) {
        free(blob);
        return 0;
    }

    ParseCityBlob(blob);
    free(blob);
    return 1;
}

static void RestoreSimState(void)
{
    EMarket = (float)MiscHis[1];
    if (!EMarket) EMarket = 4.0f;
    ResPop = MiscHis[2];
    ComPop = MiscHis[3];
    IndPop = MiscHis[4];
    TotalPop = (int16_t)((ResPop / 8) + ComPop + IndPop);
    RValve = MiscHis[5];
    CValve = MiscHis[6];
    IValve = MiscHis[7];
    CrimeRamp = MiscHis[10];
    PolluteRamp = MiscHis[11];
    LVAverage = MiscHis[12];
    CrimeAverage = MiscHis[13];
    PolluteAverage = MiscHis[14];

    CityClass = MiscHis[16];
    if (CityClass > 5 || CityClass < 0) CityClass = 0;
    CityScore = MiscHis[17];
    if (CityScore > 999 || CityScore < 1) CityScore = 500;

    ResCap = 0;
    ComCap = 0;
    IndCap = 0;
    AvCityTax = (CityTime % 48) * 7;
}

/* Recounts infrastructure and reruns the derived scans. The loaders set RoadEffect, PoliceEffect, and FireEffect themselves. */
void CityRescan(void)
{
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

    DoPowerScan();
    DoNilPower();
    PTLScan();
    CrimeScan();
    PopDenScan();
    FireAnalysis();
}

int32_t LoadCity(const char *filename)
{
    if (!filename) return 0;

    if (!LoadFileInternal(filename)) {
        return 0;
    }

    Scenario_EditorEnd();

    uint16_t f0 = (uint16_t)MiscHis[50];
    uint16_t f1 = (uint16_t)MiscHis[51];
    TotalFunds = (int32_t)(((uint32_t)f0 << 16) | f1);

    uint16_t t0 = (uint16_t)MiscHis[8];
    uint16_t t1 = (uint16_t)MiscHis[9];
    CityTime = (int32_t)(((uint32_t)t0 << 16) | t1);

    autoBulldoze = MiscHis[52];
    autoBudget = MiscHis[53];
    autoGo = MiscHis[54];
    UserSoundOn = MiscHis[55];
    CityTax = MiscHis[56];
    SimSpeed = MiscHis[57];

    uint16_t p0 = (uint16_t)MiscHis[58];
    uint16_t p1 = (uint16_t)MiscHis[59];
    policePercent = ClampPercent((float)((int32_t)(((uint32_t)p0 << 16) | p1)) / 65536.0f);

    uint16_t fi0 = (uint16_t)MiscHis[60];
    uint16_t fi1 = (uint16_t)MiscHis[61];
    firePercent = ClampPercent((float)((int32_t)(((uint32_t)fi0 << 16) | fi1)) / 65536.0f);

    uint16_t r0 = (uint16_t)MiscHis[62];
    uint16_t r1 = (uint16_t)MiscHis[63];
    roadPercent = ClampPercent((float)((int32_t)(((uint32_t)r0 << 16) | r1)) / 65536.0f);

    if (CityTime < 0) CityTime = 0;
    if (CityTax > 20 || CityTax < 0) CityTax = 7;
    if (SimSpeed < 0 || SimSpeed > SIM_SPEED_MAX) SimSpeed = SIM_SPEED_FAST;
    NoDisasters = (MiscHis[64] == NODISASTERS_SAVE_TAG) ? 1 : 0;

    int16_t level = MiscHis[15];
    if (level < 0 || level > 2) level = 0;
    SetGameLevel(level);

    ResetCityWorkingState();
    RestoreSimState();

    ScenarioID = 0;
    DisasterEvent = 0;
    ScoreType = 0;
    InitSimLoad = 1;
    DoInitialEval = 0;

    InitSprites();

    RoadEffect = 32;
    PoliceEffect = 1000;
    FireEffect = 1000;
    CityRescan();
    CityEvaluation();

    if (CityFileName) free(CityFileName);
    CityFileName = strdup(filename);

    if (EngineCallbacks.update_funds) EngineCallbacks.update_funds(TotalFunds);
    if (EngineCallbacks.invalidate_maps) EngineCallbacks.invalidate_maps();
    if (EngineCallbacks.invalidate_editors) EngineCallbacks.invalidate_editors();

    return 1;
}

int32_t SaveCity(const char *filename)
{
    if (!filename) {
        if (!CityFileName) return 0;
        filename = CityFileName;
    }

    uint8_t *blob = (uint8_t *)malloc(CITY_FILE_BYTES);
    if (!blob) return 0;
    EncodeCityBlob(blob);
    int32_t ok = WriteFileAtomic(filename, blob, CITY_FILE_BYTES);
    free(blob);
    if (!ok) return 0;

    if (CityFileName && CityFileName != filename) {
        free(CityFileName);
        CityFileName = strdup(filename);
    } else if (!CityFileName) {
        CityFileName = strdup(filename);
    }

    return 1;
}

/* .scn layout: a 376-byte little-endian header, then the 27120-byte city image used by .cty files. */
#define SCN_HEADER_BYTES   376
#define SCN_TOTAL_BYTES    (SCN_HEADER_BYTES + CITY_FILE_BYTES)
#define SCN_VERSION        1
#define SCN_OFF_VERSION    4
#define SCN_OFF_FLAGS      6
#define SCN_OFF_NAME       8
#define SCN_OFF_DESC       40
#define SCN_OFF_START      168
#define SCN_OFF_FUNDS      172
#define SCN_OFF_DEADLINE   176
#define SCN_OFF_WIN_COUNT  180
#define SCN_OFF_LOSE_COUNT 181
#define SCN_OFF_WIN        182
#define SCN_OFF_LOSE       246
#define SCN_OFF_EVENT_COUNT 310
#define SCN_OFF_EVENTS     312
#define SCN_COND_BYTES     8
#define SCN_EVENT_BYTES    8
#define SCN_FLAG_DISASTERS 0x01
#define SCN_FLAG_WIN_ALL   0x02
#define SCN_FLAG_LOSE_ALL  0x04
#define SCN_FLAG_KNOWN     0x07

_Static_assert(SCN_OFF_DESC == SCN_OFF_NAME + SCN_NAME_BYTES, "scn name field layout");
_Static_assert(SCN_OFF_START == SCN_OFF_DESC + SCN_DESC_BYTES, "scn description field layout");
_Static_assert(SCN_OFF_FUNDS == SCN_OFF_START + 4, "scn start field layout");
_Static_assert(SCN_OFF_DEADLINE == SCN_OFF_FUNDS + 4, "scn funds field layout");
_Static_assert(SCN_OFF_WIN_COUNT == SCN_OFF_DEADLINE + 4, "scn deadline field layout");
_Static_assert(SCN_OFF_LOSE_COUNT == SCN_OFF_WIN_COUNT + 1, "scn win count layout");
_Static_assert(SCN_OFF_WIN == SCN_OFF_LOSE_COUNT + 1, "scn lose count layout");
_Static_assert(SCN_OFF_LOSE == SCN_OFF_WIN + SCN_MAX_CONDITIONS * SCN_COND_BYTES, "scn win list layout");
_Static_assert(SCN_OFF_EVENT_COUNT == SCN_OFF_LOSE + SCN_MAX_CONDITIONS * SCN_COND_BYTES, "scn lose list layout");
_Static_assert(SCN_OFF_EVENTS == SCN_OFF_EVENT_COUNT + 2, "scn event count layout");
_Static_assert(SCN_HEADER_BYTES == SCN_OFF_EVENTS + SCN_MAX_EVENTS * SCN_EVENT_BYTES, "scn header size");

static void PutU16LE(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void PutI32LE(uint8_t *p, int32_t v)
{
    uint32_t u = (uint32_t)v;
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(u >> (8 * i));
}

static uint16_t GetU16LE(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int32_t GetI32LE(const uint8_t *p)
{
    uint32_t u = 0;
    for (int i = 0; i < 4; i++) u |= (uint32_t)p[i] << (8 * i);
    return (int32_t)u;
}

static void PutCondition(uint8_t *p, const ScenarioCondition *c)
{
    p[0] = c->metric;
    p[1] = c->op;
    p[2] = 0;
    p[3] = 0;
    PutI32LE(p + 4, c->value);
}

static void GetCondition(const uint8_t *p, ScenarioCondition *c)
{
    c->metric = p[0];
    c->op = p[1];
    c->value = GetI32LE(p + 4);
}

/* Byte 2-3 are reserved padding and are written as zero. */
static void PutEvent(uint8_t *p, const ScenarioEvent *e)
{
    p[0] = e->type;
    p[1] = e->month;
    p[2] = 0;
    p[3] = 0;
    PutI32LE(p + 4, e->year);
}

static void GetEvent(const uint8_t *p, ScenarioEvent *e)
{
    e->type = p[0];
    e->month = p[1];
    e->year = GetI32LE(p + 4);
}

static void EncodeScenarioHeader(const ScenarioDef *def, uint8_t *h)
{
    memset(h, 0, SCN_HEADER_BYTES);
    memcpy(h, "MCSN", 4);
    PutU16LE(h + SCN_OFF_VERSION, SCN_VERSION);

    uint16_t flags = 0;
    if (def->disasters) flags |= SCN_FLAG_DISASTERS;
    if (def->win_all) flags |= SCN_FLAG_WIN_ALL;
    if (def->lose_all) flags |= SCN_FLAG_LOSE_ALL;
    PutU16LE(h + SCN_OFF_FLAGS, flags);

    /* Validation runs before encoding, so both strings are NUL-terminated. */
    memcpy(h + SCN_OFF_NAME, def->name, strnlen(def->name, SCN_NAME_BYTES));
    memcpy(h + SCN_OFF_DESC, def->description, strnlen(def->description, SCN_DESC_BYTES));

    PutI32LE(h + SCN_OFF_START, def->start_year);
    PutI32LE(h + SCN_OFF_FUNDS, def->funds);
    PutI32LE(h + SCN_OFF_DEADLINE, def->deadline_year);
    h[SCN_OFF_WIN_COUNT] = def->win_count;
    h[SCN_OFF_LOSE_COUNT] = def->lose_count;

    for (int i = 0; i < SCN_MAX_CONDITIONS; i++) {
        PutCondition(h + SCN_OFF_WIN + i * SCN_COND_BYTES, &def->win[i]);
        PutCondition(h + SCN_OFF_LOSE + i * SCN_COND_BYTES, &def->lose[i]);
    }

    h[SCN_OFF_EVENT_COUNT] = def->event_count;
    for (int i = 0; i < SCN_MAX_EVENTS; i++) {
        PutEvent(h + SCN_OFF_EVENTS + i * SCN_EVENT_BYTES, &def->events[i]);
    }
}

/* Returns 0 for anything that is not a version-1 .scn header. Text and conditions are copied raw; Scenario_Validate checks them. */
static int32_t DecodeScenarioHeader(const uint8_t *h, ScenarioDef *def)
{
    if (memcmp(h, "MCSN", 4) != 0) return 0;
    if (GetU16LE(h + SCN_OFF_VERSION) != SCN_VERSION) return 0;

    uint16_t flags = GetU16LE(h + SCN_OFF_FLAGS);
    if (flags & (uint16_t)~SCN_FLAG_KNOWN) return 0;

    memset(def, 0, sizeof(*def));
    memcpy(def->name, h + SCN_OFF_NAME, SCN_NAME_BYTES);
    memcpy(def->description, h + SCN_OFF_DESC, SCN_DESC_BYTES);
    def->start_year = GetI32LE(h + SCN_OFF_START);
    def->funds = GetI32LE(h + SCN_OFF_FUNDS);
    def->deadline_year = GetI32LE(h + SCN_OFF_DEADLINE);
    def->disasters = (flags & SCN_FLAG_DISASTERS) ? 1 : 0;
    def->win_all = (flags & SCN_FLAG_WIN_ALL) ? 1 : 0;
    def->lose_all = (flags & SCN_FLAG_LOSE_ALL) ? 1 : 0;
    def->win_count = h[SCN_OFF_WIN_COUNT];
    def->lose_count = h[SCN_OFF_LOSE_COUNT];

    for (int i = 0; i < SCN_MAX_CONDITIONS; i++) {
        GetCondition(h + SCN_OFF_WIN + i * SCN_COND_BYTES, &def->win[i]);
        GetCondition(h + SCN_OFF_LOSE + i * SCN_COND_BYTES, &def->lose[i]);
    }

    def->event_count = h[SCN_OFF_EVENT_COUNT];
    for (int i = 0; i < SCN_MAX_EVENTS; i++) {
        GetEvent(h + SCN_OFF_EVENTS + i * SCN_EVENT_BYTES, &def->events[i]);
    }
    return 1;
}

static int32_t ReadScenarioFile(const char *path, uint8_t *buf)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    int32_t ok = fseek(f, 0L, SEEK_END) == 0 &&
                 ftell(f) == (long)SCN_TOTAL_BYTES &&
                 fseek(f, 0L, SEEK_SET) == 0 &&
                 fread(buf, 1, SCN_TOTAL_BYTES, f) == SCN_TOTAL_BYTES;
    fclose(f);
    return ok;
}

int32_t Scenario_ReadDef(const char *path, ScenarioDef *def)
{
    uint8_t header[SCN_HEADER_BYTES];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    int32_t ok = fseek(f, 0L, SEEK_END) == 0 &&
                 ftell(f) == (long)SCN_TOTAL_BYTES &&
                 fseek(f, 0L, SEEK_SET) == 0 &&
                 fread(header, 1, SCN_HEADER_BYTES, f) == SCN_HEADER_BYTES;
    fclose(f);

    return ok && DecodeScenarioHeader(header, def) && Scenario_Validate(def) == NULL;
}

int32_t Scenario_LoadFile(const char *path)
{
    uint8_t *blob = (uint8_t *)malloc(SCN_TOTAL_BYTES);
    if (!blob) return 0;

    /* Everything is checked before any global is touched, so a bad file leaves the current city intact. */
    ScenarioDef def;
    int32_t ok = ReadScenarioFile(path, blob) &&
                 DecodeScenarioHeader(blob, &def) &&
                 Scenario_Validate(&def) == NULL;
    if (!ok) {
        free(blob);
        return 0;
    }

    Scenario_EditorEnd();
    ParseCityBlob(blob + SCN_HEADER_BYTES);
    free(blob);

    SetGameLevel(0);
    ResetCityWorkingState();
    CityTime = Scenario_StartTicks(def.start_year);
    TotalFunds = def.funds;
    NoDisasters = def.disasters ? 0 : 1;
    RestoreSimState();

    ScenarioID = 0;
    DisasterEvent = 0;
    ScoreType = 0;
    ScoreWait = 0;

    snprintf(CityName, sizeof(CityName), "%s", def.name);
    free(CityFileName);
    CityFileName = NULL;

    SimSpeed = SIM_SPEED_FAST;
    CityTax = 7;
    roadPercent = 1.0f;
    policePercent = 1.0f;
    firePercent = 1.0f;

    InitSimLoad = 1;
    DoInitialEval = 0;
    InitSprites();

    RoadEffect = 32;
    PoliceEffect = 1000;
    FireEffect = 1000;
    CityRescan();
    CityEvaluation();

    Scenario_Activate(&def);

    if (EngineCallbacks.update_funds) EngineCallbacks.update_funds(TotalFunds);
    if (EngineCallbacks.invalidate_maps) EngineCallbacks.invalidate_maps();
    if (EngineCallbacks.invalidate_editors) EngineCallbacks.invalidate_editors();

    return 1;
}

void Scenario_GetDirectory(char *buf, size_t len)
{
    snprintf(buf, len, "%sscenarios/", data_root);
}

int32_t Scenario_SaveToDirectory(const ScenarioDef *def, char *saved_path, size_t saved_len)
{
    if (Scenario_Validate(def) != NULL) return 0;

    char dir[1024];
    Scenario_GetDirectory(dir, sizeof(dir));
    if (mkdir(dir, 0777) != 0 && errno != EEXIST) return 0;

    /* Keep the file name to letters, digits, '-' and '_' so names cannot escape the directory. */
    char stem[SCN_NAME_BYTES];
    size_t n = strnlen(def->name, SCN_NAME_BYTES);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)def->name[i];
        stem[i] = (isalnum(c) || c == '-' || c == '_') ? (char)c : '_';
    }
    stem[n] = '\0';

    char path[1100];
    if (snprintf(path, sizeof(path), "%s%s.scn", dir, stem) >= (int)sizeof(path)) return 0;

    uint8_t *blob = (uint8_t *)malloc(SCN_TOTAL_BYTES);
    if (!blob) return 0;
    EncodeScenarioHeader(def, blob);
    EncodeCityBlob(blob + SCN_HEADER_BYTES);
    int32_t ok = WriteFileAtomic(path, blob, SCN_TOTAL_BYTES);
    free(blob);
    if (!ok) return 0;

    if (saved_path && saved_len > 0) snprintf(saved_path, saved_len, "%s", path);
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
            dis_event = 1;
            dis_wait = 2;
            score_wait = 48 * 30;
            break;
        case 2:
            name = "San Francisco";
            fname = "snro.222";
            start_year = 1906;
            dis_event = 2;
            dis_wait = 10;
            score_wait = 48 * 5;
            break;
        case 3:
            name = "Hamburg";
            fname = "snro.333";
            start_year = 1944;
            dis_event = 3;
            dis_wait = 5;
            score_wait = 48 * 5;
            break;
        case 4:
            name = "Bern";
            fname = "snro.444";
            start_year = 1965;
            dis_event = 4;
            dis_wait = 20;
            score_wait = 48 * 10;
            break;
        case 5:
            name = "Tokyo";
            fname = "snro.555";
            start_year = 1957;
            dis_event = 5;
            dis_wait = 3;
            score_wait = 48 * 5;
            break;
        case 6:
            name = "Detroit";
            fname = "snro.666";
            start_year = 1972;
            dis_event = 6;
            dis_wait = 5;
            score_wait = 48 * 10;
            break;
        case 7:
            name = "Boston";
            fname = "snro.777";
            start_year = 2010;
            dis_event = 7;
            dis_wait = 5;
            score_wait = 48 * 5;
            break;
        case 8:
            name = "Rio de Janeiro";
            fname = "snro.888";
            start_year = 2047;
            dis_event = 8;
            dis_wait = 96;
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

    Scenario_EditorEnd();

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
    ResetCityWorkingState();
    RestoreSimState();

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

    RoadEffect = 32;
    PoliceEffect = 1000;
    FireEffect = 1000;
    CityRescan();
    CityEvaluation();

    if (EngineCallbacks.update_funds) EngineCallbacks.update_funds(TotalFunds);
    if (EngineCallbacks.invalidate_maps) EngineCallbacks.invalidate_maps();
    if (EngineCallbacks.invalidate_editors) EngineCallbacks.invalidate_editors();

    return 1;
}