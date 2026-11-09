/* s_traf.c: Road capacity traffic model.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 *
 * The road network is one pooled entity. Each road tile adds a tenth of a zone of
 * capacity, and each parking garage or passenger station adds more. Every zone and
 * placeable building adds demand. Up to TRAF_ABSORBED_ZONES over capacity only congests traffic.
 * Beyond that, road-connected growing zones go unserviced, and that set rotates at each rush hour.
 * Other buildings keep working. Road pollution follows the congestion tier: negligible below
 * capacity, moderate at capacity, and worst once gridlocked.
 */

#include "sim.h"

/* Rush hour volume: 20 citizens per ResPop, 60% commuting, and a full zone is 40 ResPop */
#define TRAF_CITIZENS_PER_RESPOP 20
#define TRAF_COMMUTE_PERCENT     60
#define TRAF_TRIPS_PER_ZONE      480

/* Zones over capacity that only congest traffic. Gridlock starts one zone past them. */
#define TRAF_ABSORBED_ZONES      3
#define TRAF_GRIDLOCK_ZONES      (TRAF_ABSORBED_ZONES + 1)

/* AnimateTiles passes per traffic animation step: fastest with spare capacity, slowest at capacity and beyond. */
#define TRAF_ANIM_FASTEST        1
#define TRAF_ANIM_SLOWEST        8

/* Pollution per traffic road tile, by congestion tier. */
#define TRAF_POLLUTION_NEGLIGIBLE 2
#define TRAF_POLLUTION_MODERATE   25
#define TRAF_POLLUTION_WORST      50

int16_t TrafMaxX = 0;
int16_t TrafMaxY = 0;
int32_t TrafServicedResPop = 0;

static uint8_t TrafUnserviced[WORLD_X][WORLD_Y];
static int32_t TrafRotor = 0;
static int32_t TrafProblemUnserviced = 0;
static int32_t TrafProblemZones = 0;
/* Rush-hour load on the road network, percent of capacity, 0-100 */
static int16_t TrafLoad = 0;
/* True when at least one serviced resident commutes. Load can round to 0% while residents still commute. */
static bool TrafHasCommuters = false;
/* Demand and capacity at the last rush hour, in zones. */
static int32_t TrafDemandZones = 0;
static int32_t TrafCapacityZones = 0;
/* Counts rush hours in this city. Police, fire and hospital coverage rotates when it changes. */
static int32_t TrafRushCount = 0;
/* Traffic tiles advance on about 1 in TrafAnimEvery AnimateTiles passes, each tile rolling on its own. */
static int32_t TrafAnimEvery = TRAF_ANIM_FASTEST;
static uint32_t TrafAnimPass = 0;

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

/* One pass over the map. Each garage and station is one building. It adds capacity only with road or rail access, power, and water. */
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
                t->buildings++;
                if (CapacityBlockHasAccess(x, y) && CapacityBlockPowered(x, y) && CapacityBlockWatered(x, y)) t->garages++;
            } else if (id == STATION + 4) {
                t->buildings++;
                if (CapacityBlockHasAccess(x, y) && CapacityBlockPowered(x, y) && CapacityBlockWatered(x, y)) t->stations++;
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

/* Passes between traffic animation steps: fastest with spare capacity, slowest from capacity onward. */
static int32_t TrafAnimInterval(int32_t demand, int32_t capacity)
{
    if (demand <= 0) return TRAF_ANIM_FASTEST;
    if (capacity <= 0) return TRAF_ANIM_SLOWEST;
    int64_t pct = (int64_t)demand * 100 / capacity;
    if (pct > 100) pct = 100;
    return TRAF_ANIM_FASTEST + (int32_t)((TRAF_ANIM_SLOWEST - TRAF_ANIM_FASTEST) * pct / 100);
}

/* Runs at each rush hour. The road grid is one pool: its capacity is the share of
 * road tiles plus the garages and stations that have road or rail access. Demand is
 * every growing zone plus every placeable building. Excess beyond TRAF_ABSORBED_ZONES
 * is cut from road-connected growing zones, and the cut window rotates at each rush hour.
 */
void TrafRushHour(void)
{
    TrafRushCount++;
    TrafTally t;
    TrafCountNetwork(&t);
    int32_t growing = t.growing;
    int32_t accessible = t.accessible;
    int32_t buildings = t.buildings;
    int32_t demand = growing + buildings;
    int32_t capacity = TrafCapacityOf(&t);
    int32_t excess = demand > capacity ? demand - capacity : 0;
    int32_t absorbed = excess > TRAF_ABSORBED_ZONES ? excess - TRAF_ABSORBED_ZONES : 0;
    int32_t cut = absorbed < accessible ? absorbed : accessible;
    int64_t volume = (int64_t)TrafServicedResPop * TRAF_CITIZENS_PER_RESPOP * TRAF_COMMUTE_PERCENT / 100;
    int64_t full = (int64_t)capacity * TRAF_TRIPS_PER_ZONE;
    int32_t level = full > 0 ? (int32_t)(volume * 255 / full) : 0;
    if (level > 255) level = 255;
    int64_t load = full > 0 ? volume * 100 / full : 0;
    TrafLoad = (int16_t)(load > 100 ? 100 : load);
    TrafHasCommuters = volume > 0;
    TrafDemandZones = demand;
    TrafCapacityZones = capacity;
    TrafAnimEvery = TrafAnimInterval(demand, capacity);

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
    TrafHasCommuters = false;
    TrafDemandZones = 0;
    TrafCapacityZones = 0;
    TrafRushCount = 0;
    TrafAnimEvery = TRAF_ANIM_FASTEST;
}

int32_t TrafRushHourCount(void)
{
    return TrafRushCount;
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

/* Demand has reached road capacity. Traffic is jammed, though no growing zone is cut yet. */
bool TrafAtCapacity(void)
{
    return TrafDemandZones > 0 && TrafDemandZones >= TrafCapacityZones;
}

bool TrafGridlocked(void)
{
    return TrafDemandZones - TrafCapacityZones >= TRAF_GRIDLOCK_ZONES;
}

/* City traffic state, using the same thresholds as the road tiers. Rail trains follow it. */
int32_t TrafState(void)
{
    if (TrafGridlocked()) return TRAF_OVERLOADED;
    if (TrafAtCapacity()) return TRAF_CONGESTED;
    if (2 * TrafDemandZones >= TrafCapacityZones) return TRAF_NORMAL;
    return TRAF_LIGHT;
}

bool TrafCommuting(void)
{
    return TrafHasCommuters;
}

/* Pollution tier for traffic road tiles. Gridlock, past the absorbed zones, is the over-capacity tier. */
int32_t TrafRoadPollution(void)
{
    if (TrafGridlocked()) return TRAF_POLLUTION_WORST;
    if (TrafAtCapacity()) return TRAF_POLLUTION_MODERATE;
    return TRAF_POLLUTION_NEGLIGIBLE;
}

/* Garage tiles pollute like an uncongested road tile, whatever the network load. */
int32_t TrafUncongestedPollution(void)
{
    return TRAF_POLLUTION_NEGLIGIBLE;
}

/* Called once per AnimateTiles pass, before the map scan. */
void TrafAnimBeginPass(void)
{
    TrafAnimPass++;
}

/* Roving gaps: a run of 3 to 5 consecutive road tiles holds still. Each road line gets its own pattern,
 * and the pattern drifts one tile every TRAF_GAP_ROVE passes. */
#define TRAF_GAP_PERIOD  20
#define TRAF_GAP_ROVE    4

static uint32_t TrafMix(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u + b * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0xC2B2AE3Du;
    h ^= h >> 13;
    return h;
}

static bool TrafRoadAt(int32_t x, int32_t y)
{
    return x >= 0 && y >= 0 && x < WORLD_X && y < WORLD_Y && RoadTest(Map[x][y]);
}

/* A tile belongs to an east-west line when a road links it east or west, otherwise to a north-south line. */
static bool TrafInRovingGap(int32_t x, int32_t y)
{
    bool eastWest = TrafRoadAt(x - 1, y) || TrafRoadAt(x + 1, y);
    bool northSouth = TrafRoadAt(x, y - 1) || TrafRoadAt(x, y + 1);
    bool horizontal = eastWest || !northSouth;
    uint32_t along = (uint32_t)(horizontal ? x : y);
    uint32_t line = (uint32_t)(horizontal ? y : x);
    uint32_t q = along + TrafAnimPass / TRAF_GAP_ROVE;
    uint32_t r = q % TRAF_GAP_PERIOD;
    uint32_t g = TrafMix(line, q / TRAF_GAP_PERIOD);
    if (g % 3 == 0) return false;
    uint32_t len = 3 + (g / 3) % 3;
    uint32_t start = (g / 9) % (TRAF_GAP_PERIOD - len + 1);
    return r >= start && r < start + len;
}

/* True when traffic tile (x, y) advances this pass. The hash mixes the tile and pass, so tiles drift out of step.
 * Gridlocked roads never advance, so the packed cars stay still. */
bool TrafAnimTileDue(int32_t x, int32_t y)
{
    if (TrafGridlocked()) return false;
    if (TrafInRovingGap(x, y)) return false;
    uint32_t h = (uint32_t)x * 0x9E3779B1u + (uint32_t)y * 0x85EBCA77u + TrafAnimPass * 0xC2B2AE3Du;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h % (uint32_t)TrafAnimEvery == 0;
}

/* Share of road tiles carrying traffic in each non-gridlocked state. */
#define TRAF_SHARE_LIGHT      25
#define TRAF_SHARE_NORMAL     50
#define TRAF_SHARE_CONGESTED  75

/* Traffic tier for road tile (x, y): 0 empty, 1 light, 2 heavy. Gridlock packs every road. Otherwise the
 * share of roads given by the state (light, normal, congested) carries traffic. A road that is busy at one
 * load stays busy at higher loads. */
int32_t TrafRoadDensity(int32_t x, int32_t y)
{
    if (TrafGridlocked()) return 2;
    if (!TrafHasCommuters) return 0;
    int32_t share = TRAF_SHARE_LIGHT;
    if (TrafDemandZones >= TrafCapacityZones) {
        share = TRAF_SHARE_CONGESTED;
    } else if (2 * TrafDemandZones >= TrafCapacityZones) {
        share = TRAF_SHARE_NORMAL;
    }
    if ((int32_t)(TrafMix((uint32_t)x, (uint32_t)y) % 100u) >= share) return 0;
    int32_t level = TrfDensity[x >> 1][y >> 1];
    return level >= 192 ? 2 : 1;
}

bool RoadTest(int32_t tile)
{
    int32_t x = tile & LOMASK;
    if (x < ROADBASE) return false;
    if (x > LASTRAIL) return false;
    if ((x >= POWERBASE) && (x < RAILHPOWERV)) return false;
    return true;
}