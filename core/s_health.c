/* s_health.c: Hospital coverage for placeable buildings.
 *
 * Each hospital serves up to 18 buildings when it has power and water, and 9 when either is
 * missing. Every placeable building (zones, parks, water towers) is assigned to the nearest hospital
 * with spare capacity, citywide, with no radius limit. Buildings with no hospital are uncovered.
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
#define HEALTH_MAX_STATIONS ((WORLD_X * WORLD_Y) / 9 + 1)

typedef struct {
    int32_t x;
    int32_t y;
    int32_t capacity;
    int32_t assigned;
} HospitalStation;

static HospitalStation hospital_stations[HEALTH_MAX_STATIONS];
static int32_t hospital_station_count;
static bool health_covered[WORLD_X][WORLD_Y];

static HospitalStation *NearestOpenHospital(int32_t x, int32_t y)
{
    HospitalStation *best = NULL;
    int32_t best_d = 0;
    for (int32_t i = 0; i < hospital_station_count; i++) {
        HospitalStation *s = &hospital_stations[i];
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
    hospital_station_count = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            if ((t & ZONEBIT) && ((t & LOMASK) == HOSPITAL) &&
                (hospital_station_count < HEALTH_MAX_STATIONS)) {
                HospitalStation *s = &hospital_stations[hospital_station_count++];
                bool degraded = !(t & PWRBIT) || !WaterIsSupplied(x, y);
                s->x = x;
                s->y = y;
                s->capacity = degraded ? HEALTH_CAPACITY_DEGRADED : HEALTH_CAPACITY_FULL;
                s->assigned = 0;
            }
        }
    }

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (!BuildingAnchorAt(x, y, &ox, &oy, &size)) continue;
            HospitalStation *s = NearestOpenHospital(x, y);
            if (s) {
                s->assigned++;
                MarkHealthFootprint(ox, oy, size);
            }
        }
    }
}

bool HealthIsCovered(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    return health_covered[x][y];
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
