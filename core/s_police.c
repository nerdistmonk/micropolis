/* s_police.c: Police coverage for placeable buildings.
 *
 * Each police station adds capacity: 24 buildings when it has power and water, and 12 when either is
 * missing. Funding scales that capacity (PoliceEffect / 1000), so an unfunded station adds none.
 * Station capacity is one citywide pool, as garage and station capacity is for traffic. Placeable
 * buildings (zones, parks, water towers) are covered in map order, up to the pool size. The covered
 * window moves forward at each rush hour, so every building takes its turn.
 * Covered zones have near-zero crime. Uncovered zones get no police benefit.
 */

#include <string.h>
#include "sim.h"

#define POLICE_CAPACITY_FULL 24
#define POLICE_CAPACITY_DEGRADED 12

static int32_t police_capacity;
static int32_t police_demand;
static int32_t police_rotor;
static int32_t police_seen_rush = -1;
static uint8_t police_zone_state[WORLD_X][WORLD_Y];
static bool police_cover[HWLDX][HWLDY];

static void MarkZoneFootprint(int32_t ox, int32_t oy, int32_t size, bool covered)
{
    for (int32_t tx = ox; tx < ox + size; tx++) {
        for (int32_t ty = oy; ty < oy + size; ty++) {
            if (!TestBounds(tx, ty)) continue;
            if (covered) {
                police_zone_state[tx][ty] = POLICE_ZONE_COVERED;
                police_cover[tx >> 1][ty >> 1] = true;
            } else if (police_zone_state[tx][ty] == POLICE_ZONE_NONE) {
                police_zone_state[tx][ty] = POLICE_ZONE_UNCOVERED;
            }
        }
    }
}

void PoliceAnalysis(void)
{
    memset(police_zone_state, 0, sizeof(police_zone_state));
    memset(police_cover, 0, sizeof(police_cover));
    police_capacity = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            if ((t & ZONEBIT) && ((t & LOMASK) == POLICESTATION)) {
                bool degraded = !(t & PWRBIT) || !WaterIsSupplied(x, y);
                int32_t base = degraded ? POLICE_CAPACITY_DEGRADED : POLICE_CAPACITY_FULL;
                int32_t cap = base * (int32_t)PoliceEffect / 1000;
                police_capacity += (cap > 0) ? cap : 0;
            }
        }
    }

    police_demand = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (BuildingAnchorAt(x, y, &ox, &oy, &size)) police_demand++;
        }
    }
    if (police_demand == 0) return;

    int32_t served = police_capacity < police_demand ? police_capacity : police_demand;
    int32_t rush = TrafRushHourCount();
    if (rush != police_seen_rush) {
        police_seen_rush = rush;
        police_rotor = (police_rotor + served) % police_demand;
    }

    int32_t start = police_rotor % police_demand;
    int32_t ordinal = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (!BuildingAnchorAt(x, y, &ox, &oy, &size)) continue;
            bool covered = ((ordinal - start + police_demand) % police_demand) < served;
            ordinal++;
            MarkZoneFootprint(ox, oy, size, covered);
        }
    }
}

int32_t PoliceZoneState(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return POLICE_ZONE_NONE;
    return police_zone_state[x][y];
}

bool PoliceCoversCrimeCell(int32_t hx, int32_t hy)
{
    return police_cover[hx][hy];
}

void PoliceSystemStats(int32_t *capacity, int32_t *demand)
{
    *capacity = police_capacity;
    *demand = police_demand;
}
