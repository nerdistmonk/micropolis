/* s_fire.c: Fire station coverage for placeable buildings.
 *
 * Each fire station adds capacity: 24 buildings when it has power and water, and 12 when either is
 * missing. Funding scales that capacity (FireEffect / 1000), so an unfunded station adds none.
 * Station capacity is one citywide pool, as garage and station capacity is for traffic. Placeable
 * buildings (zones, parks, water towers) are covered in map order, up to the pool size. The covered
 * window moves forward at each rush hour, so every building takes its turn.
 * Covered tiles burn out fast when water is supplied. Uncovered tiles burn at the stock rate.
 */

#include <string.h>
#include "sim.h"

#define FIRE_CAPACITY_FULL 24
#define FIRE_CAPACITY_DEGRADED 12
#define FIRE_BURN_RATE_STOCK 10
#define FIRE_BURN_RATE_COVERED 2

static int32_t fire_capacity;
static int32_t fire_demand;
static int32_t fire_rotor;
static int32_t fire_seen_rush = -1;
static uint8_t fire_zone_state[WORLD_X][WORLD_Y];

static void MarkFireZoneFootprint(int32_t ox, int32_t oy, int32_t size, bool covered)
{
    for (int32_t tx = ox; tx < ox + size; tx++) {
        for (int32_t ty = oy; ty < oy + size; ty++) {
            if (!TestBounds(tx, ty)) continue;
            if (covered) {
                fire_zone_state[tx][ty] = FIRE_ZONE_COVERED;
            } else if (fire_zone_state[tx][ty] == FIRE_ZONE_NONE) {
                fire_zone_state[tx][ty] = FIRE_ZONE_UNCOVERED;
            }
        }
    }
}

void FireCoverageAnalysis(void)
{
    memset(fire_zone_state, 0, sizeof(fire_zone_state));
    fire_capacity = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            if ((t & ZONEBIT) && ((t & LOMASK) == FIRESTATION)) {
                bool degraded = !(t & PWRBIT) || !WaterIsSupplied(x, y);
                int32_t base = degraded ? FIRE_CAPACITY_DEGRADED : FIRE_CAPACITY_FULL;
                int32_t cap = base * (int32_t)FireEffect / 1000;
                fire_capacity += (cap > 0) ? cap : 0;
            }
        }
    }

    fire_demand = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (BuildingAnchorAt(x, y, &ox, &oy, &size)) fire_demand++;
        }
    }
    if (fire_demand == 0) return;

    int32_t served = fire_capacity < fire_demand ? fire_capacity : fire_demand;
    int32_t rush = TrafRushHourCount();
    if (rush != fire_seen_rush) {
        fire_seen_rush = rush;
        fire_rotor = (fire_rotor + served) % fire_demand;
    }

    int32_t start = fire_rotor % fire_demand;
    int32_t ordinal = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (!BuildingAnchorAt(x, y, &ox, &oy, &size)) continue;
            bool covered = ((ordinal - start + fire_demand) % fire_demand) < served;
            ordinal++;
            MarkFireZoneFootprint(ox, oy, size, covered);
        }
    }
}

int32_t FireZoneState(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return FIRE_ZONE_NONE;
    return fire_zone_state[x][y];
}

int16_t FireBurnRate(int32_t x, int32_t y)
{
    if (TestBounds(x, y) && fire_zone_state[x][y] == FIRE_ZONE_COVERED && WaterIsSupplied(x, y)) {
        return FIRE_BURN_RATE_COVERED;
    }
    return FIRE_BURN_RATE_STOCK;
}

void FireSystemStats(int32_t *capacity, int32_t *demand)
{
    *capacity = fire_capacity;
    *demand = fire_demand;
}
