/* s_zone.c: Cellular automata for residential, commercial, and industrial zones.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"

/* Preprocessor Macros */
#define T 1
#define F 0
#define ASCBIT (ANIMBIT | CONDBIT | BURNBIT)
#define REGBIT (CONDBIT | BURNBIT)

/* Forward Declarations of Internal Functions */
static void    DoHospChur(void);
static void    DoResidential(int16_t zone_pwr_flg);
static void    DoCommercial(int16_t zone_pwr_flg);
static void    DoIndustrial(int16_t zone_pwr_flg);
static void    MakeHosp(void);
static int16_t GetCRVal(void);
static void    DoResIn(int32_t pop, int32_t value);
static void    DoComIn(int32_t pop, int32_t value);
static void    DoIndIn(int32_t pop, int32_t value);
static void    DoResOut(int32_t pop, int32_t value);
static void    DoComOut(int32_t pop, int32_t value);
static void    DoIndOut(int32_t pop, int32_t value);
static void    IncROG(int32_t amount);
static void    BuildHouse(int32_t value);
static void    ResPlop(int32_t den, int32_t value);
static void    ComPlop(int32_t den, int32_t value);
static void    IndPlop(int32_t den, int32_t value);
static int16_t EvalLot(int32_t x, int32_t y);
static int16_t EvalRes(int32_t traf);
static int16_t EvalCom(int32_t traf);
static int16_t EvalInd(int32_t traf);

int16_t SetZPower(void)
{
    QUAD power_wrd;

    if ((CChr9 == NUCLEAR) || (CChr9 == POWERPLANT)) {
        Map[SMapX][SMapY] = (uint16_t)(CChr | PWRBIT);
        return 1;
    }

    power_wrd = POWERWORD(SMapX, SMapY);
    if ((power_wrd < PWRMAPSIZE) &&
        (PowerMap[power_wrd] & (uint16_t)(1u << (SMapX & 15)))) {
        Map[SMapX][SMapY] = (uint16_t)(CChr | PWRBIT);
        return 1;
    } else {
        Map[SMapX][SMapY] = (uint16_t)(CChr & (~PWRBIT));
        return 0;
    }
}

void DoZone(void)
{
    int16_t zone_pwr_flg = SetZPower();

    if (zone_pwr_flg) {
        PwrdZCnt++;
    } else {
        unPwrdZCnt++;
    }

    /* Unwatered zones are treated like unpowered ones: no growth */
    zone_pwr_flg = zone_pwr_flg && WaterIsSupplied(SMapX, SMapY);

    if (CChr9 > PORTBASE) {
        DoSPZone(zone_pwr_flg);
        return;
    }
    if (CChr9 < HOSPITAL) {
        DoResidential(zone_pwr_flg);
        return;
    }
    if (CChr9 < COMBASE) {
        DoHospChur();
        return;
    }
    if (CChr9 < INDBASE) {
        DoCommercial(zone_pwr_flg);
        return;
    }

    DoIndustrial(zone_pwr_flg);
}

static void DoHospChur(void)
{
    if (CChr9 == HOSPITAL) {
        HospPop++;
        if (!(CityTime & 15)) {
            RepairZone(HOSPITAL, 3);
        }
        if (NeedHosp == -1) {
            if (!Rand(20)) {
                ZonePlop(RESBASE);
            }
        }
    }

    if (CChr9 == CHURCH) {
        ChurchPop++;
        if (!(CityTime & 15)) {
            RepairZone(CHURCH, 3);
        }
        if (NeedChurch == -1) {
            if (!Rand(20)) {
                ZonePlop(RESBASE);
            }
        }
    }
}

void SetSmoke(int32_t zone_power)
{
    static const int16_t AniThis[8] = { T, F, T, T, F, F, T, T };
    static const int16_t DX1[8]     = { -1, 0, 1, 0, 0, 0, 0, 1 };
    static const int16_t DY1[8]     = { -1, 0, -1, -1, 0, 0, -1, -1 };
    /* Unix assigns frame A then overwrites it with frame B; only B is ever visible. */
    static const int16_t AniTabB[8] = { 0, 0, 36, 44, 0, 0, 52, 60 };
    static const uint16_t AniTabC[8] = { IND1, 0, IND2, IND4, 0, 0, IND6, IND8 };
    static const uint16_t AniTabD[8] = { IND1, 0, IND3, IND5, 0, 0, IND7, IND9 };

    if (CChr9 < IZB) return;

    int16_t z = (int16_t)(((CChr9 - IZB) >> 3) & 7);
    if (AniThis[z]) {
        int32_t xx = SMapX + DX1[z];
        int32_t yy = SMapY + DY1[z];
        if (TestBounds(xx, yy)) {
            if (zone_power) {
                if ((Map[xx][yy] & LOMASK) == AniTabC[z]) {
                    Map[xx][yy] = (uint16_t)(ASCBIT | (SMOKEBASE + AniTabB[z]));
                }
            } else {
                if ((Map[xx][yy] & LOMASK) > AniTabC[z]) {
                    Map[xx][yy] = (uint16_t)(REGBIT | AniTabD[z]);
                }
            }
        }
    }
}

static void DoIndustrial(int16_t zone_pwr_flg)
{
    int16_t tpop, zscore, trf_good;

    IndZPop++;
    SetSmoke(zone_pwr_flg);
    tpop = IZPop(CChr9);
    IndPop += tpop;

    if (tpop > Rand(5)) {
        trf_good = (int16_t)MakeTraf(2);
    } else {
        trf_good = TRUE;
    }

    if (trf_good == -1) {
        DoIndOut(tpop, Rand16() & 1);
        return;
    }

    if (!(Rand16() & 7)) {
        zscore = (int16_t)(IValve + EvalInd(trf_good));
        if (!zone_pwr_flg) {
            zscore = -500;
        }

        if ((zscore > -350) &&
            ((int16_t)(zscore - 26380) > Rand16Signed())) {
            DoIndIn(tpop, Rand16() & 1);
            return;
        }

        if ((zscore < 350) &&
            ((int16_t)(zscore + 26380) < Rand16Signed())) {
            DoIndOut(tpop, Rand16() & 1);
        }
    }
}

static void DoCommercial(int16_t zone_pwr_flg)
{
    int16_t tpop, trf_good;
    int16_t zscore, locvalve, value;

    ComZPop++;
    tpop = CZPop(CChr9);
    ComPop += tpop;

    if (tpop > Rand(5)) {
        trf_good = (int16_t)MakeTraf(1);
    } else {
        trf_good = TRUE;
    }

    if (trf_good == -1) {
        value = GetCRVal();
        DoComOut(tpop, value);
        return;
    }

    if (!(Rand16() & 7)) {
        locvalve = EvalCom(trf_good);
        zscore = (int16_t)(CValve + locvalve);
        if (!zone_pwr_flg) {
            zscore = -500;
        }

        if (trf_good &&
            (zscore > -350) &&
            ((int16_t)(zscore - 26380) > Rand16Signed())) {
            value = GetCRVal();
            DoComIn(tpop, value);
            return;
        }

        if ((zscore < 350) &&
            ((int16_t)(zscore + 26380) < Rand16Signed())) {
            value = GetCRVal();
            DoComOut(tpop, value);
        }
    }
}

static void DoResidential(int16_t zone_pwr_flg)
{
    int16_t tpop, zscore, locvalve, value, trf_good;

    ResZPop++;
    if (CChr9 == FREEZ) {
        tpop = DoFreePop();
    } else {
        tpop = RZPop(CChr9);
    }

    ResPop += tpop;
    ResTaxPop += HealthAdjustedPop(tpop, SMapX, SMapY);

    if (tpop > Rand(35)) {
        trf_good = (int16_t)MakeTraf(0);
    } else {
        trf_good = TRUE;
    }

    if (trf_good == -1) {
        value = GetCRVal();
        DoResOut(tpop, value);
        return;
    }

    if ((CChr9 == FREEZ) || !(Rand16() & 7)) {
        locvalve = EvalRes(trf_good);
        zscore = (int16_t)(RValve + locvalve);
        if (!zone_pwr_flg) {
            zscore = -500;
        }

        if ((zscore > -350) &&
            ((int16_t)(zscore - 26380) > Rand16Signed())) {
            if ((!tpop) && !(Rand16() & 3)) {
                MakeHosp();
                return;
            }
            value = GetCRVal();
            DoResIn(tpop, value);
            return;
        }

        if ((zscore < 350) &&
            ((int16_t)(zscore + 26380) < Rand16Signed())) {
            value = GetCRVal();
            DoResOut(tpop, value);
        }
    }
}

static void MakeHosp(void)
{
    if (NeedHosp > 0) {
        ZonePlop(HOSPITAL - 4);
        NeedHosp = FALSE;
        return;
    }
    if (NeedChurch > 0) {
        ZonePlop(CHURCH - 4);
        NeedChurch = FALSE;
        return;
    }
}

static int16_t GetCRVal(void)
{
    int16_t lval = (int16_t)LandValueMem[SMapX >> 1][SMapY >> 1];
    lval -= (int16_t)PollutionMem[SMapX >> 1][SMapY >> 1];

    if (lval < 30)  return 0;
    if (lval < 80)  return 1;
    if (lval < 150) return 2;
    return 3;
}

static void DoResIn(int32_t pop, int32_t value)
{
    uint8_t z = PollutionMem[SMapX >> 1][SMapY >> 1];
    if (z > 128) return;

    if (CChr9 == FREEZ) {
        if (pop < 8) {
            BuildHouse(value);
            IncROG(1);
            return;
        }
        if (PopDensity[SMapX >> 1][SMapY >> 1] > 64) {
            ResPlop(0, value);
            IncROG(8);
            return;
        }
        return;
    }

    if (pop < 40) {
        ResPlop((pop / 8) - 1, value);
        IncROG(8);
    }
}

static void DoComIn(int32_t pop, int32_t value)
{
    int16_t z = (int16_t)(LandValueMem[SMapX >> 1][SMapY >> 1] >> 5);
    if (pop > z) return;

    if (pop < 5) {
        ComPlop(pop, value);
        IncROG(8);
    }
}

static void DoIndIn(int32_t pop, int32_t value)
{
    if (pop < 4) {
        IndPlop(pop, value);
        IncROG(8);
    }
}

static void IncROG(int32_t amount)
{
    RateOGMem[SMapX >> 3][SMapY >> 3] += (int16_t)(amount * 4);
}

static void DoResOut(int32_t pop, int32_t value)
{
    static const int16_t Brdr[9] = { 0, 3, 6, 1, 4, 7, 2, 5, 8 };
    int32_t z = 0;

    if (!pop) return;

    if (pop > 16) {
        ResPlop(((pop - 24) / 8), value);
        IncROG(-8);
        return;
    }

    if (pop == 16) {
        IncROG(-8);
        Map[SMapX][SMapY] = (uint16_t)(FREEZ | BLBNCNBIT | ZONEBIT);
        for (int32_t x = SMapX - 1; x <= SMapX + 1; x++) {
            for (int32_t y = SMapY - 1; y <= SMapY + 1; y++) {
                if (TestBounds(x, y)) {
                    if ((Map[x][y] & LOMASK) != FREEZ) {
                        Map[x][y] = (uint16_t)(LHTHR + value + Rand(2) + BLBNCNBIT);
                    }
                }
            }
        }
        return;
    }

    if (pop < 16) {
        IncROG(-1);
        for (int32_t x = SMapX - 1; x <= SMapX + 1; x++) {
            for (int32_t y = SMapY - 1; y <= SMapY + 1; y++) {
                if (TestBounds(x, y)) {
                    uint16_t loc = Map[x][y] & LOMASK;
                    if ((loc >= LHTHR) && (loc <= HHTHR)) {
                        Map[x][y] = (uint16_t)(Brdr[z] + BLBNCNBIT + FREEZ - 4);
                        return;
                    }
                }
                z++;
            }
        }
    }
}

static void DoComOut(int32_t pop, int32_t value)
{
    if (pop > 1) {
        ComPlop(pop - 2, value);
        IncROG(-8);
        return;
    }
    if (pop == 1) {
        ZonePlop(COMBASE);
        IncROG(-8);
    }
}

static void DoIndOut(int32_t pop, int32_t value)
{
    (void)value;
    if (pop > 1) {
        IndPlop(pop - 2, value);
        IncROG(-8);
        return;
    }
    if (pop == 1) {
        ZonePlop(INDCLR - 4);
        IncROG(-8);
    }
}

int16_t RZPop(int32_t ch9)
{
    int16_t cz_den = (int16_t)(((ch9 - RZB) / 9) % 4);
    return (int16_t)((cz_den * 8) + 16);
}

int16_t CZPop(int32_t ch9)
{
    if (ch9 == COMCLR) return 0;
    int16_t cz_den = (int16_t)((((ch9 - CZB) / 9) % 5) + 1);
    return cz_den;
}

int16_t IZPop(int32_t ch9)
{
    if (ch9 == INDCLR) return 0;
    int16_t cz_den = (int16_t)((((ch9 - IZB) / 9) % 4) + 1);
    return cz_den;
}

static void BuildHouse(int32_t value)
{
    int16_t score, hscore, best_loc;
    static const int16_t ZeX[9] = { 0, -1,  0,  1, -1, 1, -1, 0, 1 };
    static const int16_t ZeY[9] = { 0, -1, -1, -1,  0, 0,  1, 1, 1 };

    best_loc = 0;
    hscore = 0;

    for (int32_t z = 1; z < 9; z++) {
        int32_t xx = SMapX + ZeX[z];
        int32_t yy = SMapY + ZeY[z];
        if (TestBounds(xx, yy)) {
            score = EvalLot(xx, yy);
            if (score != 0) {
                if (score > hscore) {
                    hscore = score;
                    best_loc = (int16_t)z;
                }
                if ((score == hscore) && !(Rand16() & 7)) {
                    best_loc = (int16_t)z;
                }
            }
        }
    }

    if (best_loc) {
        int32_t xx = SMapX + ZeX[best_loc];
        int32_t yy = SMapY + ZeY[best_loc];
        if (TestBounds(xx, yy)) {
            Map[xx][yy] = (uint16_t)(HOUSE + BLBNCNBIT + Rand(2) + (value * 3));
        }
    }
}

static void ResPlop(int32_t den, int32_t value)
{
    int16_t base = (int16_t)((((value * 4) + den) * 9) + RZB - 4);
    ZonePlop(base);
}

static void ComPlop(int32_t den, int32_t value)
{
    int16_t base = (int16_t)((((value * 5) + den) * 9) + CZB - 4);
    ZonePlop(base);
}

static void IndPlop(int32_t den, int32_t value)
{
    int16_t base = (int16_t)((((value * 4) + den) * 9) + (IZB - 4));
    ZonePlop(base);
}

static int16_t EvalLot(int32_t x, int32_t y)
{
    static const int16_t DX[4] = { 0, 1, 0, -1 };
    static const int16_t DY[4] = { -1, 0, 1, 0 };

    uint16_t z = Map[x][y] & LOMASK;
    if (z && ((z < RESBASE) || (z > RESBASE + 8))) {
        return -1;
    }

    int16_t score = 1;
    for (int32_t i = 0; i < 4; i++) {
        int32_t xx = x + DX[i];
        int32_t yy = y + DY[i];
        if (TestBounds(xx, yy) && Map[xx][yy] &&
            ((Map[xx][yy] & LOMASK) <= LASTROAD)) {
            score++;
        }
    }
    return score;
}

void ZonePlop(int16_t base)
{
    static const int16_t Zx[9] = { -1,  0,  1, -1, 0, 1, -1, 0, 1 };
    static const int16_t Zy[9] = { -1, -1, -1,  0, 0, 0,  1, 1, 1 };

    for (int32_t z = 0; z < 9; z++) {
        int32_t xx = SMapX + Zx[z];
        int32_t yy = SMapY + Zy[z];
        if (TestBounds(xx, yy)) {
            uint16_t x = Map[xx][yy] & LOMASK;
            if ((x >= FLOOD) && (x < ROADBASE)) {
                return;
            }
        }
    }

    for (int32_t z = 0; z < 9; z++) {
        int32_t xx = SMapX + Zx[z];
        int32_t yy = SMapY + Zy[z];
        if (TestBounds(xx, yy)) {
            Map[xx][yy] = (uint16_t)(base + BNCNBIT);
        }
        base++;
    }

    CChr = (int16_t)Map[SMapX][SMapY];
    SetZPower();
    Map[SMapX][SMapY] |= (ZONEBIT | BULLBIT);
}

static int16_t EvalRes(int32_t traf)
{
    if (traf < 0) return -3000;

    int32_t value = LandValueMem[SMapX >> 1][SMapY >> 1];
    value -= PollutionMem[SMapX >> 1][SMapY >> 1];

    if (value < 0) {
        value = 0;
    } else {
        value <<= 5;
    }

    if (value > 6000) value = 6000;

    return (int16_t)(value - 3000);
}

static int16_t EvalCom(int32_t traf)
{
    if (traf < 0) return -3000;
    return ComRate[SMapX >> 3][SMapY >> 3];
}

static int16_t EvalInd(int32_t traf)
{
    if (traf < 0) return -1000;
    return 0;
}

int16_t DoFreePop(void)
{
    int16_t count = 0;

    for (int32_t x = SMapX - 1; x <= SMapX + 1; x++) {
        for (int32_t y = SMapY - 1; y <= SMapY + 1; y++) {
            if (TestBounds(x, y)) {
                uint16_t loc = Map[x][y] & LOMASK;
                if ((loc >= LHTHR) && (loc <= HHTHR)) {
                    count++;
                }
            }
        }
    }
    return count;
}