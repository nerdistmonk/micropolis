/* s_scan.c: Map analysis scans (population density, land value, pollution, crime, fire rate).
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"

/* Global Map Update Flags & City Geometry */
int16_t  NewMap = 0;
int16_t  NewMapFlags[NMAPS];
int16_t  CCx = 0;
int16_t  CCy = 0;
int16_t  CCx2 = 0;
int16_t  CCy2 = 0;
int16_t  PolMaxX = 0;
int16_t  PolMaxY = 0;
int16_t  CrimeMaxX = 0;
int16_t  CrimeMaxY = 0;
QUAD     DonDither = 0;

/* Forward Declarations of Internal Functions */
static int16_t GetPDen(int32_t ch9);
static int32_t GetPValue(int32_t loc);
static int16_t GetDisCC(int32_t x, int32_t y);
static void    DoSmooth(void);
static void    DoSmooth2(void);
static void    ClrTemArray(void);
static void    SmoothFSMap(void);
static void    SmoothPSMap(void);
static void    DistIntMarket(void);
static void    SmoothTerrain(void);

/* External Functions Implemented Across Engine Modules */
extern int16_t RZPop(int32_t ch9);
extern int16_t CZPop(int32_t ch9);
extern int16_t IZPop(int32_t ch9);
extern int16_t DoFreePop(void);

void FireAnalysis(void)
{
    SmoothFSMap();
    SmoothFSMap();
    SmoothFSMap();

    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            FireRate[x][y] = FireStMap[x][y];
        }
    }

    NewMapFlags[DYMAP] = 1;
    NewMapFlags[FIMAP] = 1;
}

void PopDenScan(void)
{
    int32_t xtot = 0;
    int32_t ytot = 0;
    int32_t ztot = 0;

    ClrTemArray();

    for (int16_t x = 0; x < WORLD_X; x++) {
        for (int16_t y = 0; y < WORLD_Y; y++) {
            uint16_t z = Map[x][y];
            if (z & ZONEBIT) {
                z &= LOMASK;
                SMapX = x;
                SMapY = y;
                int16_t pden = (int16_t)(GetPDen((int32_t)z) << 3);
                if (pden > 254) {
                    pden = 254;
                }
                tem[x >> 1][y >> 1] = (Byte)pden;
                xtot += x;
                ytot += y;
                ztot++;
            }
        }
    }

    DoSmooth();   /* tem -> tem2 */
    DoSmooth2();  /* tem2 -> tem */
    DoSmooth();   /* tem -> tem2 */

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            PopDensity[x][y] = (Byte)(tem2[x][y] << 1);
        }
    }

    DistIntMarket();

    if (ztot) {
        CCx = (int16_t)(xtot / ztot);
        CCy = (int16_t)(ytot / ztot);
    } else {
        CCx = HWLDX;
        CCy = HWLDY;
    }
    CCx2 = (int16_t)(CCx >> 1);
    CCy2 = (int16_t)(CCy >> 1);

    NewMapFlags[DYMAP] = 1;
    NewMapFlags[PDMAP] = 1;
    NewMapFlags[RGMAP] = 1;
}

static int16_t GetPDen(int32_t ch9)
{
    if (ch9 == FREEZ) {
        return DoFreePop();
    }
    if (ch9 < COMBASE) {
        return RZPop(ch9);
    }
    if (ch9 < INDBASE) {
        return (int16_t)(CZPop(ch9) << 3);
    }
    if (ch9 < PORTBASE) {
        return (int16_t)(IZPop(ch9) << 3);
    }
    return 0;
}

void PTLScan(void)
{
    int32_t lvtot = 0;
    int32_t lvnum = 0;
    int32_t ptot  = 0;
    int32_t pnum  = 0;
    int32_t pmax  = 0;

    for (int16_t x = 0; x < QWX; x++) {
        for (int16_t y = 0; y < QWY; y++) {
            Qtem[x][y] = 0;
        }
    }

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            int32_t plevel = 0;
            int32_t lvflag = 0;
            int16_t zx = (int16_t)(x << 1);
            int16_t zy = (int16_t)(y << 1);

            for (int16_t mx = zx; mx <= zx + 1; mx++) {
                for (int16_t my = zy; my <= zy + 1; my++) {
                    uint16_t loc = Map[mx][my] & LOMASK;
                    if (loc) {
                        if (loc < RUBBLE) {
                            Qtem[x >> 1][y >> 1] = (Byte)(Qtem[x >> 1][y >> 1] + 15);
                            continue;
                        }
                        plevel += GetPValue(loc);
                        if (loc >= ROADBASE) {
                            lvflag++;
                        }
                    }
                }
            }

            if (plevel > 255) plevel = 255;
            tem[x][y] = (Byte)plevel;

            if (lvflag) {
                int32_t dis = 34 - GetDisCC(x, y);
                dis <<= 2;
                dis += TerrainMem[x >> 1][y >> 1];
                dis -= PollutionMem[x][y];
                if (CrimeMem[x][y] > 190) dis -= 20;
                if (dis > 250) dis = 250;
                if (dis < 1)   dis = 1;

                LandValueMem[x][y] = (Byte)dis;
                lvtot += dis;
                lvnum++;
            } else {
                LandValueMem[x][y] = 0;
            }
        }
    }

    if (lvnum) {
        LVAverage = (int16_t)(lvtot / lvnum);
    } else {
        LVAverage = 0;
    }

    DoSmooth();
    DoSmooth2();

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            uint8_t z = tem[x][y];
            PollutionMem[x][y] = z;
            if (z) {
                pnum++;
                ptot += z;
                if ((z > pmax) || ((z == pmax) && !(Rand16() & 3))) {
                    pmax = z;
                    PolMaxX = (int16_t)(x << 1);
                    PolMaxY = (int16_t)(y << 1);
                }
            }
        }
    }

    if (pnum) {
        PolluteAverage = (int16_t)(ptot / pnum);
    } else {
        PolluteAverage = 0;
    }

    SmoothTerrain();

    NewMapFlags[DYMAP] = 1;
    NewMapFlags[PLMAP] = 1;
    NewMapFlags[LVMAP] = 1;
}

static int32_t GetPValue(int32_t loc)
{
    if (loc < POWERBASE) {
        if (loc >= HTRFBASE) return 75;
        if (loc >= LTRFBASE) return 50;
        if (loc < ROADBASE) {
            if (loc > FIREBASE) return 90;
            if (loc >= RADTILE) return 255;
        }
        return 0;
    }
    if (loc <= LASTIND) return 0;
    if (loc < PORTBASE) return 50;
    if (loc <= LASTPOWERPLANT) return 100;
    return 0;
}

static int16_t GetDisCC(int32_t x, int32_t y)
{
    int16_t xdis = (int16_t)((x > CCx2) ? (x - CCx2) : (CCx2 - x));
    int16_t ydis = (int16_t)((y > CCy2) ? (y - CCy2) : (CCy2 - y));
    int16_t z = (int16_t)(xdis + ydis);

    return (z > 32) ? 32 : z;
}

void CrimeScan(void)
{
    int32_t totz = 0;
    int16_t numz = 0;
    int16_t cmax = 0;

    SmoothPSMap();
    SmoothPSMap();
    SmoothPSMap();

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            uint8_t lv = LandValueMem[x][y];
            if (lv) {
                numz++;
                int32_t z = 128 - lv;
                z += PopDensity[x][y];
                if (z > 300) z = 300;
                z -= PoliceMap[x >> 2][y >> 2];
                if (z > 250) z = 250;
                if (z < 0)   z = 0;

                CrimeMem[x][y] = (Byte)z;
                totz += z;

                if ((z > cmax) || ((z == cmax) && !(Rand16() & 3))) {
                    cmax = (int16_t)z;
                    CrimeMaxX = (int16_t)(x << 1);
                    CrimeMaxY = (int16_t)(y << 1);
                }
            } else {
                CrimeMem[x][y] = 0;
            }
        }
    }

    if (numz) {
        CrimeAverage = (int16_t)(totz / numz);
    } else {
        CrimeAverage = 0;
    }

    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            PoliceMapEffect[x][y] = PoliceMap[x][y];
        }
    }

    NewMapFlags[DYMAP] = 1;
    NewMapFlags[CRMAP] = 1;
    NewMapFlags[POMAP] = 1;
}

static void SmoothTerrain(void)
{
    if (DonDither & 1) {
        int32_t y = 0;
        int32_t z = 0;
        int32_t dir = 1;

        for (int32_t x = 0; x < QWX; x++) {
            for (; y != QWY && y != -1; y += dir) {
                z += Qtem[(x == 0) ? x : (x - 1)][y] +
                     Qtem[(x == (QWX - 1)) ? x : (x + 1)][y] +
                     Qtem[x][(y == 0) ? 0 : (y - 1)] +
                     Qtem[x][(y == (QWY - 1)) ? y : (y + 1)] +
                     (Qtem[x][y] << 2);

                TerrainMem[x][y] = (Byte)(((uint32_t)z) >> 3);
                z &= 0x7;
            }
            dir = -dir;
            y += dir;
        }
    } else {
        for (int16_t x = 0; x < QWX; x++) {
            for (int16_t y = 0; y < QWY; y++) {
                int32_t z = 0;
                if (x > 0)          z += Qtem[x - 1][y];
                if (x < (QWX - 1))  z += Qtem[x + 1][y];
                if (y > 0)          z += Qtem[x][y - 1];
                if (y < (QWY - 1))  z += Qtem[x][y + 1];
                TerrainMem[x][y] = (Byte)(((z >> 2) + Qtem[x][y]) & 0xFF) >> 1;
            }
        }
    }
}

static void DoSmooth(void)
{
    if (DonDither & 2) {
        int32_t y = 0;
        int32_t z = 0;
        int32_t dir = 1;

        for (int32_t x = 0; x < HWLDX; x++) {
            for (; y != HWLDY && y != -1; y += dir) {
                z += tem[(x == 0) ? x : (x - 1)][y] +
                     tem[(x == (HWLDX - 1)) ? x : (x + 1)][y] +
                     tem[x][(y == 0) ? 0 : (y - 1)] +
                     tem[x][(y == (HWLDY - 1)) ? y : (y + 1)] +
                     tem[x][y];
                tem2[x][y] = (Byte)(((uint32_t)z) >> 2);
                z &= 3;
            }
            dir = -dir;
            y += dir;
        }
    } else {
        for (int16_t x = 0; x < HWLDX; x++) {
            for (int16_t y = 0; y < HWLDY; y++) {
                int32_t z = 0;
                if (x > 0)          z += tem[x - 1][y];
                if (x < (HWLDX - 1))z += tem[x + 1][y];
                if (y > 0)          z += tem[x][y - 1];
                if (y < (HWLDY - 1))z += tem[x][y + 1];
                z = (z + tem[x][y]) >> 2;
                if (z > 255) z = 255;
                tem2[x][y] = (Byte)z;
            }
        }
    }
}

static void DoSmooth2(void)
{
    if (DonDither & 4) {
        int32_t y = 0;
        int32_t z = 0;
        int32_t dir = 1;

        for (int32_t x = 0; x < HWLDX; x++) {
            for (; y != HWLDY && y != -1; y += dir) {
                z += tem2[(x == 0) ? x : (x - 1)][y] +
                     tem2[(x == (HWLDX - 1)) ? x : (x + 1)][y] +
                     tem2[x][(y == 0) ? 0 : (y - 1)] +
                     tem2[x][(y == (HWLDY - 1)) ? y : (y + 1)] +
                     tem2[x][y];
                tem[x][y] = (Byte)((Byte)z >> 2);
                z &= 3;
            }
            dir = -dir;
            y += dir;
        }
    } else {
        for (int16_t x = 0; x < HWLDX; x++) {
            for (int16_t y = 0; y < HWLDY; y++) {
                int32_t z = 0;
                if (x > 0)          z += tem2[x - 1][y];
                if (x < (HWLDX - 1))z += tem2[x + 1][y];
                if (y > 0)          z += tem2[x][y - 1];
                if (y < (HWLDY - 1))z += tem2[x][y + 1];
                z = (z + tem2[x][y]) >> 2;
                if (z > 255) z = 255;
                tem[x][y] = (Byte)z;
            }
        }
    }
}

static void ClrTemArray(void)
{
    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            tem[x][y] = 0;
        }
    }
}

static void SmoothFSMap(void)
{
    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            int32_t edge = 0;
            if (x > 0)          edge += FireStMap[x - 1][y];
            if (x < (SmX - 1))  edge += FireStMap[x + 1][y];
            if (y > 0)          edge += FireStMap[x][y - 1];
            if (y < (SmY - 1))  edge += FireStMap[x][y + 1];
            edge = (edge >> 2) + FireStMap[x][y];
            STem[x][y] = (int16_t)(edge >> 1);
        }
    }
    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            FireStMap[x][y] = STem[x][y];
        }
    }
}

static void SmoothPSMap(void)
{
    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            int32_t edge = 0;
            if (x > 0)          edge += PoliceMap[x - 1][y];
            if (x < (SmX - 1))  edge += PoliceMap[x + 1][y];
            if (y > 0)          edge += PoliceMap[x][y - 1];
            if (y < (SmY - 1))  edge += PoliceMap[x][y + 1];
            edge = (edge >> 2) + PoliceMap[x][y];
            STem[x][y] = (int16_t)(edge >> 1);
        }
    }
    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            PoliceMap[x][y] = STem[x][y];
        }
    }
}

static void DistIntMarket(void)
{
    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            int16_t z = GetDisCC(x << 2, y << 2);
            z <<= 2;
            z = (int16_t)(64 - z);
            ComRate[x][y] = z;
        }
    }
}