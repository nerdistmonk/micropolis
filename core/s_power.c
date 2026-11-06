/* s_power.c: Electrical grid scanning, power propagation, and plant stack management.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"

/* Power Stack and Capacity State */
int32_t PowerStackNum = 0;
static int16_t PowerStackX[PWRSTKSIZE];
static int16_t PowerStackY[PWRSTKSIZE];
#define COAL_BLACKOUT_ZONES 12
#define COAL_EXPLODE_ZONES 18
#define NUCLEAR_BLACKOUT_ZONES 18
#define NUCLEAR_MELTDOWN_ZONES 30

static int16_t PlantSeedX[PWRSTKSIZE];
static int16_t PlantSeedY[PWRSTKSIZE];
static int16_t ClaimX[WORLD_X * WORLD_Y];
static int16_t ClaimY[WORLD_X * WORLD_Y];

/* Forward Declarations of Internal Functions */
static bool TestForCond(int16_t dir);
static void PullPowerStack(void);

bool MoveMapSim(int16_t mdir)
{
    switch (mdir) {
    case 0: /* North */
        if (SMapY > 0) {
            SMapY--;
            return true;
        }
        if (SMapY < 0) {
            SMapY = 0;
        }
        return false;

    case 1: /* East */
        if (SMapX < (WORLD_X - 1)) {
            SMapX++;
            return true;
        }
        if (SMapX > (WORLD_X - 1)) {
            SMapX = WORLD_X - 1;
        }
        return false;

    case 2: /* South */
        if (SMapY < (WORLD_Y - 1)) {
            SMapY++;
            return true;
        }
        if (SMapY > (WORLD_Y - 1)) {
            SMapY = WORLD_Y - 1;
        }
        return false;

    case 3: /* West */
        if (SMapX > 0) {
            SMapX--;
            return true;
        }
        if (SMapX < 0) {
            SMapX = 0;
        }
        return false;

    case 4: /* Current position / No movement */
        return true;

    default:
        break;
    }
    return false;
}

static bool TestForCond(int16_t dir)
{
    int16_t xsave = SMapX;
    int16_t ysave = SMapY;

    if (MoveMapSim(dir)) {
        if ((Map[SMapX][SMapY] & CONDBIT) &&
            (CChr9 != NUCLEAR) &&
            (CChr9 != POWERPLANT)) {
            int32_t power_wrd = POWERWORD(SMapX, SMapY);
            if ((power_wrd >= PWRMAPSIZE) ||
                ((PowerMap[power_wrd] & (uint16_t)(1u << (SMapX & 15))) == 0)) {
                SMapX = xsave;
                SMapY = ysave;
                return true;
            }
        }
    }

    SMapX = xsave;
    SMapY = ysave;
    return false;
}

void PushPowerStack(void)
{
    if (PowerStackNum < (PWRSTKSIZE - 2)) {
        PowerStackNum++;
        PowerStackX[PowerStackNum] = SMapX;
        PowerStackY[PowerStackNum] = SMapY;
    }
}

static void PullPowerStack(void)
{
    if (PowerStackNum > 0) {
        SMapX = PowerStackX[PowerStackNum];
        SMapY = PowerStackY[PowerStackNum];
        PowerStackNum--;
    }
}

static void ClaimPowerTile(int32_t *zones, int32_t *claimed)
{
    int32_t word = POWERWORD(SMapX, SMapY);
    uint16_t bit = (uint16_t)(1u << (SMapX & 15));
    if (PowerMap[word] & bit) return;

    PowerMap[word] |= bit;
    ClaimX[*claimed] = SMapX;
    ClaimY[*claimed] = SMapY;
    (*claimed)++;
    if ((Map[SMapX][SMapY] & ZONEBIT) && !TILE_IS_POWER_PLANT(Map[SMapX][SMapY])) (*zones)++;
}

static void ClearClaims(int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        int32_t x = ClaimX[i];
        int32_t y = ClaimY[i];
        PowerMap[POWERWORD(x, y)] &= (uint16_t)~(1u << (x & 15));
    }
}

static void WalkPlantNetwork(int32_t *zones, int32_t *claimed)
{
    int16_t adir;
    int32_t con_num, dir;

    while (PowerStackNum > 0) {
        PullPowerStack();
        adir = 4;
        do {
            MoveMapSim(adir);
            ClaimPowerTile(zones, claimed);

            con_num = 0;
            dir = 0;
            while ((dir < 4) && (con_num < 2)) {
                if (TestForCond((int16_t)dir)) {
                    con_num++;
                    adir = (int16_t)dir;
                }
                dir++;
            }

            if (con_num > 1) {
                PushPowerStack();
            }
        } while (con_num > 0);
    }
}

static void ExplodePlant(int32_t sx, int32_t sy)
{
    if (TILE_IS_NUCLEAR(Map[sx][sy])) {
        DoMeltdown(sx, sy);
        return;
    }
    for (int32_t x = sx - 1; x < sx + 3; x++) {
        for (int32_t y = sy - 1; y < sy + 3; y++) {
            if (!TestBounds(x, y)) continue;
            Map[x][y] = (uint16_t)(FIRE + (Rand16() & 3) + ANIMBIT);
            MakeExplosionAt((x << 4) + 8, (y << 4) + 8);
        }
    }
}

void DoPowerScan(void)
{
    int32_t plants = 0;

    for (int32_t x = 0; x < PWRMAPSIZE; x++) {
        PowerMap[x] = 0;
    }

    while (PowerStackNum > 0) {
        PullPowerStack();
        PlantSeedX[plants] = SMapX;
        PlantSeedY[plants] = SMapY;
        plants++;
    }

    for (int32_t i = 0; i < plants; i++) {
        int32_t zones = 0;
        int32_t claimed = 0;
        int32_t sx = PlantSeedX[i];
        int32_t sy = PlantSeedY[i];
        bool nuclear = TILE_IS_NUCLEAR(Map[sx][sy]);

        SMapX = (int16_t)sx;
        SMapY = (int16_t)sy;
        PowerStackNum = 0;
        PushPowerStack();
        WalkPlantNetwork(&zones, &claimed);

        int32_t meltdown_at = nuclear ? NUCLEAR_MELTDOWN_ZONES : COAL_EXPLODE_ZONES;
        int32_t blackout_at = nuclear ? NUCLEAR_BLACKOUT_ZONES : COAL_BLACKOUT_ZONES;

        if (zones >= meltdown_at) {
            ClearClaims(claimed);
            ExplodePlant(sx, sy);
        } else if (zones >= blackout_at && Rand(2) == 0) {
            ClearClaims(claimed);
            SendMes(40);
            NewsThrottled(40);
        }
    }

    PowerStackNum = 0;
}