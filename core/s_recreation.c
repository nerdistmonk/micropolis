/* s_recreation.c: Recreation coverage for placeable buildings.
 *
 * Each park adds capacity for 9 buildings, and each stadium for 48. Funding scales that capacity
 * (RecEffect / 1000), so an unfunded park adds none. Park and stadium capacity is one citywide pool,
 * as garage and station capacity is for traffic. Placeable buildings (zones, parks, water towers, and
 * so on) are covered in map order, up to the pool size. The covered window moves forward at each rush
 * hour, so every building takes its turn.
 * Residential population outside coverage is tallied in ResUnrecPop.
 */

#include <string.h>
#include "sim.h"

#define REC_CAPACITY_PARK 9
#define REC_CAPACITY_STADIUM 48

static int32_t rec_capacity;
static int32_t rec_demand;
static int32_t rec_rotor;
static int32_t rec_seen_rush = -1;
static bool rec_cover[WORLD_X][WORLD_Y];

static void MarkRecFootprint(int32_t ox, int32_t oy, int32_t size)
{
    for (int32_t tx = ox; tx < ox + size; tx++) {
        for (int32_t ty = oy; ty < oy + size; ty++) {
            if (TestBounds(tx, ty)) rec_cover[tx][ty] = true;
        }
    }
}

void RecAnalysis(void)
{
    memset(rec_cover, 0, sizeof(rec_cover));
    rec_capacity = 0;

    int32_t park_cap = REC_CAPACITY_PARK * (int32_t)RecEffect / 1000;
    int32_t stadium_cap = REC_CAPACITY_STADIUM * (int32_t)RecEffect / 1000;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            uint16_t low = t & LOMASK;
            if (low >= FOUNTAIN && low < FOUNTAIN + FOUNTAIN_FRAMES) {
                rec_capacity += (park_cap > 0) ? park_cap : 0;
            } else if ((t & ZONEBIT) && low >= STADIUMBASE && low <= FULLSTADIUM) {
                rec_capacity += (stadium_cap > 0) ? stadium_cap : 0;
            }
        }
    }

    rec_demand = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (BuildingAnchorAt(x, y, &ox, &oy, &size)) rec_demand++;
        }
    }
    if (rec_demand == 0) return;

    int32_t served = rec_capacity < rec_demand ? rec_capacity : rec_demand;
    int32_t rush = TrafRushHourCount();
    if (rush != rec_seen_rush) {
        rec_seen_rush = rush;
        rec_rotor = (rec_rotor + served) % rec_demand;
    }

    int32_t start = rec_rotor % rec_demand;
    int32_t ordinal = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (!BuildingAnchorAt(x, y, &ox, &oy, &size)) continue;
            bool covered = ((ordinal - start + rec_demand) % rec_demand) < served;
            ordinal++;
            if (covered) MarkRecFootprint(ox, oy, size);
        }
    }
}

bool RecIsCovered(int32_t x, int32_t y)
{
    return TestBounds(x, y) && rec_cover[x][y];
}

void RecSystemStats(int32_t *capacity, int32_t *demand)
{
    *capacity = rec_capacity;
    *demand = rec_demand;
}
