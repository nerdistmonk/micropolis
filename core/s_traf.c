/* s_traf.c: Traffic generation, trip pathfinding, and transit density mapping.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"

#define MAXDIS 30

/* Coordinate Stack for Pathfinding Steps */
int16_t PosStackN = 0;
int16_t SMapXStack[MAXDIS + 1];
int16_t SMapYStack[MAXDIS + 1];
int16_t LDir = 0;
int16_t Zsource = 0;
int16_t TrafMaxX = 0;
int16_t TrafMaxY = 0;

/* Forward Declarations of Internal Functions */
static void    PushPos(void);
static void    PullPos(void);
static bool    TryDrive(void);
static bool    TryGo(int32_t step);
static int16_t GetFromMap(int32_t dir);
static bool    DriveDone(void);
bool RoadTest(int32_t tile);

/* External sprite accessor (stubbed or provided by sprite subsystem) */
extern SimSprite *GetSprite(int32_t type);

int32_t MakeTraf(int32_t zt)
{
    int16_t xtem = SMapX;
    int16_t ytem = SMapY;

    Zsource = (int16_t)zt;
    PosStackN = 0;

    if (FindPRoad()) { /* Look for road on zone perimeter */
        if (TryDrive()) { /* Attempt to drive toward destination zone */
            SetTrafMem(); /* If successful, increment traffic density */
            SMapX = xtem;
            SMapY = ytem;
            return TRUE;  /* Trip succeeded */
        }
        SMapX = xtem;
        SMapY = ytem;
        return FALSE;     /* Trip failed */
    }

    return -1;            /* No road found adjacent to zone */
}

void SetTrafMem(void)
{
    for (int16_t x = PosStackN; x > 0; x--) {
        PullPos();
        if (TestBounds(SMapX, SMapY)) {
            uint16_t z = Map[SMapX][SMapY] & LOMASK;
            if ((z >= ROADBASE) && (z < POWERBASE)) {
                int32_t density = TrfDensity[SMapX >> 1][SMapY >> 1];
                density += 50;

                if ((density > 240) && !Rand(5)) {
                    density = 240;
                    TrafMaxX = (int16_t)(SMapX << 4);
                    TrafMaxY = (int16_t)(SMapY << 4);

                    SimSprite *sprite = GetSprite(COP);
                    if (sprite && (sprite->control == -1)) {
                        sprite->dest_x = TrafMaxX;
                        sprite->dest_y = TrafMaxY;
                    }
                }

                if (density > 255) density = 255;
                TrfDensity[SMapX >> 1][SMapY >> 1] = (Byte)density;
            }
        }
    }
}

static void PushPos(void)
{
    if (PosStackN < MAXDIS) {
        PosStackN++;
        SMapXStack[PosStackN] = SMapX;
        SMapYStack[PosStackN] = SMapY;
    }
}

static void PullPos(void)
{
    if (PosStackN > 0) {
        SMapX = SMapXStack[PosStackN];
        SMapY = SMapYStack[PosStackN];
        PosStackN--;
    }
}

int32_t FindPRoad(void)
{
    static const int16_t PerimX[12] = { -1,  0,  1,  2, 2, 2, 1, 0, -1, -2, -2, -2 };
    static const int16_t PerimY[12] = { -2, -2, -2, -1, 0, 1, 2, 2,  2,  1,  0, -1 };

    for (int32_t z = 0; z < 12; z++) {
        int16_t tx = (int16_t)(SMapX + PerimX[z]);
        int16_t ty = (int16_t)(SMapY + PerimY[z]);
        if (TestBounds(tx, ty)) {
            if (RoadTest(Map[tx][ty])) {
                SMapX = tx;
                SMapY = ty;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static bool TryDrive(void)
{
    LDir = 5;
    for (int16_t z = 0; z < MAXDIS; z++) {
        if (TryGo(z)) {
            if (DriveDone()) {
                return true;
            }
        } else {
            if (PosStackN > 0) {
                PosStackN--;
                z = (int16_t)(z + 3);
            } else {
                return false;
            }
        }
    }
    return false;
}

static bool TryGo(int32_t step)
{
    int16_t rdir = (int16_t)(Rand16() & 3);

    for (int16_t x = rdir; x < (rdir + 4); x++) {
        int16_t realdir = (int16_t)(x & 3);
        if (realdir == LDir) {
            continue;
        }

        if (RoadTest(GetFromMap(realdir))) {
            MoveMapSim(realdir);
            LDir = (int16_t)((realdir + 2) & 3);
            if (step & 1) {
                PushPos();
            }
            return true;
        }
    }
    return false;
}

static int16_t GetFromMap(int32_t dir)
{
    switch (dir) {
    case 0: /* North */
        if (SMapY > 0) {
            return (int16_t)(Map[SMapX][SMapY - 1] & LOMASK);
        }
        return 0;

    case 1: /* East */
        if (SMapX < (WORLD_X - 1)) {
            return (int16_t)(Map[SMapX + 1][SMapY] & LOMASK);
        }
        return 0;

    case 2: /* South */
        if (SMapY < (WORLD_Y - 1)) {
            return (int16_t)(Map[SMapX][SMapY + 1] & LOMASK);
        }
        return 0;

    case 3: /* West */
        if (SMapX > 0) {
            return (int16_t)(Map[SMapX - 1][SMapY] & LOMASK);
        }
        return 0;

    default:
        break;
    }
    return 0;
}

static bool DriveDone(void)
{
    static const uint16_t TARGL[3] = { COMBASE, LHTHR, LHTHR };
    static const uint16_t TARGH[3] = { NUCLEAR, PORT,  COMBASE };

    uint16_t l = TARGL[Zsource];
    uint16_t h = TARGH[Zsource];
    uint16_t z;

    if (SMapY > 0) {
        z = Map[SMapX][SMapY - 1] & LOMASK;
        if ((z >= l) && (z <= h)) return true;
    }

    if (SMapX < (WORLD_X - 1)) {
        z = Map[SMapX + 1][SMapY] & LOMASK;
        if ((z >= l) && (z <= h)) return true;
    }

    if (SMapY < (WORLD_Y - 1)) {
        z = Map[SMapX][SMapY + 1] & LOMASK;
        if ((z >= l) && (z <= h)) return true;
    }

    if (SMapX > 0) {
        z = Map[SMapX - 1][SMapY] & LOMASK;
        if ((z >= l) && (z <= h)) return true;
    }

    return false;
}

bool RoadTest(int32_t tile)
{
    int32_t x = tile & LOMASK;
    if (x < ROADBASE) return false;
    if (x > LASTRAIL) return false;
    if ((x >= POWERBASE) && (x < RAILHPOWERV)) return false;
    return true;
}