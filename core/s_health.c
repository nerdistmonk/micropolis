/* s_health.c: Hospital coverage for residential tax.
 *
 * A hospital adds its strength to HospStMap as it is scanned, using the same rules as a fire
 * station: halved if unpowered, halved again with no road nearby. HealthAnalysis smooths that
 * map like the fire map, so hospitals cover the same area fire stations do. Residential zones
 * outside coverage contribute only HEALTH_UNCOVERED_PCT of their population to tax. Covered
 * zones reach full value when healthPercent is 1.0, and fall back to the uncovered rate as
 * funding drops.
 */

#include <string.h>
#include "sim.h"

#define HEALTH_UNCOVERED_PCT 80
#define HEALTH_COVERED_SPAN 20

static int16_t health_rate[SmX][SmY];

void HealthAnalysis(void)
{
    memcpy(health_rate, HospStMap, sizeof(health_rate));
    SmoothServiceMap(health_rate);
    SmoothServiceMap(health_rate);
    SmoothServiceMap(health_rate);
}

bool HealthIsCovered(int32_t x, int32_t y)
{
    return health_rate[x >> 3][y >> 3] != 0;
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
