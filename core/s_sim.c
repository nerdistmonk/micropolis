/* s_sim.c: Core simulation loop, step dispatching, and cellular automata drivers.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"

#include <time.h>

/* Simulation State Variables */
int32_t ValveFlag = 0;
int16_t CrimeRamp = 0;
int16_t PolluteRamp = 0;
int16_t RValve = 0;
int16_t CValve = 0;
int16_t IValve = 0;
int16_t ResCap = 0;
int16_t ComCap = 0;
int16_t IndCap = 0;
int16_t CashFlow = 0;
float EMarket = 4.0f;
int16_t DisasterEvent = 0;
int16_t DisasterWait = 0;
int16_t ScoreType = 0;
int16_t ScoreWait = 0;
int16_t PwrdZCnt = 0;
int16_t unPwrdZCnt = 0;
int16_t UnwateredZCnt = 0;
int16_t NewPower = 0;
int16_t AvCityTax = 0;
int16_t Scycle = 0;
int16_t Fcycle = 0;
int16_t Spdcycle = 0;
int16_t DoInitialEval = 0;
int16_t MeltX = 0;
int16_t MeltY = 0;

int16_t SimSpeed = 3;
int16_t SimMetaSpeed = 0;
int16_t NoDisasters = 0;
int16_t autoBulldoze = 1;
int16_t autoBudget = 1;
int16_t autoGo = 1;
int16_t UserSoundOn = 1;
int16_t InitSimLoad = 0;
int16_t ScenarioID = 0;

int32_t MustUpdateFunds = 0;
int32_t MustUpdateOptions = 0;

/* Internal Function Prototypes */
static void SetValves(void);
void ClearCensus(void);
static void TakeCensus(void);
static void Take2Census(void);
static void DecROGMem(void);
static void SetCommonInits(void);
static void MapScan(int32_t x1, int32_t x2);
static void DoRail(void);
static void DoRadTile(void);
static void DoRoad(void);
static int32_t DoBridge(void);
static int32_t GetBoatDis(void);
static void DoFire(void);
static void DrawStadium(int32_t z);
static void DoAirport(void);
static void CoalSmoke(int32_t mx, int32_t my);
static void UpdateFundEffects(void);

/* PRNG state */
static uint32_t sim_next_rand = 1;

static uint16_t sim_rand_raw(void)
{
    sim_next_rand = sim_next_rand * 1103515245u + 12345u;
    return (uint16_t)((sim_next_rand % (0x10000u << 8)) >> 8);
}

void SeedRand(int32_t seed)
{
    sim_next_rand = (uint32_t)seed;
}

void RandomlySeedRand(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    SeedRand((int32_t)((uint32_t)ts.tv_nsec ^ (uint32_t)ts.tv_sec ^ sim_rand_raw()));
}

int16_t Rand(int16_t range)
{
    int32_t max_multiple, rnum;

    if (range <= 0) return 0;
    range++;
    max_multiple = 0xffff / range;
    max_multiple *= range;
    while ((rnum = (int32_t)sim_rand_raw()) >= max_multiple) {
        continue;
    }
    return (int16_t)(rnum % range);
}

int16_t RandInt(void)
{
    return (int16_t)sim_rand_raw();
}

int16_t Rand16(void)
{
    return (int16_t)sim_rand_raw();
}

int16_t Rand16Signed(void)
{
    int32_t i = (int32_t)sim_rand_raw();
    if (i > 32767) {
        i = 32767 - i;
    }
    return (int16_t)i;
}

static int16_t last_run_speed = 3;

void SetSimSpeed(int16_t speed)
{
    if (speed < 0) speed = 0;
    if (speed > SIM_SPEED_MAX) speed = SIM_SPEED_MAX;
    SimSpeed = speed;
    if (speed > 0) last_run_speed = speed;
}

int16_t GetLastRunSpeed(void)
{
    return last_run_speed;
}

void SetGameLevel(int16_t level)
{
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    GameLevel = level;
}

static const int32_t NewCityFunds[3] = { 20000, 10000, 5000 };

void SetNewCityLevel(int16_t level)
{
    SetGameLevel(level);
    TotalFunds = NewCityFunds[GameLevel];
}

/* Traffic road shapes: LTRFBASE and HTRFBASE each hold the 16 road shapes that DoRoad picks from. */
bool IsTrafficRoadTile(uint16_t id)
{
    return (id >= LTRFBASE && id < LTRFBASE + 16) || (id >= HTRFBASE && id < HTRFBASE + 16);
}

void AnimateTiles(void)
{
    if (!DoAnimation) return;

    if (++flagBlink > 30) {
        flagBlink = -30;
    }

    if (SimSpeed == 0) return;

    bool trafficStep = TrafAnimStepDue();

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y];
            if (tile & ANIMBIT) {
                uint16_t flags = tile & ALLBITS;
                uint16_t id = tile & LOMASK;

                if (!trafficStep && IsTrafficRoadTile(id)) continue;

                if (id < 1024) {
                    id = (uint16_t)aniTile[id];
                }

                Map[x][y] = (uint16_t)(id | flags);
            }
        }
    }
}

/* Ticks between Simulate steps per SimSpeed; the main loop calls SimFrame every 40 ms. */
static const int32_t SpdTickInterval[SIM_SPEED_MAX + 1] = { 1, 5, 3, 1, 1, 12 };

void SimFrame(void)
{
    if (SimSpeed == SIM_SPEED_PAUSED) {
        return;
    }

    if (++Spdcycle > 1023) {
        Spdcycle = 0;
    }

    if (Spdcycle % SpdTickInterval[SimSpeed]) {
        return;
    }

    int32_t steps = (SimSpeed == SIM_SPEED_SUPER_FAST) ? 4 : 1;
    for (int32_t i = 0; i < steps; i++) {
        if (++Fcycle > 1023) {
            Fcycle = 0;
        }
        Simulate(Fcycle & 15);
    }
}

/* CityTime ticks once per 16 steps and a calendar month is 4 ticks, so 64 consecutive steps advance exactly one month. */
void SimAdvanceOneMonth(void)
{
    for (int32_t i = 0; i < 64; i++) {
        if (++Fcycle > 1023) {
            Fcycle = 0;
        }
        Simulate(Fcycle & 15);
    }
}

void Simulate(int32_t mod16)
{
    static const int16_t SpdPwr[4] = { 1, 2, 4, 5 };
    static const int16_t SpdPtl[4] = { 1, 2, 7, 17 };
    static const int16_t SpdCri[4] = { 1, 1, 8, 18 };
    static const int16_t SpdPop[4] = { 1, 1, 9, 19 };
    static const int16_t SpdFir[4] = { 1, 1, 10, 20 };

    /* Per-step work level: Super Slow reuses Slow's budget, Super Fast reuses Fast's. */
    static const int16_t SpdWorkLevel[SIM_SPEED_MAX + 1] = { 0, 1, 2, 3, 3, 1 };
    int16_t x = SpdWorkLevel[SimSpeed];

    switch (mod16) {
    case 0:
        if (++Scycle > 1023) {
            Scycle = 0;
        }
        if (DoInitialEval) {
            DoInitialEval = 0;
            CityEvaluation();
        }
        CityTime++;
        DoLandfill();
        Neighbor_Tick();
        AvCityTax += GetAverageTaxRate();

        if (!(Scycle & 1)) {
            SetValves();
        }
        if ((CityTime & 3) == 0 || (CityTime & 3) == 3) {
            TrafRushHour();
        }
        ClearCensus();

        if (EngineCallbacks.update_date) {
            EngineCallbacks.update_date(1900 + (CityTime / 48), (CityTime % 48) / 4);
        }
        break;

    case 1:
        MapScan(0, (1 * WORLD_X) / 8);
        break;
    case 2:
        MapScan((1 * WORLD_X) / 8, (2 * WORLD_X) / 8);
        break;
    case 3:
        MapScan((2 * WORLD_X) / 8, (3 * WORLD_X) / 8);
        break;
    case 4:
        MapScan((3 * WORLD_X) / 8, (4 * WORLD_X) / 8);
        break;
    case 5:
        MapScan((4 * WORLD_X) / 8, (5 * WORLD_X) / 8);
        break;
    case 6:
        MapScan((5 * WORLD_X) / 8, (6 * WORLD_X) / 8);
        break;
    case 7:
        MapScan((6 * WORLD_X) / 8, (7 * WORLD_X) / 8);
        break;
    case 8:
        MapScan((7 * WORLD_X) / 8, WORLD_X);
        break;

    case 9:
        if (!(CityTime % CENSUSRATE)) {
            TakeCensus();
        }
        if (!(CityTime % (CENSUSRATE * 12))) {
            Take2Census();
        }
        if (!(CityTime % TAXFREQ)) {
            CollectTax();
            CityEvaluation();
        }
        break;

    case 10:
        if (!(Scycle % 5)) {
            DecROGMem();
        }
        SendMessages();
        if (EngineCallbacks.invalidate_maps) {
            EngineCallbacks.invalidate_maps();
        }
        break;

    case 11:
        if (!(Scycle % SpdPwr[x])) {
            DoPowerScan();
            DoWaterScan();
            NewPower = 1;
        }
        break;

    case 12:
        if (!(Scycle % SpdPtl[x])) {
            PTLScan();
        }
        break;

    case 13:
        if (!(Scycle % SpdCri[x])) {
            CrimeScan();
        }
        break;

    case 14:
        if (!(Scycle % SpdPop[x])) {
            PopDenScan();
        }
        break;

    case 15:
        if (!(Scycle % SpdFir[x])) {
            FireAnalysis();
            HealthAnalysis();
            RecAnalysis();
        }
        DoDisasters();
        break;

    default:
        break;
    }
}

static void DecROGMem(void)
{
    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            int16_t z = RateOGMem[x][y];
            if (z == 0) continue;
            if (z > 0) {
                --RateOGMem[x][y];
                if (z > 200) RateOGMem[x][y] = 200;
            } else if (z < 0) {
                ++RateOGMem[x][y];
                if (z < -200) RateOGMem[x][y] = -200;
            }
        }
    }
}

void ClearCensus(void)
{
    PwrdZCnt = 0;
    unPwrdZCnt = 0;
    UnwateredZCnt = 0;
    FirePop = 0;
    RoadTotal = 0;
    RailTotal = 0;
    ResPop = 0;
    TrafServicedResPop = 0;
    ResTaxPop = 0;
    ResUncoveredPop = 0;
    ResUnrecPop = 0;
    ComPop = 0;
    IndPop = 0;
    ResZPop = 0;
    ComZPop = 0;
    IndZPop = 0;
    HospPop = 0;
    ChurchPop = 0;
    PolicePop = 0;
    FireStPop = 0;
    StadiumPop = 0;
    CoalPop = 0;
    NuclearPop = 0;
    PortPop = 0;
    APortPop = 0;
    PowerStackNum = 0;
}

static int32_t TaxTableIndex(int16_t rate)
{
    int32_t z = rate + GameLevel;
    if (z > 20) z = 20;
    if (z < 0) z = 0;
    return z;
}

static void SetValves(void)
{
    static const int16_t TaxTable[21] = {
        200, 150, 120, 100, 80, 50, 30, 0, -10, -40, -100,
        -150, -200, -250, -300, -350, -400, -450, -500, -550, -600
    };

    float Employment, Migration, Births, LaborBase, IntMarket;
    float Rratio, Cratio, Iratio, temp;
    float NormResPop, PjResPop, PjComPop, PjIndPop;

    MiscHis[1] = (int16_t)EMarket;
    MiscHis[2] = (int16_t)ResPop;
    MiscHis[3] = ComPop;
    MiscHis[4] = IndPop;
    MiscHis[5] = RValve;
    MiscHis[6] = CValve;
    MiscHis[7] = IValve;
    MiscHis[10] = CrimeRamp;
    MiscHis[11] = PolluteRamp;
    MiscHis[12] = LVAverage;
    MiscHis[13] = CrimeAverage;
    MiscHis[14] = PolluteAverage;
    MiscHis[15] = GameLevel;
    MiscHis[16] = CityClass;
    MiscHis[17] = CityScore;

    NormResPop = (float)(ResPop / 8);
    LastTotalPop = TotalPop;
    TotalPop = (int16_t)(NormResPop + ComPop + IndPop);

    if (NormResPop > 0.0f) {
        Employment = ((float)(ComHis[1] + IndHis[1]) / NormResPop);
    } else {
        Employment = 1.0f;
    }

    Migration = NormResPop * (Employment - 1.0f);
    if (Migration > 0.0f) {
        Migration *= TrafServiceShare();
    }
    Births = NormResPop * 0.02f;
    PjResPop = NormResPop + Migration + Births;

    temp = (float)(ComHis[1] + IndHis[1]);
    if (temp > 0.0f) {
        LaborBase = ((float)ResHis[1] / temp);
    } else {
        LaborBase = 1.0f;
    }

    if (LaborBase > 1.3f) LaborBase = 1.3f;
    if (LaborBase < 0.0f) LaborBase = 0.0f;

    IntMarket = (NormResPop + (float)ComPop + (float)IndPop) / 3.7f;
    PjComPop = IntMarket * LaborBase;

    temp = 1.0f;
    switch (GameLevel) {
    case 0: temp = 1.2f; break;
    case 1: temp = 1.1f; break;
    case 2: temp = 0.98f; break;
    default: break;
    }

    PjIndPop = (float)IndPop * LaborBase * temp;
    if (PjIndPop < 5.0f) PjIndPop = 5.0f;

    if (NormResPop > 0.0f) {
        Rratio = (PjResPop / NormResPop);
    } else {
        Rratio = 1.3f;
    }

    if (ComPop > 0) {
        Cratio = (PjComPop / (float)ComPop);
    } else {
        Cratio = PjComPop;
    }

    if (IndPop > 0) {
        Iratio = (PjIndPop / (float)IndPop);
    } else {
        Iratio = PjIndPop;
    }

    if (Rratio > 2.0f) Rratio = 2.0f;
    if (Cratio > 2.0f) Cratio = 2.0f;
    if (Iratio > 2.0f) Iratio = 2.0f;

    Rratio = ((Rratio - 1.0f) * 600.0f) + (float)TaxTable[TaxTableIndex(ResTax)];
    Cratio = ((Cratio - 1.0f) * 600.0f) + (float)TaxTable[TaxTableIndex(ComTax)];
    Iratio = ((Iratio - 1.0f) * 600.0f) + (float)TaxTable[TaxTableIndex(IndTax)];

    /* Unix stores (short)(valve + ratio), so truncation applies to the sum */
    if (Rratio > 0.0f && RValve < 2000) RValve = (int16_t)(RValve + Rratio);
    if (Rratio < 0.0f && RValve > -2000) RValve = (int16_t)(RValve + Rratio);
    if (Cratio > 0.0f && CValve < 1500) CValve = (int16_t)(CValve + Cratio);
    if (Cratio < 0.0f && CValve > -1500) CValve = (int16_t)(CValve + Cratio);
    if (Iratio > 0.0f && IValve < 1500) IValve = (int16_t)(IValve + Iratio);
    if (Iratio < 0.0f && IValve > -1500) IValve = (int16_t)(IValve + Iratio);

    if (RValve > 2000) RValve = 2000;
    if (RValve < -2000) RValve = -2000;
    if (CValve > 1500) CValve = 1500;
    if (CValve < -1500) CValve = -1500;
    if (IValve > 1500) IValve = 1500;
    if (IValve < -1500) IValve = -1500;

    if (ResCap && (RValve > 0)) RValve = 0;
    if (ComCap && (CValve > 0)) CValve = 0;
    if (IndCap && (IValve > 0)) IValve = 0;

    ValveFlag = 1;
}

static void TakeCensus(void)
{
    for (int16_t x = 118; x >= 0; x--) {
        ResHis[x + 1] = ResHis[x];
        ComHis[x + 1] = ComHis[x];
        IndHis[x + 1] = IndHis[x];
        CrimeHis[x + 1] = CrimeHis[x];
        PollutionHis[x + 1] = PollutionHis[x];
        MoneyHis[x + 1] = MoneyHis[x];
    }

    ResHis[0] = ResPop / 8;
    ComHis[0] = ComPop;
    IndHis[0] = IndPop;

    CrimeRamp += (int16_t)((CrimeAverage - CrimeRamp) / 4);
    CrimeHis[0] = CrimeRamp;

    PolluteRamp += (int16_t)((PolluteAverage - PolluteRamp) / 4);
    PollutionHis[0] = PolluteRamp;

    int32_t x_cash = (CashFlow / 20) + 128;
    if (x_cash < 0) x_cash = 0;
    if (x_cash > 255) x_cash = 255;
    MoneyHis[0] = (int16_t)x_cash;

    if (CrimeHis[0] > 255) CrimeHis[0] = 255;
    if (PollutionHis[0] > 255) PollutionHis[0] = 255;

    CensusChanged = 1;

    if (ChurchPop < (ResPop >> 8)) NeedChurch = TRUE;
    if (ChurchPop > (ResPop >> 8)) NeedChurch = -1;
    if (ChurchPop == (ResPop >> 8)) NeedChurch = FALSE;
}

static void Take2Census(void)
{
    for (int16_t x = 238; x >= 120; x--) {
        ResHis[x + 1] = ResHis[x];
        ComHis[x + 1] = ComHis[x];
        IndHis[x + 1] = IndHis[x];
        CrimeHis[x + 1] = CrimeHis[x];
        PollutionHis[x + 1] = PollutionHis[x];
        MoneyHis[x + 1] = MoneyHis[x];
    }

    ResHis[120] = ResPop / 8;
    ComHis[120] = ComPop;
    IndHis[120] = IndPop;
    CrimeHis[120] = CrimeHis[0];
    PollutionHis[120] = PollutionHis[0];
    MoneyHis[120] = MoneyHis[0];

    CensusChanged = 1;
}

void ApplyBudgetCommit(void)
{
    RoadSpend = (int32_t)((float)RoadFund * roadPercent);
    PoliceSpend = (int32_t)((float)PoliceFund * policePercent);
    FireSpend = (int32_t)((float)FireFund * firePercent);
    HealthSpend = (int32_t)((float)HealthFund * healthPercent);
    RecSpend = (int32_t)((float)RecFund * recreationPercent);
    LandfillSpend = (int32_t)((float)LandfillFund * landfillPercent);

    if (TotalPop) {
        CashFlow = (int16_t)(TaxFund - (PoliceSpend + FireSpend + RoadSpend + HealthSpend + RecSpend + LandfillSpend));
        DoBudget();
    } else {
        RoadEffect = 32;
        PoliceEffect = 1000;
        FireEffect = 1000;
        HealthEffect = 1000;
        RecEffect = 1000;
    }
}

static const float RLevels[3] = { 0.7f, 0.9f, 1.2f };

int32_t CalcRoadFund(int16_t level)
{
    return (int32_t)((float)(RoadTotal + (RailTotal * 2)) * RLevels[level])
         + PortPop * PORT_COST + APortPop * AIRPORT_COST;
}

void PortSystemStats(int32_t *capacity, int32_t *demand)
{
    *capacity = PortPop * PORT_SERVICE_ZONES + APortPop * AIRPORT_SERVICE_ZONES;
    *demand = (int32_t)ComZPop + (int32_t)IndZPop;
}

int16_t GetAverageTaxRate(void)
{
    return (int16_t)((ResTax + ComTax + IndTax) / 3);
}

static const float FLevels[3] = { 3.0f, 2.5f, 2.0f };

int32_t CalcTaxFund(void)
{
    float resBase = (float)(ResTaxPop / 8);
    float weighted = (resBase * (float)ResTax) + ((float)ComPop * (float)ComTax)
                   + ((float)IndPop * (float)IndTax);
    return (int32_t)(weighted * (float)LVAverage / 60.0f * FLevels[GameLevel]);
}

void CollectTax(void)
{
    if (!TaxFlag) {
        AvCityTax = 0;
        PoliceFund = PolicePop * POLICE_STATION_COST;
        FireFund = FireStPop * FIRE_STATION_COST;
        HealthFund = HospPop * HOSPITAL_COST;
        RecFund = ParkPop * PARK_COST + StadiumPop * STADIUM_COST;
        LandfillFund = LandfillPop * LANDFILL_CENTRE_COST;
        RoadFund = CalcRoadFund(GameLevel);
        TaxFund = CalcTaxFund();
        DealsIncome = Neighbor_TakeDealsFlow();

        RoadSpend = (int32_t)((float)RoadFund * roadPercent);
        PoliceSpend = (int32_t)((float)PoliceFund * policePercent);
        FireSpend = (int32_t)((float)FireFund * firePercent);
        HealthSpend = (int32_t)((float)HealthFund * healthPercent);
        RecSpend = (int32_t)((float)RecFund * recreationPercent);
        LandfillSpend = (int32_t)((float)LandfillFund * landfillPercent);

        if (!autoBudget) {
            if (EngineCallbacks.request_budget_dialog) {
                EngineCallbacks.request_budget_dialog();
            } else {
                SetSimSpeed(0);
            }
        } else {
            ApplyBudgetCommit();
        }
    }
}

void DoBudget(void)
{
    TotalFunds += CashFlow;
    if (TotalFunds < 0) {
        TotalFunds = 0;
    }
    MustUpdateFunds = 1;

    if (EngineCallbacks.update_funds) {
        EngineCallbacks.update_funds(TotalFunds);
    }
    UpdateFundEffects();
}

static void UpdateFundEffects(void)
{
    RoadEffect = RoadFund > 0
        ? (int16_t)(((float)RoadSpend / (float)RoadFund) * 32.0f)
        : 32;
    PoliceEffect = PoliceFund > 0
        ? (int16_t)(((float)PoliceSpend / (float)PoliceFund) * 1000.0f)
        : 1000;
    FireEffect = FireFund > 0
        ? (int16_t)(((float)FireSpend / (float)FireFund) * 1000.0f)
        : 1000;
    HealthEffect = HealthFund > 0
        ? (int16_t)(((float)HealthSpend / (float)HealthFund) * 1000.0f)
        : 1000;
    RecEffect = RecFund > 0
        ? (int16_t)(((float)RecSpend / (float)RecFund) * 1000.0f)
        : 1000;
}

static void MapScan(int32_t x1, int32_t x2)
{
    for (int32_t x = x1; x < x2; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            CChr = (int16_t)Map[x][y];
            if (CChr != 0) {
                CChr9 = CChr & LOMASK;
                if (CChr9 >= FLOOD) {
                    SMapX = (int16_t)x;
                    SMapY = (int16_t)y;

                    if (CChr9 < ROADBASE) {
                        if (CChr9 >= FIREBASE) {
                            FirePop++;
                            if (!(Rand16() & 3)) {
                                DoFire();
                            }
                            continue;
                        }
                        if (CChr9 < RADTILE) {
                            DoFlood();
                        } else {
                            DoRadTile();
                        }
                        continue;
                    }

                    if (NewPower && (CChr & CONDBIT)) {
                        SetZPower();
                    }

                    if ((CChr9 >= ROADBASE) && (CChr9 < POWERBASE)) {
                        DoRoad();
                        continue;
                    }

                    if (TILE_IS_TRAFFIC_BLOCK(CChr9)) {
                        continue;
                    }

                    if (CChr & ZONEBIT) {
                        DoZone();
                        continue;
                    }

                    /* Water towers supply water, so they are not counted against water demand */
                    if (CChr9 == WATERTOWER) {
                        CountUnzonedUtilities(false);
                    } else if ((CChr9 >= FOUNTAIN) && (CChr9 < FOUNTAIN + FOUNTAIN_FRAMES)) {
                        CountUnzonedUtilities(true);
                    }

                    if ((CChr9 >= RAILBASE) && (CChr9 < RESBASE)) {
                        DoRail();
                        continue;
                    }

                    if ((CChr9 >= SOMETINYEXP) && (CChr9 <= LASTTINYEXP)) {
                        Map[x][y] = (uint16_t)(RUBBLE + (Rand16() & 3) + BULLBIT);
                    }
                }
            }
        }
    }
}

static void DoRail(void)
{
    RailTotal++;
    GenerateTrain(SMapX, SMapY);

    if (RoadEffect < 30) {
        if (!(Rand16() & 511)) {
            if (!(CChr & CONDBIT)) {
                if (RoadEffect < (Rand16() & 31)) {
                    if (CChr9 < (RAILBASE + 2)) {
                        Map[SMapX][SMapY] = RIVER;
                    } else {
                        Map[SMapX][SMapY] = (uint16_t)(RUBBLE + (Rand16() & 3) + BULLBIT);
                    }
                }
            }
        }
    }
}

static void DoRadTile(void)
{
    if (!(Rand16() & 4095)) {
        Map[SMapX][SMapY] = DIRT;
    }
}

static void DoRoad(void)
{
    int16_t Density, tden;
    static const int16_t DenTab[3] = { ROADBASE, LTRFBASE, HTRFBASE };

    RoadTotal++;

    if (RoadEffect < 30) {
        if (!(Rand16() & 511)) {
            if (!(CChr & CONDBIT)) {
                if (RoadEffect < (Rand16() & 31)) {
                    if (((CChr9 & 15) < 2) || ((CChr9 & 15) == 15)) {
                        Map[SMapX][SMapY] = RIVER;
                    } else {
                        Map[SMapX][SMapY] = (uint16_t)(RUBBLE + (Rand16() & 3) + BULLBIT);
                    }
                    return;
                }
            }
        }
    }

    if (!(CChr & BURNBIT)) {
        RoadTotal += 4;
        if (DoBridge()) {
            return;
        }
    }

    if (CChr9 < LTRFBASE) {
        tden = 0;
    } else if (CChr9 < HTRFBASE) {
        tden = 1;
    } else {
        RoadTotal++;
        tden = 2;
    }

    /* Any rush-hour load animates. Heavy load (192 and up) uses the heavy-traffic sprites. */
    Density = TrfDensity[SMapX >> 1][SMapY >> 1];
    Density = (int16_t)(Density == 0 ? 0 : (Density < 192 ? 1 : 2));
    /* Congestion keeps every road drawn as traffic, so the congestion tier applies to all of them. */
    if (TrafGridlocked()) {
        Density = 2;
    } else if (TrafAtCapacity() && Density == 0) {
        Density = 1;
    }

    if (tden != Density) {
        uint16_t z = (uint16_t)(((CChr9 - ROADBASE) & 15) + DenTab[Density]);
        z |= (uint16_t)(CChr & (ALLBITS - ANIMBIT));
        if (Density) z |= ANIMBIT;
        Map[SMapX][SMapY] = z;
    }
}

static int32_t DoBridge(void)
{
    static const int16_t HDx[7] = { -2, 2, -2, -1, 0, 1, 2 };
    static const int16_t HDy[7] = { -1, -1, 0, 0, 0, 0, 0 };
    static const uint16_t HBRTAB[7] = {
        HBRDG1 | BULLBIT, HBRDG3 | BULLBIT, HBRDG0 | BULLBIT,
        RIVER, BRWH | BULLBIT, RIVER, HBRDG2 | BULLBIT
    };
    static const uint16_t HBRTAB2[7] = {
        RIVER, RIVER, HBRIDGE | BULLBIT, HBRIDGE | BULLBIT, HBRIDGE | BULLBIT,
        HBRIDGE | BULLBIT, HBRIDGE | BULLBIT
    };

    static const int16_t VDx[7] = { 0, 1, 0, 0, 0, 0, 1 };
    static const int16_t VDy[7] = { -2, -2, -1, 0, 1, 2, 2 };
    static const uint16_t VBRTAB[7] = {
        VBRDG0 | BULLBIT, VBRDG1 | BULLBIT, RIVER, BRWV | BULLBIT,
        RIVER, VBRDG2 | BULLBIT, VBRDG3 | BULLBIT
    };
    static const uint16_t VBRTAB2[7] = {
        VBRIDGE | BULLBIT, RIVER, VBRIDGE | BULLBIT, VBRIDGE | BULLBIT,
        VBRIDGE | BULLBIT, VBRIDGE | BULLBIT, RIVER
    };

    int32_t x, y, z;
    uint16_t MPtem;

    if (CChr9 == BRWV) {
        if (!(Rand16() & 3) && (GetBoatDis() > 340)) {
            for (z = 0; z < 7; z++) {
                x = SMapX + VDx[z];
                y = SMapY + VDy[z];
                if (TestBounds(x, y)) {
                    if ((Map[x][y] & LOMASK) == (VBRTAB[z] & LOMASK)) {
                        Map[x][y] = VBRTAB2[z];
                    }
                }
            }
        }
        return TRUE;
    }

    if (CChr9 == BRWH) {
        if (!(Rand16() & 3) && (GetBoatDis() > 340)) {
            for (z = 0; z < 7; z++) {
                x = SMapX + HDx[z];
                y = SMapY + HDy[z];
                if (TestBounds(x, y)) {
                    if ((Map[x][y] & LOMASK) == (HBRTAB[z] & LOMASK)) {
                        Map[x][y] = HBRTAB2[z];
                    }
                }
            }
        }
        return TRUE;
    }

    if ((GetBoatDis() < 300) || !(Rand16() & 7)) {
        if (CChr9 & 1) {
            if (SMapX < (WORLD_X - 1) && Map[SMapX + 1][SMapY] == CHANNEL) {
                for (z = 0; z < 7; z++) {
                    x = SMapX + VDx[z];
                    y = SMapY + VDy[z];
                    if (TestBounds(x, y)) {
                        MPtem = Map[x][y];
                        if ((MPtem == CHANNEL) || ((MPtem & 15) == (VBRTAB2[z] & 15))) {
                            Map[x][y] = VBRTAB[z];
                        }
                    }
                }
                return TRUE;
            }
            return FALSE;
        } else {
            if (SMapY > 0 && Map[SMapX][SMapY - 1] == CHANNEL) {
                for (z = 0; z < 7; z++) {
                    x = SMapX + HDx[z];
                    y = SMapY + HDy[z];
                    if (TestBounds(x, y)) {
                        MPtem = Map[x][y];
                        if (((MPtem & 15) == (HBRTAB2[z] & 15)) || (MPtem == CHANNEL)) {
                            Map[x][y] = HBRTAB[z];
                        }
                    }
                }
                return TRUE;
            }
            return FALSE;
        }
    }
    return FALSE;
}

static int32_t GetBoatDis(void)
{
    int32_t dist = 99999;
    int32_t mx = (SMapX << 4) + 8;
    int32_t my = (SMapY << 4) + 8;

    for (int32_t i = 0; i < OBJN; i++) {
        SimSprite *sprite = GlobalSprites[i];
        if (sprite && (sprite->type == SHI) && (sprite->frame != 0)) {
            int32_t dx = sprite->x + sprite->x_hot - mx;
            int32_t dy = sprite->y + sprite->y_hot - my;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            int32_t total = dx + dy;
            if (total < dist) dist = total;
        }
    }
    return dist;
}

static void DoFire(void)
{
    static const int16_t DX[4] = { -1, 0, 1, 0 };
    static const int16_t DY[4] = { 0, -1, 0, 1 };
    int16_t Rate;

    for (int32_t z = 0; z < 4; z++) {
        if (!(Rand16() & 7)) {
            int16_t Xtem = SMapX + DX[z];
            int16_t Ytem = SMapY + DY[z];
            if (TestBounds(Xtem, Ytem)) {
                uint16_t c = Map[Xtem][Ytem];
                if (c & BURNBIT) {
                    if (c & ZONEBIT) {
                        FireZone(Xtem, Ytem, (int32_t)c);
                        if ((c & LOMASK) > IZB) {
                            MakeExplosionAt((Xtem << 4) + 8, (Ytem << 4) + 8);
                        }
                    }
                    Map[Xtem][Ytem] = (uint16_t)(FIRE + (Rand16() & 3) + ANIMBIT);
                }
            }
        }
    }

    Rate = FireBurnRate(SMapX, SMapY);
    if (!Rand(Rate)) {
        Map[SMapX][SMapY] = (uint16_t)(RUBBLE + (Rand16() & 3) + BULLBIT);
    }
}

void FireZone(int32_t xloc, int32_t yloc, int32_t ch)
{
    RateOGMem[xloc >> 3][yloc >> 3] -= 20;
    ch = ch & LOMASK;

    int16_t XYmax = (ch < PORTBASE) ? 2 : ((ch == AIRPORT) ? 5 : 4);

    for (int16_t x = -1; x < XYmax; x++) {
        for (int16_t y = -1; y < XYmax; y++) {
            int16_t Xtem = (int16_t)(xloc + x);
            int16_t Ytem = (int16_t)(yloc + y);
            if (!TestBounds(Xtem, Ytem)) continue;
            if ((Map[Xtem][Ytem] & LOMASK) >= ROADBASE) {
                Map[Xtem][Ytem] |= BULLBIT;
            }
        }
    }
}

void RepairZone(int16_t zone_id, int16_t size)
{
    int16_t cnt = 0;
    size--;

    for (int16_t y = -1; y < size; y++) {
        for (int16_t x = -1; x < size; x++) {
            int16_t xx = SMapX + x;
            int16_t yy = SMapY + y;
            cnt++;
            if (TestBounds(xx, yy)) {
                uint16_t thch = Map[xx][yy];
                if (thch & ZONEBIT) continue;
                if (thch & ANIMBIT) continue;
                uint16_t base = thch & LOMASK;
                if ((base < RUBBLE) || (base >= ROADBASE)) {
                    Map[xx][yy] = (uint16_t)(zone_id - 3 - size + cnt + CONDBIT + BURNBIT);
                }
            }
        }
    }
}

void DoSPZone(int16_t pwron)
{
    static const int16_t MltdwnTab[3] = { 30000, 20000, 10000 };

    switch (CChr9) {
    case POWERPLANT: {
        bool watered = WaterPlantSupplied(SMapX, SMapY);
        if (watered) {
            CoalPop++;
        }
        if (!(CityTime & 7)) {
            RepairZone(POWERPLANT, 4);
        }
        if (watered) {
            PushPowerStack();
        }
        CoalSmoke(SMapX, SMapY);
        return;
    }

    case NUCLEAR: {
        if (!NoDisasters && !Rand(MltdwnTab[GameLevel])) {
            DoMeltdown(SMapX, SMapY);
            return;
        }
        bool watered = WaterPlantSupplied(SMapX, SMapY);
        if (watered) {
            NuclearPop++;
        }
        if (!(CityTime & 7)) {
            RepairZone(NUCLEAR, 4);
        }
        if (watered) {
            PushPowerStack();
        }
        return;
    }

    case FIRESTATION:
        FireStPop++;
        if (!(CityTime & 7)) {
            RepairZone(FIRESTATION, 3);
        }
        return;

    case POLICESTATION:
        PolicePop++;
        if (!(CityTime & 7)) {
            RepairZone(POLICESTATION, 3);
        }
        return;

    case STADIUM:
        StadiumPop++;
        if (!(CityTime & 15)) {
            RepairZone(STADIUM, 4);
        }
        if (pwron && !((CityTime + SMapX + SMapY) & 31)) {
            DrawStadium(FULLSTADIUM);
            Map[SMapX + 1][SMapY] = FOOTBALLGAME1 + ANIMBIT;
            Map[SMapX + 1][SMapY + 1] = FOOTBALLGAME2 + ANIMBIT;
        }
        return;

    case FULLSTADIUM:
        StadiumPop++;
        if (!((CityTime + SMapX + SMapY) & 7)) {
            DrawStadium(STADIUM);
        }
        return;

    case AIRPORT:
        APortPop++;
        if (!(CityTime & 7)) {
            RepairZone(AIRPORT, 6);
        }
        if (pwron) {
            if ((Map[SMapX + 1][SMapY - 1] & LOMASK) == RADAR) {
                Map[SMapX + 1][SMapY - 1] = RADAR + ANIMBIT + CONDBIT + BURNBIT;
            }
            DoAirport();
        } else {
            Map[SMapX + 1][SMapY - 1] = RADAR + CONDBIT + BURNBIT;
        }
        return;

    case PORT:
        PortPop++;
        if ((CityTime & 15) == 0) {
            RepairZone(PORT, 4);
        }
        if (pwron) {
            GenerateShip();
        }
        return;

    default:
        break;
    }
}

static void DrawStadium(int32_t z)
{
    z -= 5;
    for (int32_t y = (SMapY - 1); y < (SMapY + 3); y++) {
        for (int32_t x = (SMapX - 1); x < (SMapX + 3); x++) {
            Map[x][y] = (uint16_t)((z++) | BNCNBIT);
        }
    }
    Map[SMapX][SMapY] |= (ZONEBIT | PWRBIT);
}

static void DoAirport(void)
{
    if (!Rand(5)) {
        GeneratePlane(SMapX, SMapY);
        return;
    }
    if (!Rand(12)) {
        GenerateCopter(SMapX, SMapY);
    }
}

static void CoalSmoke(int32_t mx, int32_t my)
{
    static const uint16_t SmTb[4] = { COALSMOKE1, COALSMOKE2, COALSMOKE3, COALSMOKE4 };
    static const int16_t dx[4] = { 1, 2, 1, 2 };
    static const int16_t dy[4] = { -1, -1, 0, 0 };

    for (int32_t x = 0; x < 4; x++) {
        Map[mx + dx[x]][my + dy[x]] = SmTb[x] | ANIMBIT | CONDBIT | PWRBIT | BURNBIT;
    }
}

void DoMeltdown(int32_t sx, int32_t sy)
{
    MeltX = (int16_t)sx;
    MeltY = (int16_t)sy;

    MakeExplosion(sx - 1, sy - 1);
    MakeExplosion(sx - 1, sy + 2);
    MakeExplosion(sx + 2, sy - 1);
    MakeExplosion(sx + 2, sy + 2);

    for (int32_t x = (sx - 1); x < (sx + 3); x++) {
        for (int32_t y = (sy - 1); y < (sy + 3); y++) {
            Map[x][y] = (uint16_t)(FIRE + (Rand16() & 3) + ANIMBIT);
        }
    }

    for (int32_t z = 0; z < 200; z++) {
        int32_t x = sx - 20 + Rand(40);
        int32_t y = sy - 15 + Rand(30);
        if (!TestBounds(x, y)) continue;
        uint16_t t = Map[x][y];
        if (t & ZONEBIT) continue;
        if ((t & BURNBIT) || (t == 0)) {
            Map[x][y] = RADTILE;
        }
    }

    ClearMes();
    SendMesAt(-43, (int16_t)sx, (int16_t)sy);
}

void DoNilPower(void)
{
    for (int16_t x = 0; x < WORLD_X; x++) {
        for (int16_t y = 0; y < WORLD_Y; y++) {
            uint16_t z = Map[x][y];
            if (z & ZONEBIT) {
                SMapX = x;
                SMapY = y;
                CChr = (int16_t)z;
                SetZPower();
            }
        }
    }
}

static void SetCommonInits(void)
{
    EvalInit();
    RoadEffect = 32;
    PoliceEffect = 1000;
    FireEffect = 1000;
    HealthEffect = 1000;
    RecEffect = 1000;
    roadPercent = 1.0f;
    policePercent = 1.0f;
    firePercent = 1.0f;
    healthPercent = 1.0f;
    recreationPercent = 1.0f;
    LandfillResetCity();
    TaxFlag = 0;
    TaxFund = 0;
    DealsIncome = 0;
}

static void ClearCityMemory(void)
{
    InitSprites();

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            PopDensity[x][y]   = 0;
            TrfDensity[x][y]   = 0;
            PollutionMem[x][y] = 0;
            LandValueMem[x][y] = 0;
            CrimeMem[x][y]     = 0;
        }
    }

    for (int16_t x = 0; x < QWX; x++) {
        for (int16_t y = 0; y < QWY; y++) {
            TerrainMem[x][y] = 0;
        }
    }

    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            RateOGMem[x][y]       = 0;
            ComRate[x][y]         = 0;
        }
    }
}

void Sim_InitEngine(void)
{
    AllocateMapBuffers();

    Fcycle = 0;
    Scycle = 0;
    Spdcycle = 0;
    Cheat_Reset();

    ClearCityMemory();
    SetCommonInits();
    ResetMilestoneState();
    News_ResetSession();
    Neighbor_Reset();

    for (int16_t x = 0; x < 240; x++) {
        ResHis[x] = 0;
        ComHis[x] = 0;
        IndHis[x] = 0;
        MoneyHis[x] = 128;
        CrimeHis[x] = 0;
        PollutionHis[x] = 0;
    }

    CrimeRamp = 0;
    PolluteRamp = 0;
    TotalPop = 0;
    RValve = 0;
    CValve = 0;
    IValve = 0;
    ResCap = 0;
    ComCap = 0;
    IndCap = 0;

    EMarket = 6.0f;
    DisasterEvent = 0;
    ScoreType = 0;
    PowerStackNum = 0;

    SetValves();
    ClearCensus();
    MapScan(0, WORLD_X);
    DoPowerScan();
    DoWaterScan();
    NewPower = 1;

    PTLScan();
    CrimeScan();
    PopDenScan();
    FireAnalysis();
    HealthAnalysis();
    RecAnalysis();

    TotalPop = 1;
    DoInitialEval = 1;
}