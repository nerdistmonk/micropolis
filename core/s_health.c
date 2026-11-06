/* s_health.c: Hospital coverage for residential tax.
 *
 * A hospital covers the 8x8 blocks within HEALTH_RADIUS blocks of its center. Residential
 * zones outside coverage contribute only HEALTH_UNCOVERED_PCT of their population to tax.
 * Covered zones reach full value when healthPercent is 1.0, and fall back to the
 * uncovered rate as funding drops.
 */

#include <string.h>
#include "sim.h"

#define HEALTH_RADIUS 2
#define HEALTH_UNCOVERED_PCT 80
#define HEALTH_COVERED_SPAN 20

static uint8_t health_covered[SmX][SmY];

void HealthAnalysis(void)
{
    memset(health_covered, 0, sizeof(health_covered));

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if ((Map[x][y] & LOMASK) != HOSPITAL) continue;

            int32_t bx = x >> 3;
            int32_t by = y >> 3;
            int32_t x1 = bx - HEALTH_RADIUS < 0 ? 0 : bx - HEALTH_RADIUS;
            int32_t x2 = bx + HEALTH_RADIUS > SmX - 1 ? SmX - 1 : bx + HEALTH_RADIUS;
            int32_t y1 = by - HEALTH_RADIUS < 0 ? 0 : by - HEALTH_RADIUS;
            int32_t y2 = by + HEALTH_RADIUS > SmY - 1 ? SmY - 1 : by + HEALTH_RADIUS;

            for (int32_t cx = x1; cx <= x2; cx++) {
                for (int32_t cy = y1; cy <= y2; cy++) {
                    health_covered[cx][cy] = 1;
                }
            }
        }
    }
}

bool HealthIsCovered(int32_t x, int32_t y)
{
    return health_covered[x >> 3][y >> 3] != 0;
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
