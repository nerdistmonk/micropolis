/* s_eval.c: City evaluation, voter opinion polling, problem ranking, and scoring.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"

/* Evaluation State Variables */
int16_t EvalValid = 0;
int16_t CityYes = 0;
int16_t CityNo = 0;
int16_t ProblemTable[PROBNUM];
static int16_t ProblemTaken[PROBNUM];
int16_t ProblemVotes[PROBNUM];
int16_t ProblemOrder[4];
QUAD    CityPop = 0;
QUAD    deltaCityPop = 0;
QUAD    CityAssValue = 0;
int16_t ParkPop = 0;
int16_t CityClass = 0;
int16_t CityScore = 500;
int16_t deltaCityScore = 0;
int16_t TrafficAverage = 0;
int32_t EvalChanged = 0;

/* Forward Declarations of Internal Functions */
static void    DoPopNum(void);
static void    DoProblems(void);
static void    VoteProblems(void);
static int16_t AverageTrf(void);
static int16_t GetUnemployment(void);
static int16_t GetFire(void);
static void    GetScore(void);
static void    DoVotes(void);
static void    CountParks(void);

void CityEvaluation(void)
{
    EvalValid = 0;
    if (TotalPop) {
        CountParks();
        GetAssValue();
        DoPopNum();
        DoProblems();
        GetScore();
        DoVotes();
        EvalChanged = 1;
    } else {
        EvalInit();
        EvalChanged = 1;
    }
    EvalValid = 1;
}

void EvalInit(void)
{
    CityYes        = 0;
    CityNo         = 0;
    CityPop        = 0;
    deltaCityPop   = 0;
    CityAssValue   = 0;
    CityClass      = 0;
    CityScore      = 500;
    deltaCityScore = 0;
    EvalValid      = 1;

    for (int16_t x = 0; x < PROBNUM; x++) {
        ProblemVotes[x] = 0;
    }
    for (int16_t x = 0; x < 4; x++) {
        ProblemOrder[x] = 0;
    }
}

/* Parks are fountains; scenario maps contain natural WOODS2-WOODS5, so those are not counted. */
static void CountParks(void)
{
    int16_t count = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y] & LOMASK;
            if (t >= FOUNTAIN && t < FOUNTAIN + FOUNTAIN_FRAMES) {
                count++;
            }
        }
    }
    ParkPop = count;
}

void GetAssValue(void)
{
    QUAD z;

    z = (QUAD)RoadTotal * 5;
    z += (QUAD)RailTotal * 10;
    z += (QUAD)PolicePop * 1000;
    z += (QUAD)FireStPop * 1000;
    z += (QUAD)HospPop * 400;
    z += (QUAD)StadiumPop * 3000;
    z += (QUAD)ParkPop * 750;
    z += (QUAD)PortPop * 5000;
    z += (QUAD)APortPop * 10000;
    z += (QUAD)CoalPop * 3000;
    z += (QUAD)NuclearPop * 6000;

    CityAssValue = z * 1000;
}

static void DoPopNum(void)
{
    QUAD old_city_pop = CityPop;

    CityPop = ((QUAD)ResPop + ((QUAD)ComPop * 8) + ((QUAD)IndPop * 8)) * 20;
    if (old_city_pop == -1) {
        old_city_pop = CityPop;
    }
    deltaCityPop = CityPop - old_city_pop;

    CityClass = 0; /* Village */
    if (CityPop > 2000)   CityClass++; /* Town */
    if (CityPop > 10000)  CityClass++; /* City */
    if (CityPop > 50000)  CityClass++; /* Capital */
    if (CityPop > 100000) CityClass++; /* Metropolis */
    if (CityPop > 500000) CityClass++; /* Megalopolis */
}

static int16_t ShareOfZones(int32_t count)
{
    int32_t total = (int32_t)PwrdZCnt + (int32_t)unPwrdZCnt;
    if (total <= 0) return 0;
    return (int16_t)((count * 255) / total);
}

static int16_t GetHealthcareProblem(void)
{
    if (ResPop <= 0) return 0;
    return (int16_t)((ResUncoveredPop * 255) / ResPop);
}

static void DoProblems(void)
{
    int16_t this_prob = 0;
    int16_t max_vote;

    for (int16_t z = 0; z < PROBNUM; z++) {
        ProblemTable[z] = 0;
    }

    ProblemTable[0] = CrimeAverage;                                /* Crime */
    ProblemTable[1] = PolluteAverage;                              /* Pollution */
    ProblemTable[2] = (int16_t)((float)LVAverage * 0.7f);          /* Housing */
    ProblemTable[3] = (int16_t)(GetAverageTaxRate() * 10);         /* Taxes */
    ProblemTable[4] = AverageTrf();                                /* Traffic */
    ProblemTable[5] = GetUnemployment();                           /* Unemployment */
    ProblemTable[6] = GetFire();                                   /* Fire */
    ProblemTable[7] = ShareOfZones(unPwrdZCnt);                    /* Power */
    ProblemTable[8] = ShareOfZones(UnwateredZCnt);                 /* Water */
    ProblemTable[9] = GetHealthcareProblem();                      /* Healthcare */
    ProblemTable[10] = (int16_t)(TrashPending / 20 > 255 ? 255 : TrashPending / 20); /* Garbage: saturates at a 5100 pool */
    ProblemTable[PROB_RECREATION] = ResPop > 0 ? (int16_t)((ResUnrecPop * 255) / ResPop) : 0; /* Recreation */

    VoteProblems();

    for (int16_t z = 0; z < PROBNUM; z++) {
        ProblemTaken[z] = 0;
    }

    for (int16_t z = 0; z < 4; z++) {
        max_vote = 0;
        for (int16_t x = 0; x < PROB_ACTIVE; x++) {
            if ((ProblemVotes[x] > max_vote) && (!ProblemTaken[x])) {
                this_prob = x;
                max_vote = ProblemVotes[x];
            }
        }
        if (max_vote > 0) {
            ProblemTaken[this_prob] = 1;
            ProblemOrder[z] = this_prob;
        } else {
            ProblemOrder[z] = PROB_NONE;
            ProblemTable[PROB_NONE] = 0;
        }
    }
}

static void VoteProblems(void)
{
    int16_t x = 0;
    int16_t z = 0;
    int16_t count = 0;

    for (int16_t i = 0; i < PROBNUM; i++) {
        ProblemVotes[i] = 0;
    }

    while ((z < 100) && (count < 600)) {
        if (Rand(300) < ProblemTable[x]) {
            ProblemVotes[x]++;
            z++;
        }
        x++;
        if (x >= PROB_ACTIVE) {
            x = 0;
        }
        count++;
    }
}

static int16_t AverageTrf(void)
{
    QUAD trf_total = 0;
    int32_t count = 1;

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            if (LandValueMem[x][y]) {
                trf_total += TrfDensity[x][y];
                count++;
            }
        }
    }

    TrafficAverage = (int16_t)((trf_total / count) * 2.4);
    return TrafficAverage;
}

static int16_t GetUnemployment(void)
{
    int32_t b = (ComPop + IndPop) << 3;
    if (b <= 0) return 0;

    float r = (float)ResPop / (float)b;
    int32_t result = (int32_t)((r - 1.0f) * 255.0f);

    if (result > 255) result = 255;

    return (int16_t)result;
}

static int16_t GetFire(void)
{
    int32_t z = FirePop * 5;
    if (z > 255) return 255;
    return (int16_t)z;
}

static void GetScore(void)
{
    int16_t old_city_score = CityScore;
    int32_t x = 0;

    for (int16_t i = 0; i < PROB_RECREATION; i++) {
        x += ProblemTable[i];
    }

    x = x / 3;
    if (x > 256) x = 256;

    int32_t z = (256 - x) * 4;
    if (z > 1000) z = 1000;
    if (z < 0)    z = 0;

    /* Unix truncates to int after every multiplier; keep that per step */
    if (ResCap) z = (int32_t)(z * 0.85);
    if (ComCap) z = (int32_t)(z * 0.85);
    if (IndCap) z = (int32_t)(z * 0.85);

    if (RoadEffect < 32) {
        z -= (32 - RoadEffect);
    }
    if (PoliceEffect < 1000) {
        z = (int32_t)(z * (0.9 + ((double)PoliceEffect / 10000.1)));
    }
    if (FireEffect < 1000) {
        z = (int32_t)(z * (0.9 + ((double)FireEffect / 10000.1)));
    }

    if (RValve < -1000) z = (int32_t)(z * 0.85);
    if (CValve < -1000) z = (int32_t)(z * 0.85);
    if (IValve < -1000) z = (int32_t)(z * 0.85);

    /* Recreation: up to 10 points (1% of the 1000 max) when no residents are covered. */
    z -= (ProblemTable[PROB_RECREATION] * 10) / 255;

    float sm = 1.0f;
    if ((CityPop == 0) || (deltaCityPop == 0)) {
        sm = 1.0f;
    } else if (deltaCityPop == CityPop) {
        sm = 1.0f;
    } else if (deltaCityPop > 0) {
        sm = ((float)deltaCityPop / (float)CityPop) + 1.0f;
    } else if (deltaCityPop < 0) {
        sm = 0.95f + ((float)deltaCityPop / (float)(CityPop - deltaCityPop));
    }
    z = (int32_t)(z * sm);
    z -= GetFire();
    z -= GetAverageTaxRate();

    float total_zones = (float)(unPwrdZCnt + PwrdZCnt);
    if (total_zones > 0.0f) {
        sm = (float)PwrdZCnt / total_zones;
    } else {
        sm = 1.0f;
    }
    z = (int32_t)(z * sm);
    if (z > 1000) z = 1000;
    if (z < 0)    z = 0;

    CityScore = (int16_t)((CityScore + z) / 2);
    deltaCityScore = (int16_t)(CityScore - old_city_score);
}

static void DoVotes(void)
{
    CityYes = 0;
    CityNo  = 0;

    for (int32_t z = 0; z < 100; z++) {
        if (Rand(1000) < CityScore) {
            CityYes++;
        } else {
            CityNo++;
        }
    }
}