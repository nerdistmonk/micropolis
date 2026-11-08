/* s_neighbor.c: Four off-screen neighbor cities, one per map side.
 *
 * Each side has a random name, saved in the city file. The player links a side by placing a
 * Connect gate on the edge tile of that side, with the matching conductor directly inside it:
 *   - road or rail: a road or rail tile (one of each per side).
 *   - power: a power line tile.   - water: a water pipe tile.
 * A link is active only while its gate and conductor both exist, so bulldozing it breaks the link.
 *
 * Deals: units > 0 buys from the neighbor, units < 0 sells to it, in whole zone-sized units. Each
 * month a mid price follows a bounded random walk that reverts to its base. Buying costs the
 * mid price plus NB_SPREAD_PCT, and selling pays the mid price minus NB_SPREAD_PCT.
 *   - Power and water buys feed the grid at the gate as a source (see power and water scans).
 *   - Power and water sales are paid only from the grid's spare capacity at the gate.
 *   - Garbage buys clear unserviced trash; garbage sales add to it.
 *
 * Land value: each active road or rail link adds NB_LV_PER_LINK percent, up to NB_LV_BONUS_MAX.
 */

#include <stdio.h>
#include <string.h>
#include "sim.h"

#define NB_LINK_TILE_NONE (-1)
#define NB_NAME_MAX       15
#define NB_MAX_UNITS      4
#define NB_POWER_ZONES    12
#define NB_WATER_ZONES    12
#define NB_TRASH_PER_UNIT 200
#define NB_DRIFT          4
#define NB_SPREAD_PCT     10
#define NB_LV_PER_LINK    1
#define NB_LV_BONUS_MAX   8

/* Save layout in MiscHis (slots 79-167) */
#define NB_SAVE_TAG       0x4E42
#define NB_SAVE_TAG_SLOT  79
#define NB_SAVE_NAME_BASE 80   /* 4 sides x 8 slots, two chars per slot */
#define NB_SAVE_GATE_BASE 112  /* 4 sides x 4 links x (x, y) */
#define NB_SAVE_MID_BASE  144  /* 4 sides x 3 goods */
#define NB_SAVE_UNIT_BASE 156  /* 4 sides x 3 goods */
#define NB_NAME_SLOTS     8

typedef struct {
    char name[NB_NAME_MAX + 1];
    int16_t gate_x[NB_LINK_COUNT];
    int16_t gate_y[NB_LINK_COUNT];
    int16_t mid[NB_GOODS_COUNT];
    int16_t units[NB_GOODS_COUNT];
} NeighborSide;

/* Base mid prices per unit per month. Provisional: tune with the user before release. */
static const int32_t NB_BASE_PRICE[NB_GOODS_COUNT] = { 60, 40, 30 };

static NeighborSide sides[NB_SIDES];

static const char *const NB_PREFIX[] = {
    "Bel", "Mar", "Oak", "Silver", "Haver", "Cedar", "Ash", "Lake", "Stone", "Glen", "Crest", "Fair"
};
static const char *const NB_SUFFIX[] = {
    "haven", "ford", "ton", "field", "brook", "view", "gate", "port", "wood", "mere"
};
#define NB_PREFIX_COUNT ((int)(sizeof(NB_PREFIX) / sizeof(NB_PREFIX[0])))
#define NB_SUFFIX_COUNT ((int)(sizeof(NB_SUFFIX) / sizeof(NB_SUFFIX[0])))

/* Inward step from the edge tile for each side: N, E, S, W. */
static const int32_t NB_INWARD_DX[NB_SIDES] = { 0, -1, 0, 1 };
static const int32_t NB_INWARD_DY[NB_SIDES] = { 1, 0, -1, 0 };

static bool OnSideEdge(int32_t side, int32_t x, int32_t y)
{
    switch (side) {
    case NB_NORTH: return y == 0;
    case NB_EAST:  return x == WORLD_X - 1;
    case NB_SOUTH: return y == WORLD_Y - 1;
    case NB_WEST:  return x == 0;
    default:       return false;
    }
}

static bool InsideTile(int32_t side, int32_t gx, int32_t gy, int32_t *ix, int32_t *iy)
{
    *ix = gx + NB_INWARD_DX[side];
    *iy = gy + NB_INWARD_DY[side];
    return TestBounds(*ix, *iy);
}

static bool ConductorMatches(int32_t link, int32_t x, int32_t y)
{
    uint16_t t = Map[x][y] & LOMASK;
    switch (link) {
    case NB_LINK_ROAD:
        return t >= ROADBASE && t <= LASTROAD;
    case NB_LINK_RAIL:
        return (t >= RAILBASE && t <= LASTRAIL) || t == RAILHPOWERV || t == RAILVPOWERH;
    case NB_LINK_POWER:
        return (t >= POWERBASE && t <= LASTPOWER) || t == HROADPOWER || t == VROADPOWER
            || t == ROADVPOWERH;
    case NB_LINK_WATER:
        return WaterHasPipe(x, y);
    default:
        return false;
    }
}

static bool LinkActive(int32_t side, int32_t link)
{
    NeighborSide *s = &sides[side];
    if (s->gate_x[link] == NB_LINK_TILE_NONE) return false;
    int32_t ix, iy;
    if (!InsideTile(side, s->gate_x[link], s->gate_y[link], &ix, &iy)) return false;
    return ConductorMatches(link, ix, iy);
}

static int32_t ActiveRoadRailLinks(int32_t side)
{
    return (LinkActive(side, NB_LINK_ROAD) ? 1 : 0) + (LinkActive(side, NB_LINK_RAIL) ? 1 : 0);
}

static int32_t GoodsCap(int32_t side, int32_t goods)
{
    switch (goods) {
    case NB_GOODS_POWER:   return LinkActive(side, NB_LINK_POWER) ? NB_MAX_UNITS : 0;
    case NB_GOODS_WATER:   return LinkActive(side, NB_LINK_WATER) ? NB_MAX_UNITS : 0;
    case NB_GOODS_GARBAGE: return ActiveRoadRailLinks(side) * NB_MAX_UNITS;
    default:               return 0;
    }
}

static int32_t BuyPrice(int32_t side, int32_t goods)
{
    return (sides[side].mid[goods] * (100 + NB_SPREAD_PCT)) / 100;
}

static int32_t SellPrice(int32_t side, int32_t goods)
{
    return (sides[side].mid[goods] * (100 - NB_SPREAD_PCT)) / 100;
}

static void PickName(int32_t side)
{
    char buf[NB_NAME_MAX + 1];
    for (int32_t attempt = 0; attempt < 32; attempt++) {
        const char *p = NB_PREFIX[Rand(NB_PREFIX_COUNT - 1)];
        const char *q = NB_SUFFIX[Rand(NB_SUFFIX_COUNT - 1)];
        snprintf(buf, sizeof(buf), "%s%s", p, q);
        bool clash = false;
        for (int32_t o = 0; o < side; o++) {
            if (strcmp(sides[o].name, buf) == 0) clash = true;
        }
        if (!clash) break;
    }
    strncpy(sides[side].name, buf, NB_NAME_MAX);
    sides[side].name[NB_NAME_MAX] = '\0';
}

void Neighbor_Reset(void)
{
    for (int32_t s = 0; s < NB_SIDES; s++) {
        NeighborSide *n = &sides[s];
        memset(n->name, 0, sizeof(n->name));
        for (int32_t l = 0; l < NB_LINK_COUNT; l++) {
            n->gate_x[l] = NB_LINK_TILE_NONE;
            n->gate_y[l] = NB_LINK_TILE_NONE;
        }
        for (int32_t g = 0; g < NB_GOODS_COUNT; g++) {
            n->mid[g] = (int16_t)NB_BASE_PRICE[g];
            n->units[g] = 0;
        }
    }
    for (int32_t s = 0; s < NB_SIDES; s++) {
        PickName(s);
    }
}

const char *Neighbor_Name(int32_t side)
{
    return (side >= 0 && side < NB_SIDES) ? sides[side].name : "";
}

bool Neighbor_IsLinked(int32_t side, int32_t link)
{
    return side >= 0 && side < NB_SIDES && link >= 0 && link < NB_LINK_COUNT && LinkActive(side, link);
}

bool Neighbor_BuildGate(int32_t side, int32_t link, int32_t x, int32_t y)
{
    if (side < 0 || side >= NB_SIDES || link < 0 || link >= NB_LINK_COUNT) return false;
    if (!TestBounds(x, y) || !OnSideEdge(side, x, y)) return false;
    int32_t ix, iy;
    if (!InsideTile(side, x, y, &ix, &iy)) return false;
    if (!ConductorMatches(link, ix, iy)) return false;
    sides[side].gate_x[link] = (int16_t)x;
    sides[side].gate_y[link] = (int16_t)y;
    return true;
}

static const char *const NB_LINK_NAME[NB_LINK_COUNT] = { "road", "rail", "power", "water" };

/* The link a Connect gate makes is whichever conductor lies inward. Rail is checked before road so
 * a rail-over-power tile links as rail, and road before power so a road-over-power tile links as road. */
static int32_t InwardLink(int32_t x, int32_t y)
{
    if (ConductorMatches(NB_LINK_RAIL, x, y)) return NB_LINK_RAIL;
    if (ConductorMatches(NB_LINK_ROAD, x, y)) return NB_LINK_ROAD;
    if (ConductorMatches(NB_LINK_POWER, x, y)) return NB_LINK_POWER;
    if (ConductorMatches(NB_LINK_WATER, x, y)) return NB_LINK_WATER;
    return NB_LINK_TILE_NONE;
}

bool Neighbor_Connect(int32_t x, int32_t y)
{
    bool on_edge = false;
    for (int32_t side = 0; side < NB_SIDES; side++) {
        if (!OnSideEdge(side, x, y)) continue;
        on_edge = true;
        int32_t ix, iy;
        if (!InsideTile(side, x, y, &ix, &iy)) continue;
        int32_t link = InwardLink(ix, iy);
        if (link == NB_LINK_TILE_NONE) continue;
        Neighbor_BuildGate(side, link, x, y);
        char msg[256];
        snprintf(msg, sizeof(msg), "Connected %s to %s", NB_LINK_NAME[link], sides[side].name);
        SetMessageField(msg, (int16_t)x, (int16_t)y, true);
        return true;
    }
    SetMessageField(on_edge ? "Nothing to connect: put road, rail, power, or pipe just inside the edge"
                            : "Connect works on the map edge", (int16_t)x, (int16_t)y, true);
    return false;
}

void Neighbor_RemoveGate(int32_t side, int32_t link)
{
    if (side < 0 || side >= NB_SIDES || link < 0 || link >= NB_LINK_COUNT) return;
    sides[side].gate_x[link] = NB_LINK_TILE_NONE;
    sides[side].gate_y[link] = NB_LINK_TILE_NONE;
}

int32_t Neighbor_Units(int32_t side, int32_t goods)
{
    return sides[side].units[goods];
}

int32_t Neighbor_Cap(int32_t side, int32_t goods)
{
    return GoodsCap(side, goods);
}

int32_t Neighbor_MidPrice(int32_t side, int32_t goods)
{
    return sides[side].mid[goods];
}

int32_t Neighbor_BuyCost(int32_t side, int32_t goods)
{
    return BuyPrice(side, goods);
}

int32_t Neighbor_SellPrice(int32_t side, int32_t goods)
{
    return SellPrice(side, goods);
}

void Neighbor_SetUnits(int32_t side, int32_t goods, int32_t units)
{
    int32_t cap = GoodsCap(side, goods);
    if (units > cap) units = cap;
    if (units < -cap) units = -cap;
    sides[side].units[goods] = (int16_t)units;
}

/* Returns the grid seed for a power or water feed: the inside tile of an active link that is
 * buying. Only buys supply the grid. */
bool Neighbor_FeedSeed(int32_t side, int32_t goods, int32_t *x, int32_t *y)
{
    if (side < 0 || side >= NB_SIDES) return false;
    int32_t link = (goods == NB_GOODS_POWER) ? NB_LINK_POWER : (goods == NB_GOODS_WATER) ? NB_LINK_WATER : -1;
    if (link < 0 || sides[side].units[goods] <= 0 || !LinkActive(side, link)) return false;
    return InsideTile(side, sides[side].gate_x[link], sides[side].gate_y[link], x, y);
}

int32_t Neighbor_FeedZones(int32_t goods)
{
    return goods == NB_GOODS_POWER ? NB_POWER_ZONES : NB_WATER_ZONES;
}

bool Neighbor_RailTile(int32_t side, int32_t *x, int32_t *y)
{
    if (side < 0 || side >= NB_SIDES || !LinkActive(side, NB_LINK_RAIL)) return false;
    return InsideTile(side, sides[side].gate_x[NB_LINK_RAIL], sides[side].gate_y[NB_LINK_RAIL], x, y);
}

int32_t Neighbor_FeedUnits(int32_t side, int32_t goods)
{
    return sides[side].units[goods] > 0 ? sides[side].units[goods] : 0;
}

int32_t Neighbor_LandValueBonusPercent(void)
{
    int32_t total = 0;
    for (int32_t s = 0; s < NB_SIDES; s++) {
        total += ActiveRoadRailLinks(s) * NB_LV_PER_LINK;
    }
    return total > NB_LV_BONUS_MAX ? NB_LV_BONUS_MAX : total;
}

static void DriftMid(int32_t side, int32_t goods)
{
    int32_t base = NB_BASE_PRICE[goods];
    int32_t mid = sides[side].mid[goods];
    mid += Rand(2 * NB_DRIFT) - NB_DRIFT;
    mid += (base - mid) / 8;
    if (mid < base / 2) mid = base / 2;
    if (mid > base * 2) mid = base * 2;
    sides[side].mid[goods] = (int16_t)mid;
}

/* Settles one month of deals. Called from the simulation tick; runs once every 4 ticks. */
void Neighbor_Tick(void)
{
    if (CityTime & 3) return;

    for (int32_t s = 0; s < NB_SIDES; s++) {
        for (int32_t g = 0; g < NB_GOODS_COUNT; g++) {
            DriftMid(s, g);
        }
    }

    for (int32_t s = 0; s < NB_SIDES; s++) {
        for (int32_t g = 0; g < NB_GOODS_COUNT; g++) {
            int32_t units = sides[s].units[g];
            if (units == 0) continue;
            int32_t cap = GoodsCap(s, g);
            if (units > cap) units = cap;
            if (units < -cap) units = -cap;

            if (units > 0) {
                int32_t cost = units * BuyPrice(s, g);
                if (TotalFunds < cost) {
                    sides[s].units[g] = 0;
                    continue;
                }
                TotalFunds -= cost;
                if (g == NB_GOODS_GARBAGE) {
                    int32_t clear = units * NB_TRASH_PER_UNIT;
                    if (clear > UnservicedTrash) clear = UnservicedTrash;
                    UnservicedTrash -= clear;
                }
                continue;
            }

            int32_t sold = -units;
            if (g == NB_GOODS_GARBAGE) {
                UnservicedTrash += sold * NB_TRASH_PER_UNIT;
            } else {
                int32_t spare = 0;
                if (g == NB_GOODS_POWER && LinkActive(s, NB_LINK_POWER)) {
                    int32_t ix, iy;
                    if (InsideTile(s, sides[s].gate_x[NB_LINK_POWER], sides[s].gate_y[NB_LINK_POWER], &ix, &iy)) {
                        int32_t dem = 0, cap_z = 0;
                        if (PowerGridStats(ix, iy, &dem, &cap_z) && cap_z > dem) {
                            spare = (cap_z - dem) / NB_POWER_ZONES;
                        }
                    }
                } else if (g == NB_GOODS_WATER && LinkActive(s, NB_LINK_WATER)) {
                    int32_t ix, iy;
                    if (InsideTile(s, sides[s].gate_x[NB_LINK_WATER], sides[s].gate_y[NB_LINK_WATER], &ix, &iy)) {
                        int32_t dem = 0, cap_z = 0;
                        if (WaterNetworkStats(ix, iy, &dem, &cap_z) && cap_z > dem) {
                            spare = (cap_z - dem) / NB_WATER_ZONES;
                        }
                    }
                }
                if (sold > spare) sold = spare;
                if (sold <= 0) continue;
            }
            TotalFunds += sold * SellPrice(s, g);
        }
    }
    if (TotalFunds < 0) TotalFunds = 0;
}

void Neighbor_SyncMisc(void)
{
    MiscHis[NB_SAVE_TAG_SLOT] = NB_SAVE_TAG;
    for (int32_t s = 0; s < NB_SIDES; s++) {
        for (int32_t i = 0; i < NB_NAME_SLOTS; i++) {
            uint8_t c0 = (uint8_t)sides[s].name[2 * i];
            uint8_t c1 = (uint8_t)sides[s].name[2 * i + 1];
            MiscHis[NB_SAVE_NAME_BASE + s * NB_NAME_SLOTS + i] = (int16_t)((c0 << 8) | c1);
        }
        for (int32_t l = 0; l < NB_LINK_COUNT; l++) {
            int32_t base = NB_SAVE_GATE_BASE + (s * NB_LINK_COUNT + l) * 2;
            MiscHis[base] = sides[s].gate_x[l];
            MiscHis[base + 1] = sides[s].gate_y[l];
        }
        for (int32_t g = 0; g < NB_GOODS_COUNT; g++) {
            MiscHis[NB_SAVE_MID_BASE + s * NB_GOODS_COUNT + g] = sides[s].mid[g];
            MiscHis[NB_SAVE_UNIT_BASE + s * NB_GOODS_COUNT + g] = sides[s].units[g];
        }
    }
}

void Neighbor_LoadMisc(void)
{
    if (MiscHis[NB_SAVE_TAG_SLOT] != NB_SAVE_TAG) {
        Neighbor_Reset();
        return;
    }
    for (int32_t s = 0; s < NB_SIDES; s++) {
        NeighborSide *n = &sides[s];
        for (int32_t i = 0; i < NB_NAME_SLOTS; i++) {
            uint16_t v = (uint16_t)MiscHis[NB_SAVE_NAME_BASE + s * NB_NAME_SLOTS + i];
            n->name[2 * i] = (char)(v >> 8);
            n->name[2 * i + 1] = (char)(v & 0xFFu);
        }
        n->name[NB_NAME_MAX] = '\0';
        if (n->name[0] == '\0') PickName(s);

        for (int32_t l = 0; l < NB_LINK_COUNT; l++) {
            int32_t base = NB_SAVE_GATE_BASE + (s * NB_LINK_COUNT + l) * 2;
            int32_t gx = MiscHis[base];
            int32_t gy = MiscHis[base + 1];
            if (TestBounds(gx, gy) && OnSideEdge(s, gx, gy)) {
                n->gate_x[l] = (int16_t)gx;
                n->gate_y[l] = (int16_t)gy;
            } else {
                n->gate_x[l] = NB_LINK_TILE_NONE;
                n->gate_y[l] = NB_LINK_TILE_NONE;
            }
        }
        for (int32_t g = 0; g < NB_GOODS_COUNT; g++) {
            int32_t mid = MiscHis[NB_SAVE_MID_BASE + s * NB_GOODS_COUNT + g];
            if (mid < NB_BASE_PRICE[g] / 2 || mid > NB_BASE_PRICE[g] * 2) mid = NB_BASE_PRICE[g];
            n->mid[g] = (int16_t)mid;
            n->units[g] = MiscHis[NB_SAVE_UNIT_BASE + s * NB_GOODS_COUNT + g];
        }
    }
}
