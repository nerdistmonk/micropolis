/* s_traf.c: Road capacity traffic model.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 *
 * The road network is one pooled entity. Each road tile adds a tenth of a zone of
 * capacity, and each parking garage or passenger station adds more. Every zone and
 * placeable building adds demand. Road-connected growing zones absorb the excess as
 * unserviced zones, and that set rotates at each rush hour. Other buildings keep working.
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
/* Rush-hour load on the road network, percent of capacity, 0-100 */
static int16_t TrafLoad = 0;

/* Fire, police, hospitals, power plants, ports, airports, stadiums, landfill, water towers, and parks.
 * Each building counts once: at its zone centre, landfill centre, water tower centre, or park tile.
 */
static bool IsPlaceableBuilding(uint16_t tile)
{
    uint16_t id = tile & LOMASK;
    if (tile & ZONEBIT) return !IsGrowingZone(id);
    if (id == WATERTOWER + 4) return true;
    if (id >= FOUNTAIN && id < FOUNTAIN + FOUNTAIN_FRAMES) return true;
    return id >= LANDFILL && id < LANDFILL + LANDFILL_TILES &&
           ((id - LANDFILL) % LANDFILL_STAGE_TILES) == LANDFILL_CENTRE_OFFSET;
}

/* One pass over the map. Garages and stations count only with the access they need. */
typedef struct {
    int32_t roads;
    int32_t garages;
    int32_t stations;
    int32_t growing;
    int32_t accessible;
    int32_t buildings;
} TrafTally;

static void TrafCountNetwork(TrafTally *t)
{
    memset(t, 0, sizeof *t);
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t tile = Map[x][y];
            uint16_t id = tile & LOMASK;

            if (id >= ROADBASE && id <= LASTROAD) {
                t->roads++;
            } else if (id == GARAGE + 4) {
                if (CapacityBlockHasAccess(x, y)) t->garages++;
            } else if (id == STATION + 4) {
                if (CapacityBlockHasAccess(x, y)) t->stations++;
            } else if ((tile & ZONEBIT) && IsGrowingZone(id)) {
                t->growing++;
                if (ZoneHasRoadAccess(x, y)) t->accessible++;
            } else if (IsPlaceableBuilding(tile)) {
                t->buildings++;
            }
        }
    }
}

static int32_t TrafCapacityOf(const TrafTally *t)
{
    return t->roads / TRAF_ROADS_PER_ZONE + t->garages * GARAGE_ZONES + t->stations * STATION_ZONES;
}

/* Current demand and capacity of the pooled road network, in zones. */
void TrafNetworkStats(int32_t *demand, int32_t *capacity)
{
    TrafTally t;
    TrafCountNetwork(&t);
    *demand = t.growing + t.buildings;
    *capacity = TrafCapacityOf(&t);
}

/* Runs at each rush hour. The road grid is one pool: its capacity is the share of
 * road tiles plus the garages and stations that have road or rail access. Demand is
 * every growing zone plus every placeable building. The excess over capacity is cut
 * from road-connected growing zones, and the cut window rotates at each rush hour.
 */
void TrafRushHour(void)
{
    TrafTally t;
    TrafCountNetwork(&t);
    int32_t growing = t.growing;
    int32_t accessible = t.accessible;
    int32_t buildings = t.buildings;
    int32_t demand = growing + buildings;
    int32_t capacity = TrafCapacityOf(&t);
    int32_t excess = demand > capacity ? demand - capacity : 0;
    int32_t cut = excess < accessible ? excess : accessible;
    int64_t volume = (int64_t)TrafServicedResPop * TRAF_CITIZENS_PER_RESPOP * TRAF_COMMUTE_PERCENT / 100;
    int64_t full = (int64_t)capacity * TRAF_TRIPS_PER_ZONE;
    int32_t level = full > 0 ? (int32_t)(volume * 255 / full) : 0;
    if (level > 255) level = 255;
    int64_t load = full > 0 ? volume * 100 / full : 0;
    TrafLoad = (int16_t)(load > 100 ? 100 : load);

    int32_t rotor = accessible > 0 ? TrafRotor % accessible : 0;
    int32_t ordinal = 0;
    int32_t blocked = 0;
    bool roadFound = false;
    bool jamFound = false;
    int16_t roadX = 0, roadY = 0;
    int16_t jamX = 0, jamY = 0;

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
                    roadX = (int16_t)x;
                    roadY = (int16_t)y;
                    roadFound = true;
                }
            } else if ((tile & ZONEBIT) && IsGrowingZone(id)) {
                if (!ZoneHasRoadAccess(x, y)) {
                    unserviced = true;
                } else {
                    unserviced = ((ordinal - rotor + accessible) % accessible) < cut;
                    ordinal++;
                    if (unserviced && !jamFound) {
                        jamX = (int16_t)x;
                        jamY = (int16_t)y;
                        jamFound = true;
                    }
                }
            }

            TrafUnserviced[x][y] = unserviced ? 1 : 0;
            if (unserviced) blocked++;
        }
    }

    TrafRotor = accessible > 0 ? (rotor + cut) % accessible : 0;
    TrafProblemUnserviced = blocked;
    TrafProblemZones = growing;
    TrafMaxX = jamFound ? jamX : roadX;
    TrafMaxY = jamFound ? jamY : roadY;
}

/* Clears rush-hour state so a new or loaded city does not inherit the previous city's jams. */
void TrafResetCity(void)
{
    memset(TrafUnserviced, 0, sizeof TrafUnserviced);
    TrafRotor = 0;
    TrafProblemUnserviced = 0;
    TrafProblemZones = 0;
    TrafMaxX = 0;
    TrafMaxY = 0;
    TrafServicedResPop = 0;
    TrafLoad = 0;
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

int16_t TrafRoadLoad(void)
{
    return TrafLoad;
}

bool RoadTest(int32_t tile)
{
    int32_t x = tile & LOMASK;
    if (x < ROADBASE) return false;
    if (x > LASTRAIL) return false;
    if ((x >= POWERBASE) && (x < RAILHPOWERV)) return false;
    return true;
}