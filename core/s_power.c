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
/* Each grid pools the capacity of all its plants. A shortfall of 1..5 zones
 * blacks out that many zones per scan; 6 or more destroys the grid's plants. */
#define COAL_ZONES_SUPPLIED 24
#define NUCLEAR_ZONES_SUPPLIED 48
#define MAX_BLACKOUT_SHORTFALL 5
#define MELTDOWN_SHORTFALL 6

static int16_t PlantSeedX[PWRSTKSIZE];
static int16_t PlantSeedY[PWRSTKSIZE];
static int16_t ClaimX[WORLD_X * WORLD_Y];
static int16_t ClaimY[WORLD_X * WORLD_Y];

/* Per-scan grid bookkeeping. GridTag holds grid root index + 1 (0 = not on a grid).
 * ClaimOwner holds the plant seed index + 1 that claimed a conductor tile. SeedParent
 * is a union-find over plant seeds, so plants that touch the same conductor network
 * form one grid and share its capacity. */
static int16_t GridTag[WORLD_X][WORLD_Y];
static int16_t ClaimOwner[WORLD_X][WORLD_Y];
static int32_t SeedParent[PWRSTKSIZE];
static int32_t GridDemand[PWRSTKSIZE];
static int32_t GridCapacity[PWRSTKSIZE];
static bool GridExplodes[PWRSTKSIZE];
static int32_t NeighborFeedZones[NB_SIDES];
static uint32_t BlackoutRotor = 0;
#define SEED_SEEN ((int16_t)-1)

/* Forward Declarations of Internal Functions */
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

static bool IsPowerZone(uint16_t tile)
{
    return (tile & ZONEBIT) && !TILE_IS_POWER_PLANT(tile) && !TILE_IS_LANDFILL(tile);
}

static bool IsConductiveTile(int32_t x, int32_t y)
{
    uint16_t t = Map[x][y];
    if (TILE_IS_WATER_TOWER(t)) return true;
    return (t & CONDBIT) && !TILE_IS_NUCLEAR(t) && ((t & LOMASK) != POWERPLANT);
}

static int32_t SeedFind(int32_t s)
{
    while (SeedParent[s] != s) {
        SeedParent[s] = SeedParent[SeedParent[s]];
        s = SeedParent[s];
    }
    return s;
}

static void SeedUnion(int32_t a, int32_t b)
{
    a = SeedFind(a);
    b = SeedFind(b);
    if (a != b) SeedParent[b] = a;
}

/* Claims an unclaimed conductor for `seed`. Touching a tile another plant already
 * claimed joins the two plants into one grid; that grid's tiles are fully claimed
 * by its owner, so there is nothing more to walk from here. */
static void TryClaimNeighbor(int32_t seed, int32_t x, int32_t y, int32_t *claimed)
{
    if (!TestBounds(x, y)) return;
    if (ClaimOwner[x][y]) {
        SeedUnion(seed, ClaimOwner[x][y] - 1);
        return;
    }
    if (!IsConductiveTile(x, y)) return;

    PowerMap[POWERWORD(x, y)] |= (uint16_t)(1u << (x & 15));
    ClaimOwner[x][y] = (int16_t)(seed + 1);
    ClaimX[*claimed] = (int16_t)x;
    ClaimY[*claimed] = (int16_t)y;
    (*claimed)++;
}

static void FloodFromSeed(int32_t seed, int32_t sx, int32_t sy, int32_t *claimed)
{
    static const int32_t dx[4] = { 0, 1, 0, -1 };
    static const int32_t dy[4] = { -1, 0, 1, 0 };
    int32_t head = *claimed;

    for (int32_t d = 0; d < 4; d++) TryClaimNeighbor(seed, sx + dx[d], sy + dy[d], claimed);
    while (head < *claimed) {
        int32_t x = ClaimX[head];
        int32_t y = ClaimY[head];
        head++;
        for (int32_t d = 0; d < 4; d++) TryClaimNeighbor(seed, x + dx[d], y + dy[d], claimed);
    }
}

static void ClearGridClaims(int32_t g, int32_t tiles)
{
    for (int32_t t = 0; t < tiles; t++) {
        int32_t x = ClaimX[t];
        int32_t y = ClaimY[t];
        if (GridTag[x][y] != g + 1) continue;
        PowerMap[POWERWORD(x, y)] &= (uint16_t)~(1u << (x & 15));
    }
}

/* Unpowers `count` zones of grid g, starting at the rotating cursor so the
 * blackout moves across the whole grid over successive scans. */
static void DarkenGridZones(int32_t g, int32_t count, int32_t tiles)
{
    int32_t zones = GridDemand[g];
    int32_t first = (int32_t)(BlackoutRotor % (uint32_t)zones);
    int32_t index = 0;

    for (int32_t t = 0; t < tiles; t++) {
        int32_t x = ClaimX[t];
        int32_t y = ClaimY[t];
        if (GridTag[x][y] != g + 1 || !IsPowerZone(Map[x][y])) continue;
        if (((index - first + zones) % zones) < count) {
            PowerMap[POWERWORD(x, y)] &= (uint16_t)~(1u << (x & 15));
        }
        index++;
    }
    BlackoutRotor += (uint32_t)count;
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
    int32_t tiles = 0;

    for (int32_t x = 0; x < PWRMAPSIZE; x++) {
        PowerMap[x] = 0;
    }
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            GridTag[x][y] = 0;
            ClaimOwner[x][y] = 0;
        }
    }

    while (PowerStackNum > 0) {
        PullPowerStack();
        PlantSeedX[plants] = SMapX;
        PlantSeedY[plants] = SMapY;
        plants++;
    }

    const int32_t mapPlants = plants;
    for (int32_t s = 0; s < NB_SIDES && plants < PWRSTKSIZE; s++) {
        int32_t fx, fy;
        if (!Neighbor_FeedSeed(s, NB_GOODS_POWER, &fx, &fy)) continue;
        PlantSeedX[plants] = (int16_t)fx;
        PlantSeedY[plants] = (int16_t)fy;
        NeighborFeedZones[plants - mapPlants] = Neighbor_FeedUnits(s, NB_GOODS_POWER) * Neighbor_FeedZones(NB_GOODS_POWER);
        plants++;
    }

    for (int32_t i = 0; i < plants; i++) {
        int32_t sx = PlantSeedX[i];
        int32_t sy = PlantSeedY[i];
        GridDemand[i] = 0;
        GridCapacity[i] = 0;
        GridExplodes[i] = false;
        SeedParent[i] = i;
        if (GridTag[sx][sy] == SEED_SEEN) {
            SeedParent[i] = -1;
            continue;
        }
        GridTag[sx][sy] = SEED_SEEN;
        FloodFromSeed(i, sx, sy, &tiles);
    }

    for (int32_t t = 0; t < tiles; t++) {
        int32_t x = ClaimX[t];
        int32_t y = ClaimY[t];
        int32_t r = SeedFind(ClaimOwner[x][y] - 1);
        GridTag[x][y] = (int16_t)(r + 1);
        if (IsPowerZone(Map[x][y])) GridDemand[r]++;
    }

    for (int32_t i = 0; i < plants; i++) {
        if (SeedParent[i] < 0) continue;
        int32_t sx = PlantSeedX[i];
        int32_t sy = PlantSeedY[i];
        int32_t r = SeedFind(i);
        GridTag[sx][sy] = (int16_t)(r + 1);
        GridCapacity[r] += (i < mapPlants) ? (TILE_IS_NUCLEAR(Map[sx][sy]) ? NUCLEAR_ZONES_SUPPLIED : COAL_ZONES_SUPPLIED)
                                           : NeighborFeedZones[i - mapPlants];
    }

    /* Neighbor power purchases draw real load at their gate. Not capped by spare capacity: overselling
     * leaves the grid short, which the blackout and meltdown rules punish. */
    Neighbor_ClearSales(NB_GOODS_POWER);
    for (int32_t s = 0; s < NB_SIDES; s++) {
        int32_t gx, gy;
        if (!Neighbor_GateTile(s, NB_GOODS_POWER, &gx, &gy) || !TestBounds(gx, gy) || GridTag[gx][gy] == 0) continue;
        int32_t r = GridTag[gx][gy] - 1;
        GridDemand[r] += Neighbor_OfferSale(s, NB_GOODS_POWER, GridCapacity[r] - GridDemand[r]);
    }

    bool blackout = false;
    for (int32_t g = 0; g < plants; g++) {
        if (SeedParent[g] != g) continue;
        int32_t shortfall = GridDemand[g] - GridCapacity[g];
        if (shortfall >= MELTDOWN_SHORTFALL) {
            GridExplodes[g] = true;
            ClearGridClaims(g, tiles);
        } else if (shortfall > 0) {
            DarkenGridZones(g, shortfall, tiles);
            blackout = true;
        }
    }

    /* Sold load stays on the grid during a blackout, but the neighbor is not paid for it. */
    for (int32_t s = 0; s < NB_SIDES; s++) {
        int32_t gx, gy;
        if (!Neighbor_GateTile(s, NB_GOODS_POWER, &gx, &gy) || !TestBounds(gx, gy) || GridTag[gx][gy] == 0) continue;
        int32_t r = GridTag[gx][gy] - 1;
        if (GridDemand[r] > GridCapacity[r]) Neighbor_BlackoutSale(s, NB_GOODS_POWER);
    }

    if (blackout) {
        SendMes(40);
        NewsThrottled(40);
    }

    for (int32_t i = 0; i < plants; i++) {
        if (SeedParent[i] < 0 || i >= mapPlants) continue;
        if (GridExplodes[SeedFind(i)]) {
            ExplodePlant(PlantSeedX[i], PlantSeedY[i]);
        }
    }

    PowerStackNum = 0;
}

bool PowerGridStats(int32_t x, int32_t y, int32_t *demand, int32_t *capacity)
{
    if (!TestBounds(x, y)) return false;
    int16_t tag = GridTag[x][y];
    if (!tag) return false;
    *demand = GridDemand[tag - 1];
    *capacity = GridCapacity[tag - 1];
    return true;
}
