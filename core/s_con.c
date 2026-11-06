/* s_con.c: Auto-tiling connection rules, bridges, zone collision/bulldoze, and audio integration.
 *
 * Modernized C11 implementation ported from Micropolis w_con.c and w_tool.c.
 */

#include "sim.h"

extern void Audio_MakeSound(const char *channel, const char *sound_name);

static const int16_t RoadTable[16] = {
    66, 67, 66, 68,
    67, 67, 69, 73,
    66, 71, 66, 72,
    70, 75, 74, 76
};

static const int16_t RailTable[16] = {
    226, 227, 226, 228,
    227, 227, 229, 233,
    226, 231, 226, 232,
    230, 235, 234, 236
};

static const int16_t WireTable[16] = {
    210, 211, 210, 212,
    211, 211, 213, 217,
    210, 215, 210, 216,
    214, 219, 218, 220
};

static void NeutralizeRoad(int32_t *tile)
{
    int32_t t = *tile & LOMASK;
    if (t >= 64 && t <= 207) {
        *tile = (t & 0x000F) + 64;
    }
}

/*
 * tally: Checks if an obstacle tile can be automatically cleared by autoBulldoze.
 * Faithful to historic w_tool.c tally().
 */
static bool tally(int32_t tileValue)
{
    if (((tileValue >= FIRSTRIVEDGE) && (tileValue <= LASTRUBBLE)) ||
        ((tileValue >= (POWERBASE + 2)) && (tileValue <= (POWERBASE + 12))) ||
        ((tileValue >= TINYEXP) && (tileValue <= (LASTTINYEXP + 2)))) {
        return true;
    }
    return false;
}

bool AutoDozeCanClear(uint16_t low)
{
    return tally(low);
}

/* Unix ConnecTile autodoze for road, rail, and wire: clears only explosion debris and tiles below 64. */
static void AutoDozeForConnect(int32_t x, int32_t y)
{
    if (!autoBulldoze || TotalFunds <= 0) return;

    uint16_t raw_tile = Map[x][y];
    if (!(raw_tile & BULLBIT)) return;

    int32_t tile = raw_tile & LOMASK;
    NeutralizeRoad(&tile);

    if ((tile >= TINYEXP && tile <= LASTTINYEXP) || (tile < 64 && tile != DIRT)) {
        Map[x][y] = DIRT;
        TotalFunds -= 1;
        MustUpdateFunds = 1;
        FixZone(x, y);
    }
}

static void FixSingle(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return;

    int32_t tile = Map[x][y] & LOMASK;
    NeutralizeRoad(&tile);

    int16_t adj = 0;

    /* 1. Roads (66..76) */
    if (tile >= 66 && tile <= 76) {
        if (y > 0) {
            int32_t t = Map[x][y - 1] & LOMASK;
            NeutralizeRoad(&t);
            if (((t == 237) || (t >= 64 && t <= 78)) && (t != 77 && t != 238 && t != 64)) {
                adj |= 0x0001;
            }
        }
        if (x < (WORLD_X - 1)) {
            int32_t t = Map[x + 1][y] & LOMASK;
            NeutralizeRoad(&t);
            if (((t == 238) || (t >= 64 && t <= 78)) && (t != 78 && t != 237 && t != 65)) {
                adj |= 0x0002;
            }
        }
        if (y < (WORLD_Y - 1)) {
            int32_t t = Map[x][y + 1] & LOMASK;
            NeutralizeRoad(&t);
            if (((t == 237) || (t >= 64 && t <= 78)) && (t != 77 && t != 238 && t != 64)) {
                adj |= 0x0004;
            }
        }
        if (x > 0) {
            int32_t t = Map[x - 1][y] & LOMASK;
            NeutralizeRoad(&t);
            if (((t == 238) || (t >= 64 && t <= 78)) && (t != 78 && t != 237 && t != 65)) {
                adj |= 0x0008;
            }
        }
        Map[x][y] = (uint16_t)(RoadTable[adj] | BULLBIT | BURNBIT);
        return;
    }

    /* 2. Rails (226..236) */
    if (tile >= 226 && tile <= 236) {
        if (y > 0) {
            int32_t t = Map[x][y - 1] & LOMASK;
            NeutralizeRoad(&t);
            if ((t >= 221 && t <= 238) && (t != 221 && t != 237 && t != 224)) {
                adj |= 0x0001;
            }
        }
        if (x < (WORLD_X - 1)) {
            int32_t t = Map[x + 1][y] & LOMASK;
            NeutralizeRoad(&t);
            if ((t >= 221 && t <= 238) && (t != 222 && t != 238 && t != 225)) {
                adj |= 0x0002;
            }
        }
        if (y < (WORLD_Y - 1)) {
            int32_t t = Map[x][y + 1] & LOMASK;
            NeutralizeRoad(&t);
            if ((t >= 221 && t <= 238) && (t != 221 && t != 237 && t != 224)) {
                adj |= 0x0004;
            }
        }
        if (x > 0) {
            int32_t t = Map[x - 1][y] & LOMASK;
            NeutralizeRoad(&t);
            if ((t >= 221 && t <= 238) && (t != 222 && t != 238 && t != 225)) {
                adj |= 0x0008;
            }
        }
        Map[x][y] = (uint16_t)(RailTable[adj] | BULLBIT | BURNBIT);
        return;
    }

    /* 3. Wires (210..220) */
    if (tile >= 210 && tile <= 220) {
        if (y > 0) {
            int32_t t = Map[x][y - 1];
            if (t & CONDBIT) {
                t &= LOMASK;
                NeutralizeRoad(&t);
                if (t != 209 && t != 78 && t != 222) adj |= 0x0001;
            }
        }
        if (x < (WORLD_X - 1)) {
            int32_t t = Map[x + 1][y];
            if (t & CONDBIT) {
                t &= LOMASK;
                NeutralizeRoad(&t);
                if (t != 208 && t != 77 && t != 221) adj |= 0x0002;
            }
        }
        if (y < (WORLD_Y - 1)) {
            int32_t t = Map[x][y + 1];
            if (t & CONDBIT) {
                t &= LOMASK;
                NeutralizeRoad(&t);
                if (t != 209 && t != 78 && t != 222) adj |= 0x0004;
            }
        }
        if (x > 0) {
            int32_t t = Map[x - 1][y];
            if (t & CONDBIT) {
                t &= LOMASK;
                NeutralizeRoad(&t);
                if (t != 208 && t != 77 && t != 221) adj |= 0x0008;
            }
        }
        Map[x][y] = (uint16_t)(WireTable[adj] | BULLBIT | BURNBIT | CONDBIT);
        return;
    }
}

void FixZone(int32_t x, int32_t y)
{
    FixSingle(x, y);
    if (y > 0) FixSingle(x, y - 1);
    if (x < (WORLD_X - 1)) FixSingle(x + 1, y);
    if (y < (WORLD_Y - 1)) FixSingle(x, y + 1);
    if (x > 0) FixSingle(x - 1, y);
}

/*
 * CheckZoneClear: Non-destructive verification of zone footprint.
 * Calculates auto-bulldoze cost without altering the map.
 */
bool CheckZoneClear(int32_t x, int32_t y, int16_t size)
{
    int32_t start_x = x - 1;
    int32_t start_y = y - 1;
    int32_t doze_cost = 0;

    for (int32_t dy = 0; dy < size; dy++) {
        for (int32_t dx = 0; dx < size; dx++) {
            int32_t tx = start_x + dx;
            int32_t ty = start_y + dy;

            if (!TestBounds(tx, ty)) return false;

            uint16_t tile = Map[tx][ty];
            uint16_t low = tile & LOMASK;

            if (low != DIRT) {
                if (autoBulldoze && tally(low)) {
                    doze_cost++;
                } else {
                    return false;
                }
            }
            if (tile & (ZONEBIT | ANIMBIT)) {
                return false;
            }
        }
    }

    if (TotalFunds < doze_cost) {
        return false;
    }

    return true;
}

/* Returns 1 on success, -1 if the footprint is not clear, -2 if funds are short, 0 for a non-zone tool. */
int32_t PlaceZone(int32_t x, int32_t y, int16_t tool_id)
{
    int32_t base_cost = 0;
    int16_t base_tile = 0;
    int16_t size = 3;

    switch (tool_id) {
    case residentialState:
        base_cost = 100;
        base_tile = RESBASE;
        size = 3;
        break;
    case commercialState:
        base_cost = 100;
        base_tile = COMBASE;
        size = 3;
        break;
    case industrialState:
        base_cost = 100;
        base_tile = INDCLR - 4;
        size = 3;
        break;
    case fireState:
        base_cost = 500;
        base_tile = FIRESTATION - 4;
        size = 3;
        break;
    case policeState:
        base_cost = 500;
        base_tile = POLICESTATION - 4;
        size = 3;
        break;
    case powerState:
        base_cost = 3000;
        base_tile = POWERPLANT - 4;
        size = 4;
        break;
    case stadiumState:
        base_cost = 5000;
        base_tile = STADIUM - 4;
        size = 4;
        break;
    case seaportState:
        base_cost = 3000;
        base_tile = PORT - 4;
        size = 4;
        break;
    case nuclearState:
        base_cost = 5000;
        base_tile = NUCLEAR - 4;
        size = 4;
        break;
    case airportState:
        base_cost = 10000;
        base_tile = AIRPORT - 4;
        size = 6;
        break;
    case hospitalState:
        base_cost = 500;
        base_tile = HOSPITAL - 4;
        size = 3;
        break;
    default:
        return 0;
    }

    int32_t start_x = x - 1;
    int32_t start_y = y - 1;
    int32_t doze_cost = 0;

    for (int32_t dy = 0; dy < size; dy++) {
        for (int32_t dx = 0; dx < size; dx++) {
            int32_t tx = start_x + dx;
            int32_t ty = start_y + dy;

            if (!TestBounds(tx, ty)) return -1;

            uint16_t tile = Map[tx][ty];
            uint16_t low = tile & LOMASK;

            if (low != DIRT) {
                if (autoBulldoze && tally(low)) {
                    doze_cost++;
                } else {
                    return -1;
                }
            }
            if (tile & (ZONEBIT | ANIMBIT)) {
                return -1;
            }
        }
    }

    if (TotalFunds < (base_cost + doze_cost)) {
        return -2;
    }

    /* Deduct both zone and auto-bulldoze costs atomically */
    TotalFunds -= (base_cost + doze_cost);
    MustUpdateFunds = 1;

    /* Clear bulldozed footprint */
    for (int32_t dy = 0; dy < size; dy++) {
        for (int32_t dx = 0; dx < size; dx++) {
            int32_t tx = start_x + dx;
            int32_t ty = start_y + dy;
            Water_RemovePipe(tx, ty);
            Map[tx][ty] = DIRT;
        }
    }

    SMapX = (int16_t)x;
    SMapY = (int16_t)y;

    ZonePlop(base_tile);
    return 1;
}

/* Picks the bridge orientation for water from an adjacent road/rail, in historic order (E, W, S, N). Returns 0 if none. */
static int32_t WaterBridgeFor(int32_t x, int32_t y)
{
    int32_t t;

    if (x < WORLD_X - 1) {
        t = Map[x + 1][y] & LOMASK;
        NeutralizeRoad(&t);
        if (t == VRAILROAD || t == HBRIDGE || (t >= ROADS && t <= HROADPOWER)) return HBRIDGE;
    }
    if (x > 0) {
        t = Map[x - 1][y] & LOMASK;
        NeutralizeRoad(&t);
        if (t == VRAILROAD || t == HBRIDGE || (t >= ROADS && t <= INTERSECTION)) return HBRIDGE;
    }
    if (y < WORLD_Y - 1) {
        t = Map[x][y + 1] & LOMASK;
        NeutralizeRoad(&t);
        if (t == HRAILROAD || t == VROADPOWER || (t >= VBRIDGE && t <= INTERSECTION)) return VBRIDGE;
    }
    if (y > 0) {
        t = Map[x][y - 1] & LOMASK;
        NeutralizeRoad(&t);
        if (t == HRAILROAD || t == VROADPOWER || (t >= VBRIDGE && t <= INTERSECTION)) return VBRIDGE;
    }
    return 0;
}

/* Tunnel orientation for rail on water, from an adjacent rail or power tile (E, W, S, N). Returns 0 if none. */
static int32_t WaterRailFor(int32_t x, int32_t y)
{
    int32_t t;

    if (x < WORLD_X - 1) {
        t = Map[x + 1][y] & LOMASK;
        NeutralizeRoad(&t);
        if (t == 221 || t == 224 || (t >= 226 && t <= 237)) return 224;
    }
    if (x > 0) {
        t = Map[x - 1][y] & LOMASK;
        NeutralizeRoad(&t);
        if (t == 221 || t == 224 || (t >= 226 && t <= 237)) return 224;
    }
    if (y < WORLD_Y - 1) {
        t = Map[x][y + 1] & LOMASK;
        NeutralizeRoad(&t);
        if (t == 222 || t == 238 || (t >= 225 && t <= 236)) return 225;
    }
    if (y > 0) {
        t = Map[x][y - 1] & LOMASK;
        NeutralizeRoad(&t);
        if (t == 222 || t == 238 || (t >= 225 && t <= 236)) return 225;
    }
    return 0;
}

/* Underwater wire orientation from an adjacent powered wire (E, W, S, N). Returns 0 if none. */
static int32_t WaterWireFor(int32_t x, int32_t y)
{
    int32_t t;

    if (x < WORLD_X - 1 && (Map[x + 1][y] & CONDBIT)) {
        t = Map[x + 1][y] & LOMASK;
        NeutralizeRoad(&t);
        if (t != 77 && t != 221 && t != 208) return 209;
    }
    if (x > 0 && (Map[x - 1][y] & CONDBIT)) {
        t = Map[x - 1][y] & LOMASK;
        NeutralizeRoad(&t);
        if (t != 77 && t != 221 && t != 208) return 209;
    }
    if (y < WORLD_Y - 1 && (Map[x][y + 1] & CONDBIT)) {
        t = Map[x][y + 1] & LOMASK;
        NeutralizeRoad(&t);
        if (t != 78 && t != 222 && t != 209) return 208;
    }
    if (y > 0 && (Map[x][y - 1] & CONDBIT)) {
        t = Map[x][y - 1] & LOMASK;
        NeutralizeRoad(&t);
        if (t != 78 && t != 222 && t != 209) return 208;
    }
    return 0;
}

int32_t LayRoad(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    AutoDozeForConnect(x, y);
    if (TotalFunds < 10) return -2;

    int32_t tile = Map[x][y] & LOMASK;

    if (tile == DIRT) {
        Map[x][y] = ROADS | BULLBIT | BURNBIT;
        TotalFunds -= 10;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile == RIVER || tile == REDGE || tile == CHANNEL) {
        if (TotalFunds < 50) return -2;
        int32_t bridge = WaterBridgeFor(x, y);
        if (!bridge) return 0;
        Map[x][y] = bridge | BULLBIT;
        TotalFunds -= 50;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile == LHPOWER || tile == LVPOWER) {
        Map[x][y] = (tile == LHPOWER ? VROADPOWER : HROADPOWER) | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 10;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile == LHRAIL || tile == LVRAIL) {
        Map[x][y] = (tile == LHRAIL ? HRAILROAD : VRAILROAD) | BURNBIT | BULLBIT;
        TotalFunds -= 10;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    return 0;
}

int32_t LayRail(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    AutoDozeForConnect(x, y);
    if (TotalFunds < 20) return -2;

    int32_t tile = Map[x][y] & LOMASK;
    NeutralizeRoad(&tile);

    if (tile == DIRT) {
        Map[x][y] = 226 | BULLBIT | BURNBIT;
        TotalFunds -= 20;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile == RIVER || tile == REDGE || tile == CHANNEL) {
        if (TotalFunds < 100) return -2;
        int32_t tunnel = WaterRailFor(x, y);
        if (!tunnel) return 0;
        Map[x][y] = tunnel | BULLBIT;
        TotalFunds -= 100;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile >= ROADS && tile <= (ROADS + 1)) {
        Map[x][y] = (tile == ROADS ? 238 : 237) | BURNBIT | BULLBIT;
        TotalFunds -= 20;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile == 210 || tile == 211) {
        Map[x][y] = (tile == 210 ? 222 : 221) | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 20;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    return 0;
}

int32_t LayWire(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    AutoDozeForConnect(x, y);
    if (TotalFunds < 5) return -2;

    int32_t tile = Map[x][y] & LOMASK;
    NeutralizeRoad(&tile);

    if (tile == DIRT) {
        Map[x][y] = 210 | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 5;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile == RIVER || tile == REDGE || tile == CHANNEL) {
        if (TotalFunds < 25) return -2;
        int32_t underwater = WaterWireFor(x, y);
        if (!underwater) return 0;
        Map[x][y] = underwater | CONDBIT | BULLBIT;
        TotalFunds -= 25;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile == 66 || tile == 67) {
        Map[x][y] = (tile == 66 ? 77 : 78) | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 5;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    if (tile == 226 || tile == 227) {
        Map[x][y] = (tile == 226 ? 221 : 222) | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 5;
        MustUpdateFunds = 1;
        FixZone(x, y);
        return 1;
    }

    return 0;
}

int32_t LayPark(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    if (TotalFunds < CostOf[parkState]) return -2;

    if (Map[x][y] != DIRT) return 0;

    /* Rand(4) yields 0..4; the 4 roll places a fountain, the rest woods. */
    int16_t roll = Rand(4);
    if (roll == 4) {
        Map[x][y] = FOUNTAIN | BURNBIT | BULLBIT | ANIMBIT;
    } else {
        Map[x][y] = (uint16_t)((WOODS2 + roll) | BURNBIT | BULLBIT);
    }

    TotalFunds -= CostOf[parkState];
    MustUpdateFunds = 1;
    return 1;
}

static void ClearZoneArea(int32_t center_x, int32_t center_y, int16_t size)
{
    int32_t start_x = center_x - 1;
    int32_t start_y = center_y - 1;

    for (int32_t dy = 0; dy < size; dy++) {
        for (int32_t dx = 0; dx < size; dx++) {
            int32_t tx = start_x + dx;
            int32_t ty = start_y + dy;
            if (TestBounds(tx, ty)) {
                uint16_t old = Map[tx][ty] & LOMASK;
                if (old == RADTILE || old == 0) continue;
                Map[tx][ty] = (uint16_t)((DoAnimation ? (TINYEXP + Rand(2)) : SOMETINYEXP) | ANIMBIT | BULLBIT);
            }
        }
    }
}

/* Unix plays High for 3x3 zones, Low for 4x4, and both for 6x6. */
static void PlayZoneExplosion(int16_t size)
{
    if (size == 3 || size == 6) Audio_MakeSound("city", "expl-hi");
    if (size == 4 || size == 6) Audio_MakeSound("city", "expl-low");
}

int32_t LayDoze(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    Water_RemovePipe(x, y);

    uint16_t raw_tile = Map[x][y];
    int32_t tile = raw_tile & LOMASK;
    bool has_funds = TotalFunds >= 1;

    /* 1. Zone center demolition */
    if (raw_tile & ZONEBIT) {
        if (!has_funds) {
            Audio_MakeSound("city", "Bulldozer");
            return 1;
        }
        int16_t size = 3;
        if ((tile >= PORTBASE && tile <= LASTPORT) ||
            (tile >= COALBASE && tile <= LASTPOWERPLANT) ||
            (tile >= STADIUMBASE && tile <= FULLSTADIUM) ||
            (tile >= NUCLEARBASE && tile <= NUCLEAR)) {
            size = 4;
        } else if (tile >= AIRPORTBASE && tile <= AIRPORT) {
            size = 6;
        }

        ClearZoneArea(x, y, size);
        TotalFunds -= 1;
        MustUpdateFunds = 1;
        FixZone(x, y);
        PlayZoneExplosion(size);
        return 1;
    }

    /* 2. Outlying zone footprint demolition */
    for (int32_t dy = -5; dy <= 2; dy++) {
        for (int32_t dx = -5; dx <= 2; dx++) {
            int32_t cx = x + dx;
            int32_t cy = y + dy;
            if (TestBounds(cx, cy) && (Map[cx][cy] & ZONEBIT)) {
                uint16_t c_tile = Map[cx][cy] & LOMASK;
                int16_t size = 3;
                if ((c_tile >= PORTBASE && c_tile <= LASTPORT) ||
                    (c_tile >= COALBASE && c_tile <= LASTPOWERPLANT) ||
                    (c_tile >= STADIUMBASE && c_tile <= FULLSTADIUM) ||
                    (c_tile >= NUCLEARBASE && c_tile <= NUCLEAR)) {
                    size = 4;
                } else if (c_tile >= AIRPORTBASE && c_tile <= AIRPORT) {
                    size = 6;
                }

                int32_t min_x = cx - 1;
                int32_t min_y = cy - 1;
                int32_t max_x = min_x + size - 1;
                int32_t max_y = min_y + size - 1;

                if (x >= min_x && x <= max_x && y >= min_y && y <= max_y) {
                    if (!has_funds) {
                        Audio_MakeSound("city", "Bulldozer");
                        return 1;
                    }
                    ClearZoneArea(cx, cy, size);
                    TotalFunds -= 1;
                    MustUpdateFunds = 1;
                    FixZone(cx, cy);
                    PlayZoneExplosion(size);
                    return 1;
                }
            }
        }
    }

    /* 3. Single-tile clearing */
    if (!has_funds) return -2;
    if (raw_tile & BULLBIT) {
        int32_t folded = tile;
        NeutralizeRoad(&folded);
        bool over_water = folded == HBRIDGE || folded == VBRIDGE || folded == BRWV || folded == BRWH ||
                          (folded >= HBRDG0 && folded <= HBRDG3) || (folded >= VBRDG0 && folded <= VBRDG3) ||
                          folded == HPOWER || folded == VPOWER || folded == HRAIL || folded == VRAIL;
        Map[x][y] = over_water ? RIVER : DIRT;

        TotalFunds -= 1;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "Bulldozer");
        return 1;
    }

    return 0;
}