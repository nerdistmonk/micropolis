/* s_gen.c: Procedural terrain generator (rivers, lakes, coastlines, and forests).
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"

#define WATER_LOW   RIVER
#define WATER_HIGH  LASTRIVEDGE
#define WOODS_LOW   TREEBASE
#define WOODS_HIGH  UNUSED_TRASH2
#define RADIUS      18

/* Generation State & Parameters */
int16_t XStart = 0;
int16_t YStart = 0;
int16_t MapX = 0;
int16_t MapY = 0;
int16_t Dir = 0;
int16_t LastDir = 0;
int32_t TreeLevel = -1;    /* < 0: default random, 0: none, > 0: count */
int32_t LakeLevel = -1;    /* < 0: default random, 0: none, > 0: count */
int32_t CurveLevel = -1;   /* < 0: default random, 0: straight, > 0: curviness */
int32_t CreateIsland = -1; /* -1: 10% chance, 0: never, 1: always */

/* Forward Declarations of Internal Functions */
static int16_t ERand(int16_t limit);
static void    GetRandStart(void);
static void    MoveMap(int16_t dir);
static void    TreeSplash(int16_t xloc, int16_t yloc);
static void    DoTrees(void);
static void    SmoothRiver(void);
static bool    IsTree(int32_t cell);
static void    SmoothTrees(void);
static void    DoRivers(void);
static void    DoBRiv(void);
static void    DoSRiv(void);
static void    PutOnMap(int16_t mchar, int16_t xoff, int16_t yoff);
static void    BRivPlop(void);
static void    SRivPlop(void);
static void    MakeLakes(void);

void GenerateNewCity(void)
{
    GenerateSomeCity(Rand16());
}

void GenerateSomeCity(int32_t seed)
{
    if (CityFileName != NULL) {
        free(CityFileName);
        CityFileName = NULL;
    }

    GenerateMap(seed);

    CityTime      = 0;
    InitSimLoad   = 2;
    DoInitialEval = 0;

    Sim_InitEngine();

    if (EngineCallbacks.update_funds) {
        EngineCallbacks.update_funds(TotalFunds);
    }
    if (EngineCallbacks.invalidate_maps) {
        EngineCallbacks.invalidate_maps();
    }
    if (EngineCallbacks.invalidate_editors) {
        EngineCallbacks.invalidate_editors();
    }
}

static int16_t ERand(int16_t limit)
{
    int16_t z = Rand(limit);
    int16_t x = Rand(limit);
    return (z < x) ? z : x;
}

void GenerateMap(int32_t seed)
{
    SeedRand(seed);

    bool island = CreateIsland == 1;
    if (CreateIsland < 0 && Rand(100) < 10) { /* 10% chance for an island */
        island = true;
    }

    if (island) {
        Water_ClearPipes();
        MakeNakedIsland();
    } else {
        ClearMap();
    }

    GetRandStart();

    if (CurveLevel != 0) {
        DoRivers();
    }

    if (LakeLevel != 0) {
        MakeLakes();
    }

    SmoothRiver();

    if (TreeLevel != 0) {
        DoTrees();
    }
    RandomlySeedRand();
}

void ClearMap(void)
{
    Water_ClearPipes();
    for (int16_t x = 0; x < WORLD_X; x++) {
        for (int16_t y = 0; y < WORLD_Y; y++) {
            Map[x][y] = DIRT;
        }
    }
}

void ClearUnnatural(void)
{
    for (int16_t x = 0; x < WORLD_X; x++) {
        for (int16_t y = 0; y < WORLD_Y; y++) {
            if (Map[x][y] > WOODS) {
                Map[x][y] = DIRT;
            }
        }
    }
}

void MakeNakedIsland(void)
{
    for (int16_t x = 0; x < WORLD_X; x++) {
        for (int16_t y = 0; y < WORLD_Y; y++) {
            Map[x][y] = RIVER;
        }
    }

    for (int16_t x = 5; x < WORLD_X - 5; x++) {
        for (int16_t y = 5; y < WORLD_Y - 5; y++) {
            Map[x][y] = DIRT;
        }
    }

    for (int16_t x = 0; x < WORLD_X - 5; x = (int16_t)(x + 2)) {
        MapX = x;
        MapY = ERand(RADIUS);
        BRivPlop();
        MapY = (int16_t)((WORLD_Y - 10) - ERand(RADIUS));
        BRivPlop();
        MapY = 0;
        SRivPlop();
        MapY = (int16_t)(WORLD_Y - 6);
        SRivPlop();
    }

    for (int16_t y = 0; y < WORLD_Y - 5; y = (int16_t)(y + 2)) {
        MapY = y;
        MapX = ERand(RADIUS);
        BRivPlop();
        MapX = (int16_t)((WORLD_X - 10) - ERand(RADIUS));
        BRivPlop();
        MapX = 0;
        SRivPlop();
        MapX = (int16_t)(WORLD_X - 6);
        SRivPlop();
    }
}

static void MakeLakes(void)
{
    int16_t lim1;

    if (LakeLevel < 0) {
        lim1 = Rand(10);
    } else {
        lim1 = (int16_t)(LakeLevel / 2);
    }

    for (int16_t t = 0; t < lim1; t++) {
        int16_t x = (int16_t)(Rand(WORLD_X - 21) + 10);
        int16_t y = (int16_t)(Rand(WORLD_Y - 20) + 10);
        int16_t lim2 = (int16_t)(Rand(12) + 2);

        for (int16_t z = 0; z < lim2; z++) {
            MapX = (int16_t)(x - 6 + Rand(12));
            MapY = (int16_t)(y - 6 + Rand(12));
            if (Rand(3)) {
                SRivPlop();
            } else {
                BRivPlop();
            }
        }
    }
}

static void GetRandStart(void)
{
    XStart = (int16_t)(40 + Rand(WORLD_X - 80));
    YStart = (int16_t)(33 + Rand(WORLD_Y - 67));
    MapX = XStart;
    MapY = YStart;
}

static void MoveMap(int16_t dir)
{
    static const int16_t DirTab[2][8] = {
        {  0,  1, 1, 1, 0, -1, -1, -1 },
        { -1, -1, 0, 1, 1,  1,  0, -1 }
    };

    dir &= 7;
    MapX = (int16_t)(MapX + DirTab[0][dir]);
    MapY = (int16_t)(MapY + DirTab[1][dir]);
}

static void TreeSplash(int16_t xloc, int16_t yloc)
{
    int16_t dis;

    if (TreeLevel < 0) {
        dis = (int16_t)(Rand(150) + 50);
    } else {
        dis = (int16_t)(Rand(100 + (TreeLevel * 2)) + 50);
    }

    MapX = xloc;
    MapY = yloc;

    for (int16_t z = 0; z < dis; z++) {
        int16_t dir = Rand(7);
        MoveMap(dir);
        if (!TestBounds(MapX, MapY)) {
            return;
        }
        if ((Map[MapX][MapY] & LOMASK) == DIRT) {
            Map[MapX][MapY] = WOODS + BLBNBIT;
        }
    }
}

static void DoTrees(void)
{
    int16_t amount;

    if (TreeLevel < 0) {
        amount = (int16_t)(Rand(100) + 50);
    } else {
        amount = (int16_t)(TreeLevel + 3);
    }

    for (int16_t x = 0; x < amount; x++) {
        int16_t xloc = Rand(WORLD_X - 1);
        int16_t yloc = Rand(WORLD_Y - 1);
        TreeSplash(xloc, yloc);
    }

    SmoothTrees();
    SmoothTrees();
}

static void SmoothRiver(void)
{
    static const int16_t DX[4] = { -1, 0, 1,  0 };
    static const int16_t DY[4] = {  0, 1, 0, -1 };
    static const uint16_t REdTab[16] = {
        13 + BULLBIT, 13 + BULLBIT, 17 + BULLBIT, 15 + BULLBIT,
         5 + BULLBIT,            2, 19 + BULLBIT, 17 + BULLBIT,
         9 + BULLBIT, 11 + BULLBIT,            2, 13 + BULLBIT,
         7 + BULLBIT,  9 + BULLBIT,  5 + BULLBIT,            2
    };

    for (int16_t mx = 0; mx < WORLD_X; mx++) {
        for (int16_t my = 0; my < WORLD_Y; my++) {
            if (Map[mx][my] == REDGE) {
                int16_t bitindex = 0;
                for (int32_t z = 0; z < 4; z++) {
                    bitindex <<= 1;
                    int16_t xtem = (int16_t)(mx + DX[z]);
                    int16_t ytem = (int16_t)(my + DY[z]);
                    if (TestBounds(xtem, ytem)) {
                        uint16_t val = Map[xtem][ytem] & LOMASK;
                        if ((val != DIRT) && ((val < WOODS_LOW) || (val > WOODS_HIGH))) {
                            bitindex++;
                        }
                    }
                }

                uint16_t temp = REdTab[bitindex & 15];
                if ((temp != RIVER) && Rand(1)) {
                    temp++;
                }
                Map[mx][my] = temp;
            }
        }
    }
}

static bool IsTree(int32_t cell)
{
    int32_t val = cell & LOMASK;
    return (val >= WOODS_LOW) && (val <= WOODS_HIGH);
}

static void SmoothTrees(void)
{
    static const int16_t DX[4] = { -1, 0, 1,  0 };
    static const int16_t DY[4] = {  0, 1, 0, -1 };
    static const uint16_t TEdTab[16] = {
         0,  0,  0, 34,
         0,  0, 36, 35,
         0, 32,  0, 33,
        30, 31, 29, 37
    };

    for (int16_t mx = 0; mx < WORLD_X; mx++) {
        for (int16_t my = 0; my < WORLD_Y; my++) {
            if (IsTree(Map[mx][my])) {
                int16_t bitindex = 0;
                for (int32_t z = 0; z < 4; z++) {
                    bitindex <<= 1;
                    int16_t xtem = (int16_t)(mx + DX[z]);
                    int16_t ytem = (int16_t)(my + DY[z]);
                    if (TestBounds(xtem, ytem) && IsTree(Map[xtem][ytem])) {
                        bitindex++;
                    }
                }

                uint16_t temp = TEdTab[bitindex & 15];
                if (temp) {
                    if ((temp != WOODS) && ((mx + my) & 1)) {
                        temp = (uint16_t)(temp - 8);
                    }
                    Map[mx][my] = (uint16_t)(temp + BLBNBIT);
                } else {
                    Map[mx][my] = temp;
                }
            }
        }
    }
}

static void DoRivers(void)
{
    LastDir = Rand(3);
    Dir = LastDir;
    DoBRiv();

    MapX = XStart;
    MapY = YStart;
    LastDir = (int16_t)(LastDir ^ 4);
    Dir = LastDir;
    DoBRiv();

    MapX = XStart;
    MapY = YStart;
    LastDir = Rand(3);
    DoSRiv();
}

static void DoBRiv(void)
{
    int16_t r1 = (CurveLevel < 0) ? 100 : (int16_t)(CurveLevel + 10);
    int16_t r2 = (CurveLevel < 0) ? 200 : (int16_t)(CurveLevel + 100);

    while (TestBounds(MapX + 4, MapY + 4)) {
        BRivPlop();
        if (Rand(r1) < 10) {
            Dir = LastDir;
        } else {
            if (Rand(r2) > 90) Dir++;
            if (Rand(r2) > 90) Dir--;
        }
        MoveMap(Dir);
    }
}

static void DoSRiv(void)
{
    int16_t r1 = (CurveLevel < 0) ? 100 : (int16_t)(CurveLevel + 10);
    int16_t r2 = (CurveLevel < 0) ? 200 : (int16_t)(CurveLevel + 100);

    while (TestBounds(MapX + 3, MapY + 3)) {
        SRivPlop();
        if (Rand(r1) < 10) {
            Dir = LastDir;
        } else {
            if (Rand(r2) > 90) Dir++;
            if (Rand(r2) > 90) Dir--;
        }
        MoveMap(Dir);
    }
}

static void PutOnMap(int16_t mchar, int16_t xoff, int16_t yoff)
{
    if (mchar == 0) return;

    int16_t xloc = (int16_t)(MapX + xoff);
    int16_t yloc = (int16_t)(MapY + yoff);

    if (!TestBounds(xloc, yloc)) return;

    uint16_t temp = Map[xloc][yloc];
    if (temp) {
        temp &= LOMASK;
        if (temp == RIVER && mchar != CHANNEL) return;
        if (temp == CHANNEL) return;
    }

    Map[xloc][yloc] = mchar;
}

static void BRivPlop(void)
{
    static const int16_t BRMatrix[9][9] = {
        { 0, 0, 0, 3, 3, 3, 0, 0, 0 },
        { 0, 0, 3, 2, 2, 2, 3, 0, 0 },
        { 0, 3, 2, 2, 2, 2, 2, 3, 0 },
        { 3, 2, 2, 2, 2, 2, 2, 2, 3 },
        { 3, 2, 2, 2, 4, 2, 2, 2, 3 },
        { 3, 2, 2, 2, 2, 2, 2, 2, 3 },
        { 0, 3, 2, 2, 2, 2, 2, 3, 0 },
        { 0, 0, 3, 2, 2, 2, 3, 0, 0 },
        { 0, 0, 0, 3, 3, 3, 0, 0, 0 }
    };

    for (int16_t x = 0; x < 9; x++) {
        for (int16_t y = 0; y < 9; y++) {
            PutOnMap(BRMatrix[y][x], x, y);
        }
    }
}

static void SRivPlop(void)
{
    static const int16_t SRMatrix[6][6] = {
        { 0, 0, 3, 3, 0, 0 },
        { 0, 3, 2, 2, 3, 0 },
        { 3, 2, 2, 2, 2, 3 },
        { 3, 2, 2, 2, 2, 3 },
        { 0, 3, 2, 2, 3, 0 },
        { 0, 0, 3, 3, 0, 0 }
    };

    for (int16_t x = 0; x < 6; x++) {
        for (int16_t y = 0; y < 6; y++) {
            PutOnMap(SRMatrix[y][x], x, y);
        }
    }
}

void SmoothWater(void)
{
    for (int16_t x = 0; x < WORLD_X; x++) {
        for (int16_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y] & LOMASK;
            if ((tile >= WATER_LOW) && (tile <= WATER_HIGH)) {
                if (x > 0) {
                    uint16_t neighbor = Map[x - 1][y] & LOMASK;
                    if ((neighbor < WATER_LOW) || (neighbor > WATER_HIGH)) goto set_edge;
                }
                if (x < (WORLD_X - 1)) {
                    uint16_t neighbor = Map[x + 1][y] & LOMASK;
                    if ((neighbor < WATER_LOW) || (neighbor > WATER_HIGH)) goto set_edge;
                }
                if (y > 0) {
                    uint16_t neighbor = Map[x][y - 1] & LOMASK;
                    if ((neighbor < WATER_LOW) || (neighbor > WATER_HIGH)) goto set_edge;
                }
                if (y < (WORLD_Y - 1)) {
                    uint16_t neighbor = Map[x][y + 1] & LOMASK;
                    if ((neighbor < WATER_LOW) || (neighbor > WATER_HIGH)) goto set_edge;
                }
                continue;

set_edge:
                Map[x][y] = REDGE;
            }
        }
    }

    for (int16_t x = 0; x < WORLD_X; x++) {
        for (int16_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y] & LOMASK;
            if ((tile != CHANNEL) && (tile >= WATER_LOW) && (tile <= WATER_HIGH)) {
                if (x > 0) {
                    uint16_t neighbor = Map[x - 1][y] & LOMASK;
                    if ((neighbor < WATER_LOW) || (neighbor > WATER_HIGH)) continue;
                }
                if (x < (WORLD_X - 1)) {
                    uint16_t neighbor = Map[x + 1][y] & LOMASK;
                    if ((neighbor < WATER_LOW) || (neighbor > WATER_HIGH)) continue;
                }
                if (y > 0) {
                    uint16_t neighbor = Map[x][y - 1] & LOMASK;
                    if ((neighbor < WATER_LOW) || (neighbor > WATER_HIGH)) continue;
                }
                if (y < (WORLD_Y - 1)) {
                    uint16_t neighbor = Map[x][y + 1] & LOMASK;
                    if ((neighbor < WATER_LOW) || (neighbor > WATER_HIGH)) continue;
                }
                Map[x][y] = RIVER;
            }
        }
    }

    for (int16_t x = 0; x < WORLD_X; x++) {
        for (int16_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y] & LOMASK;
            if ((tile >= WOODS_LOW) && (tile <= WOODS_HIGH)) {
                if (x > 0 && ((Map[x - 1][y] == RIVER) || (Map[x - 1][y] == CHANNEL))) {
                    Map[x][y] = REDGE;
                    continue;
                }
                if ((x < (WORLD_X - 1)) && ((Map[x + 1][y] == RIVER) || (Map[x + 1][y] == CHANNEL))) {
                    Map[x][y] = REDGE;
                    continue;
                }
                if (y > 0 && ((Map[x][y - 1] == RIVER) || (Map[x][y - 1] == CHANNEL))) {
                    Map[x][y] = REDGE;
                    continue;
                }
                if ((y < (WORLD_Y - 1)) && ((Map[x][y + 1] == RIVER) || (Map[x][y + 1] == CHANNEL))) {
                    Map[x][y] = REDGE;
                    continue;
                }
            }
        }
    }
}