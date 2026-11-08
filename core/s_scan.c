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
static void    DistIntMarket(void);
static void    SmoothTerrain(void);

/* External Functions Implemented Across Engine Modules */
extern int16_t RZPop(int32_t ch9);
extern int16_t CZPop(int32_t ch9);
extern int16_t IZPop(int32_t ch9);
extern int16_t DoFreePop(void);

void FireAnalysis(void)
{
    FireCoverageAnalysis();

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

static LandValueFactors lvFactorsMem[HWLDX][HWLDY];

static bool zone_footprint[WORLD_X][WORLD_Y];
static bool zone_access[WORLD_X][WORLD_Y];

/* Rail crossings count as roads; plain rail and power lines do not. */
static bool IsAccessRoad(uint16_t loc)
{
    return (loc >= ROADBASE && loc <= LASTROAD) ||
           loc == HRAILROAD || loc == VRAILROAD || loc == ROADVPOWERH;
}

/* Returns the footprint of the placeable building anchored at (x, y), if one starts there. */
static bool BuildingAnchorAt(int32_t x, int32_t y, int32_t *ox, int32_t *oy, int32_t *size)
{
    uint16_t raw = Map[x][y];
    uint16_t low = raw & LOMASK;

    if (raw & ZONEBIT) {
        int32_t s = 3;
        if ((low >= PORTBASE && low <= LASTPORT) ||
            (low >= COALBASE && low <= LASTPOWERPLANT) ||
            (low >= STADIUMBASE && low <= FULLSTADIUM) ||
            (low >= NUCLEARBASE && low <= NUCLEAR)) {
            s = 4;
        } else if (low >= AIRPORTBASE && low <= AIRPORT) {
            s = 6;
        }
        *ox = x - 1;
        *oy = y - 1;
        *size = s;
        return true;
    }
    if (low == WATERTOWER) {
        *ox = x;
        *oy = y;
        *size = 3;
        return true;
    }
    if (low >= FOUNTAIN && low < FOUNTAIN + FOUNTAIN_FRAMES) {
        *ox = x;
        *oy = y;
        *size = 1;
        return true;
    }
    return false;
}

static bool BuildingHasRoadAccess(int32_t ox, int32_t oy, int32_t size)
{
    static const int32_t dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int32_t x = ox; x < ox + size; x++) {
        for (int32_t y = oy; y < oy + size; y++) {
            for (int32_t d = 0; d < 4; d++) {
                int32_t nx = x + dirs[d][0];
                int32_t ny = y + dirs[d][1];
                if (nx >= ox && nx < ox + size && ny >= oy && ny < oy + size) continue;
                if (TestBounds(nx, ny) && IsAccessRoad(Map[nx][ny] & LOMASK)) return true;
            }
        }
    }
    return false;
}

/* Marks every placeable building's tiles, and whether a road touches its edge. */
static void ZoneScanAccess(void)
{
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            zone_footprint[x][y] = false;
            zone_access[x][y] = false;
        }
    }

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (!BuildingAnchorAt(x, y, &ox, &oy, &size)) continue;

            bool access = BuildingHasRoadAccess(ox, oy, size);
            for (int32_t tx = ox; tx < ox + size; tx++) {
                for (int32_t ty = oy; ty < oy + size; ty++) {
                    if (!TestBounds(tx, ty)) continue;
                    zone_footprint[tx][ty] = true;
                    zone_access[tx][ty] = access;
                }
            }
        }
    }
}

bool ZoneHasRoadAccess(int32_t x, int32_t y)
{
    return TestBounds(x, y) && zone_access[x][y];
}

/* Land value at half-resolution cell (x, y), split into the factors that make it up. */
static void LandValueFactorsAt(int32_t x, int32_t y, LandValueFactors *f)
{
    int32_t zx = x << 1;
    int32_t zy = y << 1;

    f->zoned = false;
    f->access = false;
    bool industrial = false;
    for (int32_t mx = zx; mx <= zx + 1; mx++) {
        for (int32_t my = zy; my <= zy + 1; my++) {
            uint16_t loc = Map[mx][my] & LOMASK;
            if (zone_footprint[mx][my]) {
                f->zoned = true;
            }
            if (zone_access[mx][my]) {
                f->access = true;
            }
            if (loc >= INDBASE && loc < PORTBASE) {
                industrial = true;
            }
        }
    }

    f->centre = (34 - GetDisCC(x, y)) * 4;
    f->terrain = TerrainMem[x >> 1][y >> 1];
    /* Industrial cells ignore their own pollution; neighbors still lose value to it. */
    f->pollution = industrial ? 0 : -(int32_t)PollutionMem[x][y];
    f->crime = (CrimeMem[x][y] > 190) ? -20 : 0;

    int32_t value = f->centre + f->terrain + f->pollution + f->crime;
    if (value > 250) value = 250;
    if (value < 1)   value = 1;
    f->value = value;
}

void LandValueFactorsOf(int32_t x, int32_t y, LandValueFactors *f)
{
    *f = lvFactorsMem[x][y];
}

/* Citywide neighbor-link bonus applied on top of a tile's base land value. */
int32_t LandValueWithNeighbors(int32_t value)
{
    int32_t v = value * (100 + Neighbor_LandValueBonusPercent()) / 100;
    return v > 250 ? 250 : v;
}

void PTLScan(void)
{
    int32_t lvtot = 0;
    int32_t lvnum = 0;
    int32_t ptot  = 0;
    int32_t pnum  = 0;
    int32_t pmax  = 0;
    int32_t trashPenalty = LandfillTrashPenalty();

    ZoneScanAccess();

    for (int16_t x = 0; x < QWX; x++) {
        for (int16_t y = 0; y < QWY; y++) {
            Qtem[x][y] = 0;
        }
    }

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            int32_t plevel = 0;
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
                        if (loc >= RESBASE && loc <= LASTZONE) {
                            plevel += trashPenalty;
                        }
                    }
                }
            }

            if (plevel > 255) plevel = 255;
            tem[x][y] = (Byte)plevel;

            LandValueFactorsAt(x, y, &lvFactorsMem[x][y]);
            const LandValueFactors *lvf = &lvFactorsMem[x][y];
            if (lvf->access) {
                int32_t lv = LandValueWithNeighbors(lvf->value);
                LandValueMem[x][y] = (Byte)lv;
                lvtot += lv;
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
    return LandfillPollution(loc);
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

    PoliceAnalysis();

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            uint8_t lv = LandValueMem[x][y];
            if (lvFactorsMem[x][y].zoned) {
                numz++;
                int32_t z = 128 - lv;
                z += PopDensity[x][y];
                if (z > 300) z = 300;
                if (PoliceCoversCrimeCell(x, y)) z = 0;
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

void SmoothServiceMap(int16_t map[SmX][SmY])
{
    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            int32_t edge = 0;
            if (x > 0)          edge += map[x - 1][y];
            if (x < (SmX - 1))  edge += map[x + 1][y];
            if (y > 0)          edge += map[x][y - 1];
            if (y < (SmY - 1))  edge += map[x][y + 1];
            edge = (edge >> 2) + map[x][y];
            STem[x][y] = (int16_t)(edge >> 1);
        }
    }
    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            map[x][y] = STem[x][y];
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