/* s_police.c: Police coverage for zones.
 *
 * Each police station serves up to 24 zones when it has power and water, and 12 when either is
 * missing. Funding scales that capacity (PoliceEffect / 1000), so an unfunded station serves none.
 * Each residential, commercial, or industrial zone is assigned to the nearest station with spare
 * capacity, citywide, with no radius limit. Zones that get no station are uncovered.
 * Covered zones have near-zero crime. Uncovered zones get no police benefit.
 */

#include <stdlib.h>
#include <string.h>
#include "sim.h"

#define POLICE_CAPACITY_FULL 24
#define POLICE_CAPACITY_DEGRADED 12
#define POLICE_MAX_STATIONS ((WORLD_X * WORLD_Y) / 9 + 1)

typedef struct {
    int32_t x;
    int32_t y;
    int32_t capacity;
    int32_t assigned;
} PoliceStation;

static PoliceStation police_stations[POLICE_MAX_STATIONS];
static int32_t police_station_count;
static uint8_t police_zone_state[WORLD_X][WORLD_Y];
static bool police_cover[HWLDX][HWLDY];

static bool IsPoliceZoneTile(uint16_t t)
{
    int32_t low = t & LOMASK;
    if (!(t & ZONEBIT)) return false;
    return ((low >= RESBASE) && (low < HOSPITAL)) ||
           ((low >= COMBASE) && (low < INDBASE)) ||
           ((low >= INDBASE) && (low < PORTBASE));
}

static PoliceStation *NearestOpenStation(int32_t x, int32_t y)
{
    PoliceStation *best = NULL;
    int32_t best_d = 0;
    for (int32_t i = 0; i < police_station_count; i++) {
        PoliceStation *s = &police_stations[i];
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

static void MarkZoneFootprint(int32_t cx, int32_t cy, bool covered)
{
    for (int32_t dx = -1; dx <= 1; dx++) {
        for (int32_t dy = -1; dy <= 1; dy++) {
            int32_t nx = cx + dx;
            int32_t ny = cy + dy;
            if (!TestBounds(nx, ny)) continue;
            if (covered) {
                police_zone_state[nx][ny] = POLICE_ZONE_COVERED;
                police_cover[nx >> 1][ny >> 1] = true;
            } else if (police_zone_state[nx][ny] == POLICE_ZONE_NONE) {
                police_zone_state[nx][ny] = POLICE_ZONE_UNCOVERED;
            }
        }
    }
}

void PoliceAnalysis(void)
{
    memset(police_zone_state, 0, sizeof(police_zone_state));
    memset(police_cover, 0, sizeof(police_cover));
    police_station_count = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            if ((t & ZONEBIT) && ((t & LOMASK) == POLICESTATION) &&
                (police_station_count < POLICE_MAX_STATIONS)) {
                PoliceStation *s = &police_stations[police_station_count++];
                bool degraded = !(t & PWRBIT) || !WaterIsSupplied(x, y);
                int32_t base = degraded ? POLICE_CAPACITY_DEGRADED : POLICE_CAPACITY_FULL;
                int32_t cap = base * (int32_t)PoliceEffect / 1000;
                s->x = x;
                s->y = y;
                s->capacity = (cap > 0) ? cap : 0;
                s->assigned = 0;
            }
        }
    }

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if (!IsPoliceZoneTile(Map[x][y])) continue;
            PoliceStation *s = NearestOpenStation(x, y);
            if (s) {
                s->assigned++;
                MarkZoneFootprint(x, y, true);
            } else {
                MarkZoneFootprint(x, y, false);
            }
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

bool PoliceStationStats(int32_t x, int32_t y, int32_t *capacity, int32_t *assigned)
{
    for (int32_t i = 0; i < police_station_count; i++) {
        PoliceStation *s = &police_stations[i];
        if (abs(s->x - x) <= 1 && abs(s->y - y) <= 1) {
            *capacity = s->capacity;
            *assigned = s->assigned;
            return true;
        }
    }
    return false;
}
