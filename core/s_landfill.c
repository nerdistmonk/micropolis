/* s_landfill.c: Landfill blocks absorb the waste of the zones they service.
 *
 * Each road-connected landfill block services LANDFILL_ZONES_PER_BLOCK developed zones, scaled
 * by landfillPercent (budget funding), as police and fire stations do. Demand is the citywide
 * count of residential, commercial, and industrial zone centres. Each CityTime tick compares
 * demand with total capacity:
 *   - Overloaded: each connected block that is not full gains LANDFILL_STEP_TRASH waste.
 *     Demand beyond capacity is uncollected.
 *   - Under capacity: each block holding waste loses LANDFILL_STEP_TRASH every
 *     LANDFILL_EMPTY_PERIOD ticks, so it empties at one third of its fill rate.
 *   - Balanced: nothing changes.
 * With no capacity at all (no connected block) and demand present, trash piles up unserviced
 * at LANDFILL_STEP_TRASH per tick, up to LANDFILL_UNSERVICED_MAX. It drains the same way once
 * demand is gone.
 * A block shows four stages as it fills. Its upkeep is tiered: empty costs nothing, then one
 * third, two thirds, and full cost. Filled blocks pollute through LandfillPollution.
 * TrashPending is the total waste: stored in blocks plus UnservicedTrash. The land-value,
 * garbage, and pile effects read it.
 *
 * Per-block fill is kept in LandfillFill, indexed by block centre. It is saved as a trailing
 * section of the city file. LandfillRescan keeps each loaded value within its stage's range.
 */

#include <string.h>
#include "sim.h"

#define LANDFILL_ZONES_PER_BLOCK 9
#define LANDFILL_STEP_TRASH      40
#define LANDFILL_EMPTY_PERIOD    3
#define LANDFILL_STAGE_CAP       800
#define LANDFILL_BLOCK_CAP       (LANDFILL_STAGE_CAP * (LANDFILL_STAGES - 1))
#define LANDFILL_CENTRE_OFFSET   4
#define LANDFILL_PENALTY_DIV     250
#define LANDFILL_PENALTY_MAX     20
#define LANDFILL_UNSERVICED_MAX  5000

static const int32_t LandfillStagePollution[LANDFILL_STAGES] = { 0, 10, 20, 30 };

float landfillPercent = 1.0f;
int16_t LandfillPop = 0;
int32_t LandfillFund = 0;
int32_t LandfillSpend = 0;
int32_t TrashPending = 0;
int32_t UnservicedTrash = 0;
bool LandfillAtCapacity = false;

static uint16_t LandfillFill[WORLD_X][WORLD_Y];

static bool IsLandfillTile(int32_t low)
{
    return low >= LANDFILL && low < LANDFILL + LANDFILL_TILES;
}

static bool IsLandfillCentre(int32_t low)
{
    return IsLandfillTile(low) && ((low - LANDFILL) % LANDFILL_STAGE_TILES) == LANDFILL_CENTRE_OFFSET;
}

static int32_t LandfillStageOf(int32_t low)
{
    return (low - LANDFILL) / LANDFILL_STAGE_TILES;
}

/* ZonePlop reads the centre from SMapX/SMapY and clobbers CChr, so save and restore them. */
static void PlopBlock(int32_t cx, int32_t cy, int32_t stage)
{
    int16_t saveX = SMapX;
    int16_t saveY = SMapY;
    int16_t saveC = CChr;

    SMapX = (int16_t)cx;
    SMapY = (int16_t)cy;
    ZonePlop((int16_t)(LANDFILL + stage * LANDFILL_STAGE_TILES));

    SMapX = saveX;
    SMapY = saveY;
    CChr = saveC;
}

/* True when a road touches the block's 3x3 edge (the 12 cells just outside it). */
static bool IsConnected(int32_t cx, int32_t cy)
{
    static const int32_t edge[12][2] = {
        {-1, -2}, {0, -2}, {1, -2},
        {-1,  2}, {0,  2}, {1,  2},
        {-2, -1}, {-2, 0}, {-2, 1},
        { 2, -1}, { 2, 0}, { 2, 1},
    };
    for (int32_t i = 0; i < 12; i++) {
        int32_t tx = cx + edge[i][0];
        int32_t ty = cy + edge[i][1];
        if (TestBounds(tx, ty) && RoadTest(Map[tx][ty])) {
            return true;
        }
    }
    return false;
}

static bool IsDemandZone(uint16_t t)
{
    int32_t low = t & LOMASK;
    if (!(t & ZONEBIT)) {
        return false;
    }
    return ((low >= RESBASE) && (low < HOSPITAL)) ||
           ((low >= COMBASE) && (low < INDBASE)) ||
           ((low >= INDBASE) && (low < PORTBASE));
}

void DoLandfill(void)
{
    int32_t zones = 0;
    int32_t connectedCount = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if (IsDemandZone(Map[x][y])) {
                zones++;
            } else if (IsLandfillCentre(Map[x][y] & LOMASK) && IsConnected(x, y)) {
                connectedCount++;
            }
        }
    }

    float capacity = (float)LANDFILL_ZONES_PER_BLOCK * landfillPercent * (float)connectedCount;
    bool overloaded = (float)zones > capacity;
    bool underCapacity = (float)zones < capacity;
    bool emptyTick = (CityTime % LANDFILL_EMPTY_PERIOD) == 0;

    int32_t blocks = 0;
    int32_t roomy = 0;
    int32_t waste = 0;

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t low = Map[x][y] & LOMASK;
            if (!IsLandfillCentre(low)) {
                continue;
            }
            blocks++;

            int32_t stage = LandfillStageOf(low);
            if ((int32_t)LandfillFill[x][y] / LANDFILL_STAGE_CAP != stage) {
                LandfillFill[x][y] = (uint16_t)(stage * LANDFILL_STAGE_CAP);
            }
            int32_t fill = LandfillFill[x][y];
            bool connected = IsConnected(x, y);

            if (overloaded && connected && fill < LANDFILL_BLOCK_CAP) {
                int32_t room = LANDFILL_BLOCK_CAP - fill;
                fill += (room < LANDFILL_STEP_TRASH) ? room : LANDFILL_STEP_TRASH;
            } else if (underCapacity && emptyTick && fill > 0) {
                fill -= (fill < LANDFILL_STEP_TRASH) ? fill : LANDFILL_STEP_TRASH;
            }

            if (fill != LandfillFill[x][y]) {
                LandfillFill[x][y] = (uint16_t)fill;
                int32_t newStage = fill / LANDFILL_STAGE_CAP;
                if (newStage != stage) {
                    PlopBlock(x, y, newStage);
                }
            }

            waste += fill;
            if (connected && fill < LANDFILL_BLOCK_CAP) {
                roomy++;
            }
        }
    }

    if (connectedCount == 0) {
        if (zones > 0) {
            UnservicedTrash += LANDFILL_STEP_TRASH;
            if (UnservicedTrash > LANDFILL_UNSERVICED_MAX) UnservicedTrash = LANDFILL_UNSERVICED_MAX;
        } else if (UnservicedTrash > 0) {
            UnservicedTrash -= (UnservicedTrash < LANDFILL_STEP_TRASH) ? UnservicedTrash : LANDFILL_STEP_TRASH;
        }
    } else {
        UnservicedTrash = 0;
    }

    LandfillPop = (int16_t)blocks;
    TrashPending = waste + UnservicedTrash;
    LandfillAtCapacity = overloaded && roomy == 0 && (connectedCount == 0 || waste > 0);
}


void LandfillBlockPlaced(int32_t x, int32_t y)
{
    LandfillFill[x][y] = 0;
}

int32_t LandfillTrashPenalty(void)
{
    int32_t p = TrashPending / LANDFILL_PENALTY_DIV;
    return p > LANDFILL_PENALTY_MAX ? LANDFILL_PENALTY_MAX : p;
}

int32_t LandfillPollution(int32_t loc)
{
    if (!IsLandfillTile(loc)) {
        return 0;
    }
    return LandfillStagePollution[LandfillStageOf(loc)];
}

void LandfillResetCity(void)
{
    landfillPercent = 1.0f;
    TrashPending = 0;
    UnservicedTrash = 0;
    LandfillAtCapacity = false;
    LandfillPop = 0;
    LandfillFund = 0;
    LandfillSpend = 0;
    memset(LandfillFill, 0, sizeof LandfillFill);
}

void LandfillRescan(void)
{
    LandfillPop = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t low = Map[x][y] & LOMASK;
            if (!IsLandfillCentre(low)) {
                LandfillFill[x][y] = 0;
                continue;
            }
            LandfillPop++;
            int32_t stage = LandfillStageOf(low);
            int32_t lo = stage * LANDFILL_STAGE_CAP;
            int32_t hi = (stage == LANDFILL_STAGES - 1) ? LANDFILL_BLOCK_CAP : lo + LANDFILL_STAGE_CAP - 1;
            int32_t fill = LandfillFill[x][y];
            if (fill < lo || fill > hi) {
                fill = lo;
            }
            LandfillFill[x][y] = (uint16_t)fill;
        }
    }
    LandfillFund = LandfillPop * 100;
    LandfillSpend = (int32_t)((float)LandfillFund * landfillPercent);
}

int32_t LandfillTieredCost(void)
{
    int32_t cost = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t low = Map[x][y] & LOMASK;
            if (!IsLandfillCentre(low)) {
                continue;
            }
            int32_t fill = LandfillFill[x][y];
            if (fill <= 0) {
                continue;
            }
            int32_t tier = (3 * fill + LANDFILL_BLOCK_CAP - 1) / LANDFILL_BLOCK_CAP;
            if (tier > 3) {
                tier = 3;
            }
            cost += 100 * tier / 3;
        }
    }
    return cost;
}

static void PutU16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static uint16_t GetU16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

void LandfillEncodeFill(uint8_t *dst)
{
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            PutU16(dst + ((size_t)x * WORLD_Y + (size_t)y) * 2, LandfillFill[x][y]);
        }
    }
}

/* A NULL src clears the fill, so a city without a fill section starts from stage floors. */
void LandfillDecodeFill(const uint8_t *src)
{
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            LandfillFill[x][y] = src ? GetU16(src + ((size_t)x * WORLD_Y + (size_t)y) * 2) : 0;
        }
    }
}
