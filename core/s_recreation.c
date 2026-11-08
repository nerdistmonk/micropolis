/* s_recreation.c: Recreation coverage for placeable buildings.
 *
 * Each park serves up to 9 buildings, and each stadium up to 48. Every placeable building (zones,
 * parks, water towers, and so on) is assigned to the nearest park or stadium with spare capacity,
 * citywide, with no radius limit. Buildings with no park or stadium are unrecreated.
 * Residential population outside coverage is tallied in ResUnrecPop.
 */

#include <string.h>
#include "sim.h"

#define REC_CAPACITY_PARK 9
#define REC_CAPACITY_STADIUM 48
#define REC_MAX_SITES (WORLD_X * WORLD_Y)

typedef struct {
    int32_t x;
    int32_t y;
    int32_t capacity;
    int32_t assigned;
} RecSite;

static RecSite rec_sites[REC_MAX_SITES];
static int32_t rec_site_count;
static bool rec_cover[WORLD_X][WORLD_Y];

static RecSite *NearestOpenSite(int32_t x, int32_t y)
{
    RecSite *best = NULL;
    int32_t best_d = 0;
    for (int32_t i = 0; i < rec_site_count; i++) {
        RecSite *s = &rec_sites[i];
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

static void AddSite(int32_t x, int32_t y, int32_t capacity)
{
    RecSite *s = &rec_sites[rec_site_count++];
    s->x = x;
    s->y = y;
    s->capacity = capacity;
    s->assigned = 0;
}

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
    rec_site_count = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            uint16_t low = t & LOMASK;
            if (low >= FOUNTAIN && low < FOUNTAIN + FOUNTAIN_FRAMES) {
                AddSite(x, y, REC_CAPACITY_PARK);
            } else if ((t & ZONEBIT) && low >= STADIUMBASE && low <= FULLSTADIUM) {
                AddSite(x, y, REC_CAPACITY_STADIUM);
            }
        }
    }

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t ox, oy, size;
            if (!BuildingAnchorAt(x, y, &ox, &oy, &size)) continue;
            RecSite *s = NearestOpenSite(x, y);
            if (s) {
                s->assigned++;
                MarkRecFootprint(ox, oy, size);
            }
        }
    }
}

bool RecIsCovered(int32_t x, int32_t y)
{
    return TestBounds(x, y) && rec_cover[x][y];
}
