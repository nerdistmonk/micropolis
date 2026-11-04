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

static bool TryAutoBulldozeTile(int32_t x, int32_t y)
{
    if (!autoBulldoze || TotalFunds < 1) return false;
    if (!TestBounds(x, y)) return false;

    uint16_t raw_tile = Map[x][y];
    int32_t tile = raw_tile & LOMASK;
    NeutralizeRoad(&tile);

    if (tally(tile) || (raw_tile & BULLBIT)) {
        if (tile != DIRT && tile != RIVER && tile != REDGE && tile != CHANNEL) {
            Map[x][y] = DIRT;
            TotalFunds -= 1;
            MustUpdateFunds = 1;
            return true;
        }
    }

    return false;
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
        uint16_t cond = (Map[x][y] & PWRBIT) ? PWRBIT : 0;
        Map[x][y] = (uint16_t)(WireTable[adj] | BULLBIT | BURNBIT | CONDBIT | cond);
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

bool PlaceZone(int32_t x, int32_t y, int16_t tool_id)
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
    default:
        return false;
    }

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

    if (TotalFunds < (base_cost + doze_cost)) {
        return false;
    }

    /* Deduct both zone and auto-bulldoze costs atomically */
    TotalFunds -= (base_cost + doze_cost);
    MustUpdateFunds = 1;

    /* Clear bulldozed footprint */
    for (int32_t dy = 0; dy < size; dy++) {
        for (int32_t dx = 0; dx < size; dx++) {
            int32_t tx = start_x + dx;
            int32_t ty = start_y + dy;
            Map[tx][ty] = DIRT;
        }
    }

    SMapX = (int16_t)x;
    SMapY = (int16_t)y;

    ZonePlop(base_tile);
    Audio_MakeSound("city", "build");
    return true;
}

int32_t LayRoad(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    if (TotalFunds < 10) return -2;

    TryAutoBulldozeTile(x, y);

    int32_t tile = Map[x][y] & LOMASK;

    if (tile == DIRT) {
        Map[x][y] = ROADS | BULLBIT | BURNBIT;
        TotalFunds -= 10;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "road");
        return 1;
    }

    if (tile == RIVER || tile == REDGE || tile == CHANNEL) {
        if (TotalFunds < 50) return -2;
        Map[x][y] = HBRIDGE | BULLBIT;
        TotalFunds -= 50;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "road");
        return 1;
    }

    if (tile == LHPOWER || tile == LVPOWER) {
        Map[x][y] = (tile == LHPOWER ? VROADPOWER : HROADPOWER) | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 10;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "road");
        return 1;
    }

    if (tile == LHRAIL || tile == LVRAIL) {
        Map[x][y] = (tile == LHRAIL ? HRAILROAD : VRAILROAD) | BURNBIT | BULLBIT;
        TotalFunds -= 10;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "road");
        return 1;
    }

    return 0;
}

int32_t LayRail(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    if (TotalFunds < 20) return -2;

    TryAutoBulldozeTile(x, y);

    int32_t tile = Map[x][y] & LOMASK;

    if (tile == DIRT) {
        Map[x][y] = 226 | BULLBIT | BURNBIT;
        TotalFunds -= 20;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "rail");
        return 1;
    }

    if (tile == RIVER || tile == REDGE || tile == CHANNEL) {
        if (TotalFunds < 100) return -2;
        Map[x][y] = 224 | BULLBIT;
        TotalFunds -= 100;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "rail");
        return 1;
    }

    if (tile >= ROADS && tile <= (ROADS + 1)) {
        Map[x][y] = (tile == ROADS ? 238 : 237) | BURNBIT | BULLBIT;
        TotalFunds -= 20;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "rail");
        return 1;
    }

    if (tile == 210 || tile == 211) {
        Map[x][y] = (tile == 210 ? 222 : 221) | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 20;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "rail");
        return 1;
    }

    return 0;
}

int32_t LayWire(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    if (TotalFunds < 5) return -2;

    TryAutoBulldozeTile(x, y);

    int32_t tile = Map[x][y] & LOMASK;

    if (tile == DIRT) {
        Map[x][y] = 210 | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 5;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "wire");
        return 1;
    }

    if (tile == RIVER || tile == REDGE || tile == CHANNEL) {
        if (TotalFunds < 25) return -2;
        Map[x][y] = 208 | CONDBIT | BULLBIT;
        TotalFunds -= 25;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "wire");
        return 1;
    }

    if (tile == 66 || tile == 67) {
        Map[x][y] = (tile == 66 ? 77 : 78) | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 5;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "wire");
        return 1;
    }

    if (tile == 226 || tile == 227) {
        Map[x][y] = (tile == 226 ? 221 : 222) | CONDBIT | BURNBIT | BULLBIT;
        TotalFunds -= 5;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "wire");
        return 1;
    }

    return 0;
}

int32_t LayPark(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    if (TotalFunds < CostOf[parkState]) return -2;

    TryAutoBulldozeTile(x, y);

    if ((Map[x][y] & LOMASK) != DIRT) return 0;
    if (TotalFunds < CostOf[parkState]) return -2;

    /* Rand(4) yields 0..4; the 4 roll places a fountain, the rest woods. */
    int16_t roll = Rand(4);
    if (roll == 4) {
        Map[x][y] = FOUNTAIN | BURNBIT | BULLBIT | ANIMBIT;
    } else {
        Map[x][y] = (uint16_t)((WOODS2 + roll) | BURNBIT | BULLBIT);
    }

    TotalFunds -= CostOf[parkState];
    MustUpdateFunds = 1;
    Audio_MakeSound("city", "park");
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
                Map[tx][ty] = (uint16_t)((RUBBLE + (Rand16() & 3)) | BULLBIT);
            }
        }
    }
}

int32_t LayDoze(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    if (TotalFunds < 1) return -2;

    uint16_t raw_tile = Map[x][y];
    int32_t tile = raw_tile & LOMASK;

    /* 1. Zone center demolition */
    if (raw_tile & ZONEBIT) {
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
        Audio_MakeSound("city", "expl-low");
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
                    ClearZoneArea(cx, cy, size);
                    TotalFunds -= 1;
                    MustUpdateFunds = 1;
                    FixZone(cx, cy);
                    Audio_MakeSound("city", "expl-low");
                    return 1;
                }
            }
        }
    }

    /* 3. Single-tile clearing */
    if (raw_tile & BULLBIT) {
        if ((tile >= HBRIDGE && tile <= VBRIDGE) ||
            (tile >= 208 && tile <= 209) ||
            (tile >= 224 && tile <= 225)) {
            Map[x][y] = RIVER;
        } else {
            Map[x][y] = DIRT;
        }

        TotalFunds -= 1;
        MustUpdateFunds = 1;
        FixZone(x, y);
        Audio_MakeSound("city", "Bulldozer");
        return 1;
    }

    return 0;
}