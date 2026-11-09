/* s_traf.c: Road capacity traffic model.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 *
 * The road network is one pooled entity. Each road tile adds a tenth of a zone of
 * capacity, and each parking garage or passenger station adds more. Growing zones
 * beyond capacity are unserviced, and the unserviced set rotates at each rush hour.
 */

#include "sim.h"

/* Rush hour volume: 20 citizens per ResPop, 60% commuting, and a full zone is 40 ResPop */
#define TRAF_CITIZENS_PER_RESPOP 20
#define TRAF_COMMUTE_PERCENT     60
#define TRAF_TRIPS_PER_ZONE      480

int16_t TrafMaxX = 0;
int16_t TrafMaxY = 0;
int32_t TrafServicedResPop = 0;

static uint8_t TrafUnserviced[WORLD_X][WORLD_Y];
static int32_t TrafRotor = 0;
static int32_t TrafProblemUnserviced = 0;
static int32_t TrafProblemZones = 0;

/* Runs at each rush hour. The road grid is one pool: its capacity is the share of
 * road tiles plus garages and stations. Growing zones with road access beyond that
 * capacity are unserviced, and the unserviced window rotates at each rush hour.
 */
void TrafRushHour(void)
{
    int32_t roads = 0;
    int32_t garages = 0;
    int32_t stations = 0;
    int32_t growing = 0;
    int32_t demand = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y];
            uint16_t id = tile & LOMASK;

            if (id >= ROADBASE && id <= LASTROAD) {
                roads++;
            } else if (id == GARAGE + 4) {
                garages++;
            } else if (id == STATION + 4) {
                stations++;
            } else if ((tile & ZONEBIT) && IsGrowingZone(id)) {
                growing++;
                if (ZoneHasRoadAccess(x, y)) demand++;
            }
        }
    }

    int32_t capacity = roads / TRAF_ROADS_PER_ZONE + garages * GARAGE_ZONES + stations * STATION_ZONES;
    int32_t excess = demand > capacity ? demand - capacity : 0;
    int64_t volume = (int64_t)TrafServicedResPop * TRAF_CITIZENS_PER_RESPOP * TRAF_COMMUTE_PERCENT / 100;
    int64_t full = (int64_t)capacity * TRAF_TRIPS_PER_ZONE;
    int32_t level = full > 0 ? (int32_t)(volume * 255 / full) : 0;
    if (level > 255) level = 255;

    int32_t rotor = demand > 0 ? TrafRotor % demand : 0;
    int32_t ordinal = 0;
    int32_t blocked = 0;
    bool roadFound = false;

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            TrfDensity[x][y] = 0;
        }
    }

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y];
            uint16_t id = tile & LOMASK;
            bool unserviced = false;

            if (id >= ROADBASE && id <= LASTROAD) {
                TrfDensity[x >> 1][y >> 1] = (Byte)level;
                if (!roadFound) {
                    TrafMaxX = (int16_t)(x << 4);
                    TrafMaxY = (int16_t)(y << 4);
                    roadFound = true;
                }
            } else if ((tile & ZONEBIT) && IsGrowingZone(id)) {
                if (!ZoneHasRoadAccess(x, y)) {
                    unserviced = true;
                } else {
                    unserviced = ((ordinal - rotor + demand) % demand) < excess;
                    ordinal++;
                }
            }

            TrafUnserviced[x][y] = unserviced ? 1 : 0;
            if (unserviced) blocked++;
        }
    }

    TrafRotor = demand > 0 ? (rotor + excess) % demand : 0;
    TrafProblemUnserviced = blocked;
    TrafProblemZones = growing;
}

bool TrafZoneUnserviced(int32_t x, int32_t y)
{
    return TestBounds(x, y) && TrafUnserviced[x][y] != 0;
}

float TrafServiceShare(void)
{
    if (ResPop <= 0) return 1.0f;
    float share = (float)TrafServicedResPop / (float)ResPop;
    return share > 1.0f ? 1.0f : share;
}

int32_t TrafProblemShare(void)
{
    if (TrafProblemZones <= 0) return 0;
    return (int32_t)((int64_t)TrafProblemUnserviced * 255 / TrafProblemZones);
}

bool RoadTest(int32_t tile)
{
    int32_t x = tile & LOMASK;
    if (x < ROADBASE) return false;
    if (x > LASTRAIL) return false;
    if ((x >= POWERBASE) && (x < RAILHPOWERV)) return false;
    return true;
}