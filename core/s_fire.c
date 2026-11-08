/* s_fire.c: Fire station coverage for placeable buildings.
 *
 * Each fire station serves up to 24 buildings when it has power and water, and 12 when either is
 * missing. Funding scales that capacity (FireEffect / 1000), so an unfunded station serves none.
 * Every placeable building (zones, parks, water towers) is assigned to the nearest station with spare
 * capacity, citywide, with no radius limit. Buildings that get no station are uncovered.
 * Covered tiles burn out fast when water is supplied. Uncovered tiles burn at the stock rate.
 */

#include <stdlib.h>
#include <string.h>
#include "sim.h"

#define FIRE_CAPACITY_FULL 24
#define FIRE_CAPACITY_DEGRADED 12
#define FIRE_MAX_STATIONS ((WORLD_X * WORLD_Y) / 9 + 1)
#define FIRE_BURN_RATE_STOCK 10
#define FIRE_BURN_RATE_COVERED 2

typedef struct {
    int32_t x;
    int32_t y;
    int32_t capacity;
    int32_t assigned;
} FireStation;

static FireStation fire_stations[FIRE_MAX_STATIONS];
static int32_t fire_station_count;
static uint8_t fire_zone_state[WORLD_X][WORLD_Y];

static FireStation *NearestOpenFireStation(int32_t x, int32_t y)
{
    FireStation *best = NULL;
    int32_t best_d = 0;
    for (int32_t i = 0; i < fire_station_count; i++) {
        FireStation *s = &fire_stations[i];
        if (s->assigned >= s->capacity) continue;
        int32_t dx = s->x - x;
        int32_t dy = s->y - y;
        int32_t d = dx * dx + dy * dy;
        if (!best || d < best_d) {
            best = s;
            best_d = d;
        }
    }
    return best;
}

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
    fire_station_count = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            if ((t & ZONEBIT) && ((t & LOMASK) == FIRESTATION) &&
                (fire_station_count < FIRE_MAX_STATIONS)) {
                FireStation *s = &fire_stations[fire_station_count++];
                bool degraded = !(t & PWRBIT) || !WaterIsSupplied(x, y);
                int32_t base = degraded ? FIRE_CAPACITY_DEGRADED : FIRE_CAPACITY_FULL;
                int32_t cap = base * (int32_t)FireEffect / 1000;
                s->x = x;
                s->y = y;
                s->capacity = (cap > 0) ? cap : 0;
                s->assigned = 0;
            }
        }
    }

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (!BuildingAnchorAt(x, y, &ox, &oy, &size)) continue;
            FireStation *s = NearestOpenFireStation(x, y);
            if (s) {
                s->assigned++;
                MarkFireZoneFootprint(ox, oy, size, true);
            } else {
                MarkFireZoneFootprint(ox, oy, size, false);
            }
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

bool FireStationStats(int32_t x, int32_t y, int32_t *capacity, int32_t *assigned)
{
    for (int32_t i = 0; i < fire_station_count; i++) {
        FireStation *s = &fire_stations[i];
        if (abs(s->x - x) <= 1 && abs(s->y - y) <= 1) {
            *capacity = s->capacity;
            *assigned = s->assigned;
            return true;
        }
    }
    return false;
}
