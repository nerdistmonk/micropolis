/* s_neighbor.c: Four off-screen neighbor cities, one per map side.
 *
 * Each side has a random name, saved in the city file. The player links a side by placing a
 * Connect gate on the edge tile of that side, with the matching conductor directly inside it:
 *   - road or rail: a road or rail tile (one of each per side).
 *   - power: a power line tile.   - water: a water pipe tile.
 * A link is active only while its gate and conductor both exist, so bulldozing it breaks the link.
 *
 * Deals: zones > 0 buys from the neighbor, zones < 0 sells to it, measured in zones. Each month a
 * mid price follows a bounded random walk that reverts to its base. Prices are per zone; buying
 * costs the mid price plus NB_SPREAD_PCT, and selling pays the mid price minus NB_SPREAD_PCT.
 *   - Power and water buys feed the grid at the gate as a source (see power and water scans).
 *   - Sales draw load from the network they sell into: power and water at the gate's grid or
 *     network, garbage in the landfill. The load is never capped by spare capacity, so overselling
 *     leaves your own city short. The neighbor pays for the units drawn (see Neighbor_OfferSale),
 *     except power sold into a grid in blackout: the load still counts, but nothing is paid.
 *     A sale that cannot reach a network (no grid or water network at the gate) draws nothing.
 *   - Garbage buys add landfill capacity (see Neighbor_GarbageFeedZones), as power and water buys
 *     add supply.
 *   - Safe to sell: the zones that fit in the spare capacity of the network, shown in the deals UI.
 *
 * Land value: each active road or rail link adds NB_LV_PER_LINK percent, up to NB_LV_BONUS_MAX.
 */

#include <stdio.h>
#include <string.h>
#include "sim.h"

#define NB_LINK_TILE_NONE (-1)
#define NB_NAME_MAX       15
#define NB_LINK_ZONES     48   /* zones one link can carry */
#define NB_CONNECT_COST   1000
#define NB_DRIFT          4
#define NB_SPREAD_PCT     10
#define NB_LV_PER_LINK    1
#define NB_LV_BONUS_MAX   8

/* Save layout in MiscHis. Only slots 0-119 are written to the city file (see ParseCityBlob), so
 * everything lives there. Slots 0, 46-49 and 118-119 are otherwise unused. */
#define NB_SAVE_TAG       0x4E44
#define NB_SAVE_TAG_LEGACY 0x4E43   /* units in 4-bit nibbles, one slot per side */
#define NB_SAVE_TAG_SLOT  79
#define NB_SAVE_NAME_BASE 80   /* 4 sides x 8 slots, two chars per slot (80-111) */
#define NB_SAVE_GATE_BASE 18   /* 4 sides x 4 links, one slot each: (x << 8) | y, or -1 if none (18-33) */
#define NB_SAVE_MID_BASE  34   /* 4 sides x 3 goods (34-45) */
#define NB_SAVE_UNIT_BASE 112  /* 4 sides x 3 goods of zones, one byte each (zones + NB_ZONE_BIAS), two per slot: 112-117 */
#define NB_SAVE_DEALS_BASE 121 /* deals_flow since the last tax period, int32 hi/lo: 121-122 */
#define NB_NAME_SLOTS     8
#define NB_ZONE_BIAS      128
#define NB_LEGACY_UNIT_BIAS 8
#define NB_LEGACY_UNIT_ZONES 12

typedef struct {
    char name[NB_NAME_MAX + 1];
    int16_t gate_x[NB_LINK_COUNT];
    int16_t gate_y[NB_LINK_COUNT];
    int16_t mid[NB_GOODS_COUNT];
    int16_t units[NB_GOODS_COUNT];
} NeighborSide;

/* Base mid prices per unit per month. Provisional: tune with the user before release. */
static const int32_t NB_BASE_PRICE[NB_GOODS_COUNT] = { 120, 27, 11 };

static NeighborSide sides[NB_SIDES];
static int32_t SaleSafeUnits[NB_SIDES][NB_GOODS_COUNT];
static int32_t SaleDrawnUnits[NB_SIDES][NB_GOODS_COUNT];
static bool SaleBlackedOut[NB_SIDES][NB_GOODS_COUNT];
/* False from a units change until the next monthly sale pass records the sale. */
static bool SaleSettled[NB_SIDES][NB_GOODS_COUNT];

static const char *const NB_PREFIX[] = {
    "Bel", "Mar", "Oak", "Silver", "Haver", "Cedar", "Ash", "Lake", "Stone", "Glen", "Crest", "Fair"
};
static const char *const NB_SUFFIX[] = {
    "haven", "ford", "ton", "field", "brook", "view", "gate", "port", "wood", "mere"
};
#define NB_PREFIX_COUNT ((int)(sizeof(NB_PREFIX) / sizeof(NB_PREFIX[0])))
#define NB_SUFFIX_COUNT ((int)(sizeof(NB_SUFFIX) / sizeof(NB_SUFFIX[0])))

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
    return ConductorMatches(link, s->gate_x[link], s->gate_y[link]);
}

static int32_t ActiveRoadRailLinks(int32_t side)
{
    return (LinkActive(side, NB_LINK_ROAD) ? 1 : 0) + (LinkActive(side, NB_LINK_RAIL) ? 1 : 0);
}

static int32_t GoodsCap(int32_t side, int32_t goods)
{
    switch (goods) {
    case NB_GOODS_POWER:   return LinkActive(side, NB_LINK_POWER) ? NB_LINK_ZONES : 0;
    case NB_GOODS_WATER:   return LinkActive(side, NB_LINK_WATER) ? NB_LINK_ZONES : 0;
    case NB_GOODS_GARBAGE: return ActiveRoadRailLinks(side) * NB_LINK_ZONES;
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
    snprintf(sides[side].name, sizeof(sides[side].name), "%s", buf);
}

static int32_t deals_flow;

void Neighbor_Reset(void)
{
    deals_flow = 0;
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
    if (!ConductorMatches(link, x, y)) return false;
    sides[side].gate_x[link] = (int16_t)x;
    sides[side].gate_y[link] = (int16_t)y;
    return true;
}

static const char *const NB_LINK_NAME[NB_LINK_COUNT] = { "road", "rail", "power", "water" };

/* The link a Connect gate makes is the conductor on the edge tile. Rail is checked before road so
 * a rail-over-power tile links as rail, and road before power so a road-over-power tile links as road. */
static int32_t GateLink(int32_t x, int32_t y)
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
        int32_t link = GateLink(x, y);
        if (link == NB_LINK_TILE_NONE) continue;
        if (LinkActive(side, link)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Already connected: %s to %s", NB_LINK_NAME[link], sides[side].name);
            SetMessageField(msg, (int16_t)x, (int16_t)y, true);
            return false;
        }
        if (TotalFunds < NB_CONNECT_COST) {
            char cost_msg[96];
            snprintf(cost_msg, sizeof(cost_msg), "Not enough funds to connect (costs $%d)", NB_CONNECT_COST);
            SetMessageField(cost_msg, (int16_t)x, (int16_t)y, true);
            return false;
        }
        TotalFunds -= NB_CONNECT_COST;
        MustUpdateFunds = 1;
        Neighbor_BuildGate(side, link, x, y);
        char msg[256];
        snprintf(msg, sizeof(msg), "Connected %s to %s", NB_LINK_NAME[link], sides[side].name);
        SetMessageField(msg, (int16_t)x, (int16_t)y, true);
        return true;
    }
    SetMessageField(on_edge ? "Nothing to connect: put road, rail, power, or pipe on this edge tile"
                            : "Connect works on the map edge", (int16_t)x, (int16_t)y, true);
    return false;
}

void Neighbor_RemoveGate(int32_t side, int32_t link)
{
    if (side < 0 || side >= NB_SIDES || link < 0 || link >= NB_LINK_COUNT) return;
    sides[side].gate_x[link] = NB_LINK_TILE_NONE;
    sides[side].gate_y[link] = NB_LINK_TILE_NONE;
}

void Neighbor_PruneGates(void)
{
    for (int32_t s = 0; s < NB_SIDES; s++) {
        for (int32_t l = 0; l < NB_LINK_COUNT; l++) {
            if (sides[s].gate_x[l] == NB_LINK_TILE_NONE) continue;
            if (!ConductorMatches(l, sides[s].gate_x[l], sides[s].gate_y[l])) Neighbor_RemoveGate(s, l);
        }
    }
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
    SaleSettled[side][goods] = false;
}

/* Returns the grid seed for a power or water feed: the gate tile of an active link that is
 * buying. Only buys supply the grid. */
bool Neighbor_FeedSeed(int32_t side, int32_t goods, int32_t *x, int32_t *y)
{
    if (side < 0 || side >= NB_SIDES) return false;
    int32_t link = (goods == NB_GOODS_POWER) ? NB_LINK_POWER : (goods == NB_GOODS_WATER) ? NB_LINK_WATER : -1;
    if (link < 0 || sides[side].units[goods] <= 0 || !LinkActive(side, link)) return false;
    *x = sides[side].gate_x[link];
    *y = sides[side].gate_y[link];
    return true;
}

int32_t Neighbor_FeedZones(int32_t side, int32_t goods)
{
    return sides[side].units[goods] > 0 ? sides[side].units[goods] : 0;
}

bool Neighbor_RailTile(int32_t side, int32_t *x, int32_t *y)
{
    if (side < 0 || side >= NB_SIDES || !LinkActive(side, NB_LINK_RAIL)) return false;
    *x = sides[side].gate_x[NB_LINK_RAIL];
    *y = sides[side].gate_y[NB_LINK_RAIL];
    return true;
}

/* Gate tile of an active power or water link, for scans that settle that good's sales. */
bool Neighbor_GateTile(int32_t side, int32_t goods, int32_t *x, int32_t *y)
{
    if (side < 0 || side >= NB_SIDES) return false;
    int32_t link = (goods == NB_GOODS_POWER) ? NB_LINK_POWER : (goods == NB_GOODS_WATER) ? NB_LINK_WATER : -1;
    if (link < 0 || !LinkActive(side, link)) return false;
    *x = sides[side].gate_x[link];
    *y = sides[side].gate_y[link];
    return true;
}

void Neighbor_ClearSales(int32_t goods)
{
    for (int32_t s = 0; s < NB_SIDES; s++) {
        SaleSafeUnits[s][goods] = 0;
        SaleDrawnUnits[s][goods] = 0;
        SaleBlackedOut[s][goods] = false;
        SaleSettled[s][goods] = true;
    }
}

bool Neighbor_SaleSettled(int32_t side, int32_t goods)
{
    if (side < 0 || side >= NB_SIDES) return false;
    return SaleSettled[side][goods];
}

void Neighbor_BlackoutSale(int32_t side, int32_t goods)
{
    if (side < 0 || side >= NB_SIDES) return;
    SaleBlackedOut[side][goods] = true;
}

bool Neighbor_SaleBlackedOut(int32_t side, int32_t goods)
{
    if (side < 0 || side >= NB_SIDES) return false;
    return SaleBlackedOut[side][goods];
}

/* Money for zones at a per-zone quote. */
static int32_t ZoneCharge(int32_t zones, int32_t quote)
{
    return zones * quote;
}

int32_t Neighbor_BuyCharge(int32_t side, int32_t goods, int32_t zones)
{
    if (side < 0 || side >= NB_SIDES || zones <= 0) return 0;
    return ZoneCharge(zones, BuyPrice(side, goods));
}

int32_t Neighbor_SellEarn(int32_t side, int32_t goods, int32_t zones)
{
    if (side < 0 || side >= NB_SIDES || zones <= 0) return 0;
    return ZoneCharge(zones, SellPrice(side, goods));
}

/* Settles one side's sale into a network with spareZones of capacity left before this sale. The
 * sale draws its full zones, never capped by spare capacity. The safe zones (spare capacity, capped
 * by the link) are recorded for the deals display. Returns the zones the sale adds to demand, or 0
 * when the side is not selling this good over an active link. */
int32_t Neighbor_OfferSale(int32_t side, int32_t goods, int32_t spareZones)
{
    if (side < 0 || side >= NB_SIDES || sides[side].units[goods] >= 0) return 0;
    int32_t cap = GoodsCap(side, goods);
    if (cap <= 0) return 0;
    int32_t drawn = -sides[side].units[goods];
    if (drawn > cap) drawn = cap;
    int32_t safe = spareZones > 0 ? spareZones : 0;
    SaleSafeUnits[side][goods] = safe > cap ? cap : safe;
    SaleDrawnUnits[side][goods] = drawn;
    return drawn;
}

int32_t Neighbor_SafeSellUnits(int32_t side, int32_t goods)
{
    if (side < 0 || side >= NB_SIDES) return 0;
    return SaleSafeUnits[side][goods];
}

int32_t Neighbor_SoldUnits(int32_t side, int32_t goods)
{
    if (side < 0 || side >= NB_SIDES) return 0;
    return SaleDrawnUnits[side][goods];
}

/* Landfill capacity bought from neighbors, in zones, limited to what the active links can carry. */
int32_t Neighbor_GarbageFeedZones(void)
{
    int32_t zones = 0;
    for (int32_t s = 0; s < NB_SIDES; s++) {
        int32_t units = sides[s].units[NB_GOODS_GARBAGE];
        if (units <= 0) continue;
        int32_t cap = GoodsCap(s, NB_GOODS_GARBAGE);
        zones += units > cap ? cap : units;
    }
    return zones;
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
    Neighbor_PruneGates();
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
            int32_t cost = ZoneCharge(units, BuyPrice(s, g));
                if (TotalFunds < cost) {
                    sides[s].units[g] = 0;
                    continue;
                }
                TotalFunds -= cost;
                deals_flow -= cost;
                continue;
            }

            int32_t sold = SaleDrawnUnits[s][g];
            if (sold <= 0 || SaleBlackedOut[s][g]) continue;
            int32_t gain = ZoneCharge(sold, SellPrice(s, g));
            TotalFunds += gain;
            deals_flow += gain;
        }
    }
    if (TotalFunds < 0) TotalFunds = 0;
}

int32_t Neighbor_TakeDealsFlow(void)
{
    int32_t f = deals_flow;
    deals_flow = 0;
    return f;
}

static int16_t PackGate(int32_t side, int32_t link)
{
    if (sides[side].gate_x[link] == NB_LINK_TILE_NONE) return -1;
    return (int16_t)((sides[side].gate_x[link] << 8) | sides[side].gate_y[link]);
}

void Neighbor_SyncMisc(void)
{
    MiscHis[NB_SAVE_TAG_SLOT] = NB_SAVE_TAG;
    uint32_t df = (uint32_t)deals_flow;
    MiscHis[NB_SAVE_DEALS_BASE] = (int16_t)((df >> 16) & 0xFFFFu);
    MiscHis[NB_SAVE_DEALS_BASE + 1] = (int16_t)(df & 0xFFFFu);
    for (int32_t k = 0; k < NB_SIDES * NB_GOODS_COUNT / 2; k++) {
        MiscHis[NB_SAVE_UNIT_BASE + k] = 0;
    }
    for (int32_t s = 0; s < NB_SIDES; s++) {
        for (int32_t i = 0; i < NB_NAME_SLOTS; i++) {
            uint8_t c0 = (uint8_t)sides[s].name[2 * i];
            uint8_t c1 = (uint8_t)sides[s].name[2 * i + 1];
            MiscHis[NB_SAVE_NAME_BASE + s * NB_NAME_SLOTS + i] = (int16_t)((c0 << 8) | c1);
        }
        for (int32_t l = 0; l < NB_LINK_COUNT; l++) {
            MiscHis[NB_SAVE_GATE_BASE + s * NB_LINK_COUNT + l] = PackGate(s, l);
        }
        for (int32_t g = 0; g < NB_GOODS_COUNT; g++) {
            MiscHis[NB_SAVE_MID_BASE + s * NB_GOODS_COUNT + g] = sides[s].mid[g];
        }
    }
    for (int32_t k = 0; k < NB_SIDES * NB_GOODS_COUNT; k++) {
        int32_t s = k / NB_GOODS_COUNT;
        int32_t g = k % NB_GOODS_COUNT;
        int32_t zones = sides[s].units[g];
        if (zones > 2 * NB_LINK_ZONES) zones = 2 * NB_LINK_ZONES;
        if (zones < -2 * NB_LINK_ZONES) zones = -2 * NB_LINK_ZONES;
        uint16_t byte = (uint16_t)(zones + NB_ZONE_BIAS);
        int32_t slot = NB_SAVE_UNIT_BASE + k / 2;
        MiscHis[slot] = (int16_t)(((uint16_t)MiscHis[slot]) | (uint16_t)(byte << ((k % 2) * 8)));
    }
}

void Neighbor_LoadMisc(void)
{
    const int32_t tag = MiscHis[NB_SAVE_TAG_SLOT];
    if (tag != NB_SAVE_TAG && tag != NB_SAVE_TAG_LEGACY) {
        Neighbor_Reset();
        return;
    }
    const bool legacy = tag == NB_SAVE_TAG_LEGACY;
    if (legacy) {
        deals_flow = 0;
    } else {
        uint16_t d0 = (uint16_t)MiscHis[NB_SAVE_DEALS_BASE];
        uint16_t d1 = (uint16_t)MiscHis[NB_SAVE_DEALS_BASE + 1];
        deals_flow = (int32_t)(((uint32_t)d0 << 16) | d1);
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
            int32_t packed = MiscHis[NB_SAVE_GATE_BASE + s * NB_LINK_COUNT + l];
            int32_t gx = packed >> 8;
            int32_t gy = packed & 0xFF;
            if (packed >= 0 && TestBounds(gx, gy) && OnSideEdge(s, gx, gy)) {
                n->gate_x[l] = (int16_t)gx;
                n->gate_y[l] = (int16_t)gy;
            } else {
                n->gate_x[l] = NB_LINK_TILE_NONE;
                n->gate_y[l] = NB_LINK_TILE_NONE;
            }
        }
        uint16_t units_word = (uint16_t)MiscHis[NB_SAVE_UNIT_BASE + s];
        for (int32_t g = 0; g < NB_GOODS_COUNT; g++) {
            int32_t mid = MiscHis[NB_SAVE_MID_BASE + s * NB_GOODS_COUNT + g];
            if (mid < NB_BASE_PRICE[g] / 2 || mid > NB_BASE_PRICE[g] * 2) mid = NB_BASE_PRICE[g];
            n->mid[g] = (int16_t)mid;
            int32_t k = s * NB_GOODS_COUNT + g;
            int32_t zones;
            if (legacy) {
                int32_t units = (int32_t)((units_word >> (4 * g)) & 0xFu) - NB_LEGACY_UNIT_BIAS;
                zones = units * NB_LEGACY_UNIT_ZONES;
            } else {
                uint16_t word = (uint16_t)MiscHis[NB_SAVE_UNIT_BASE + k / 2];
                zones = (int32_t)((word >> ((k % 2) * 8)) & 0xFFu) - NB_ZONE_BIAS;
            }
            if (zones > 2 * NB_LINK_ZONES) zones = 2 * NB_LINK_ZONES;
            if (zones < -2 * NB_LINK_ZONES) zones = -2 * NB_LINK_ZONES;
            n->units[g] = (int16_t)zones;
        }
    }
}

