/* s_health.c: Hospital coverage for placeable buildings.
 *
 * Each hospital adds capacity: 18 buildings when it has power and water, and 9 when either is
 * missing. Funding scales that capacity (HealthEffect / 1000), so an unfunded hospital adds none.
 * Hospital capacity is one citywide pool, as garage and station capacity is for traffic. Placeable
 * buildings (zones, parks, water towers) are covered in map order, up to the pool size. The covered
 * window moves forward at each rush hour, so every building takes its turn.
 * Residential zones outside coverage contribute only HEALTH_UNCOVERED_PCT of their population to tax.
 * Covered zones reach full value when healthPercent is 1.0, and fall back to the uncovered rate as
 * funding drops.
 */

#include <string.h>
#include "sim.h"

#define HEALTH_UNCOVERED_PCT 80
#define HEALTH_COVERED_SPAN 20
#define HEALTH_CAPACITY_FULL 18
#define HEALTH_CAPACITY_DEGRADED 9

static int32_t health_capacity;
static int32_t health_demand;
static int32_t health_rotor;
static int32_t health_seen_rush = -1;
static bool health_covered[WORLD_X][WORLD_Y];

static void MarkHealthFootprint(int32_t ox, int32_t oy, int32_t size)
{
    for (int32_t tx = ox; tx < ox + size; tx++) {
        for (int32_t ty = oy; ty < oy + size; ty++) {
            if (TestBounds(tx, ty)) health_covered[tx][ty] = true;
        }
    }
}

void HealthAnalysis(void)
{
    memset(health_covered, 0, sizeof(health_covered));
    health_capacity = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            if ((t & ZONEBIT) && ((t & LOMASK) == HOSPITAL)) {
                bool degraded = !(t & PWRBIT) || !WaterIsSupplied(x, y);
                int32_t base = degraded ? HEALTH_CAPACITY_DEGRADED : HEALTH_CAPACITY_FULL;
                int32_t cap = base * (int32_t)HealthEffect / 1000;
                health_capacity += (cap > 0) ? cap : 0;
            }
        }
    }

    health_demand = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (BuildingAnchorAt(x, y, &ox, &oy, &size)) health_demand++;
        }
    }
    if (health_demand == 0) return;

    int32_t served = health_capacity < health_demand ? health_capacity : health_demand;
    int32_t rush = TrafRushHourCount();
    if (rush != health_seen_rush) {
        health_seen_rush = rush;
        health_rotor = (health_rotor + served) % health_demand;
    }

    int32_t start = health_rotor % health_demand;
    int32_t ordinal = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (!BuildingAnchorAt(x, y, &ox, &oy, &size)) continue;
            bool covered = ((ordinal - start + health_demand) % health_demand) < served;
            ordinal++;
            if (covered) MarkHealthFootprint(ox, oy, size);
        }
    }
}

bool HealthIsCovered(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    return health_covered[x][y];
}

void HospitalSystemStats(int32_t *capacity, int32_t *demand)
{
    *capacity = health_capacity;
    *demand = health_demand;
}

int16_t HealthCoverageLevel(int32_t x, int32_t y)
{
    return HealthIsCovered(x, y) ? 255 : 0;
}

int32_t HealthAdjustedPop(int32_t pop, int32_t x, int32_t y)
{
    int32_t pct = HEALTH_UNCOVERED_PCT;
    if (HealthIsCovered(x, y)) {
        int32_t effect = HealthEffect < 0 ? 0 : (HealthEffect > 1000 ? 1000 : HealthEffect);
        pct += (HEALTH_COVERED_SPAN * effect) / 1000;
    }
    return (pop * pct) / 100;
}
