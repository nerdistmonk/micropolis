/* s_recreation.c: Recreation coverage for citizen votes.
 *
 * Each stadium adds to RecStMap from its zone-sim tick, like hospitals do for HospStMap.
 * Fountains are read from the map on each analysis. A fountain covers its own cell and the
 * eight around it (about nine zones). A stadium covers every cell within
 * RECREATION_STADIUM_RADIUS, so four stadiums spread across the map cover all of it.
 * Residential population outside coverage is tallied in ResUnrecPop.
 */

#include <string.h>
#include "sim.h"

#define RECREATION_STADIUM_RADIUS 5

static bool rec_cover[SmX][SmY];

static void MarkRecCells(int32_t cx, int32_t cy, int32_t radius)
{
    for (int32_t dx = -radius; dx <= radius; dx++) {
        for (int32_t dy = -radius; dy <= radius; dy++) {
            int32_t nx = cx + dx;
            int32_t ny = cy + dy;
            if (nx >= 0 && nx < SmX && ny >= 0 && ny < SmY) {
                rec_cover[nx][ny] = true;
            }
        }
    }
}

void RecAnalysis(void)
{
    memset(rec_cover, 0, sizeof(rec_cover));

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y] & LOMASK;
            if (t >= FOUNTAIN && t < FOUNTAIN + FOUNTAIN_FRAMES) {
                MarkRecCells(x >> 3, y >> 3, 1);
            }
        }
    }

    for (int32_t cx = 0; cx < SmX; cx++) {
        for (int32_t cy = 0; cy < SmY; cy++) {
            if (RecStMap[cx][cy] > 0) {
                MarkRecCells(cx, cy, RECREATION_STADIUM_RADIUS);
            }
        }
    }
}

bool RecIsCovered(int32_t x, int32_t y)
{
    return rec_cover[x >> 3][y >> 3];
}
