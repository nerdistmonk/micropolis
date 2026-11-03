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
static int32_t MaxPower = 0;
static int32_t NumPower = 0;

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

void DoPowerScan(void)
{
    int16_t adir;
    int32_t con_num, dir;

    for (int32_t x = 0; x < PWRMAPSIZE; x++) {
        PowerMap[x] = 0;
    }

    MaxPower = (CoalPop * 700) + (NuclearPop * 2000);
    NumPower = 0;

    while (PowerStackNum > 0) {
        PullPowerStack();
        adir = 4;
        do {
            if (++NumPower > MaxPower) {
                SendMes(40); /* Power outage notification */
                return;
            }

            MoveMapSim(adir);
            SETPOWERBIT(SMapX, SMapY);

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