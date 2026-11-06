/* s_landfill.c: Landfill blocks take in the city's trash.
 *
 * Each CityTime tick the city makes trash equal to TotalPop / LANDFILL_GEN_DIVISOR. It goes
 * into a shared pool (TrashPending). A landfill block takes from the pool only while a road
 * touches its edge, up to LANDFILL_ACCEPT_PER_TICK scaled by landfillPercent (budget funding).
 * A block fills through three visible stages. When every connected block is full and no
 * empty block exists, a new empty block is placed next to a full one. Trash left in the pool
 * adds pollution to zone tiles, and filled blocks pollute through LandfillPollution.
 *
 * Per-block fill is kept in LandfillFill, indexed by block centre. It is saved as a trailing
 * section of the city file. LandfillRescan keeps each loaded value within its stage's range.
 */

#include <string.h>
#include "sim.h"

#define LANDFILL_GEN_DIVISOR     8
#define LANDFILL_ACCEPT_PER_TICK 40
#define LANDFILL_STAGE_CAP       400
#define LANDFILL_BLOCK_CAP       (LANDFILL_STAGE_CAP * (LANDFILL_STAGES - 1))
#define LANDFILL_CENTRE_OFFSET   4
#define LANDFILL_POOL_MAX        100000
#define LANDFILL_PENALTY_DIV     250
#define LANDFILL_PENALTY_MAX     20

static const int32_t LandfillStagePollution[LANDFILL_STAGES] = { 0, 10, 20, 30 };

float landfillPercent = 1.0f;
int16_t LandfillPop = 0;
int32_t LandfillFund = 0;
int32_t LandfillSpend = 0;
int32_t TrashPending = 0;
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

static bool CanPlaceBlock(int32_t cx, int32_t cy)
{
    if (cx < 1 || cy < 1 || cx > WORLD_X - 2 || cy > WORLD_Y - 2) {
        return false;
    }
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            if (Map[cx + dx][cy + dy] != DIRT) {
                return false;
            }
        }
    }
    return true;
}

void DoLandfill(void)
{
    int32_t blocks = 0;
    int32_t roomy = 0;
    int32_t spare = 0;
    int32_t fullX = -1;
    int32_t fullY = -1;
    int32_t connected = 0;

    TrashPending += TotalPop / LANDFILL_GEN_DIVISOR;
    if (TrashPending > LANDFILL_POOL_MAX) {
        TrashPending = LANDFILL_POOL_MAX;
    }

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

            if (IsConnected(x, y)) {
                connected++;
                if (fill >= LANDFILL_BLOCK_CAP) {
                    if (fullX < 0) {
                        fullX = x;
                        fullY = y;
                    }
                } else {
                    roomy++;
                    int32_t take = (int32_t)((float)LANDFILL_ACCEPT_PER_TICK * landfillPercent);
                    if (take > TrashPending) take = TrashPending;
                    if (take > LANDFILL_BLOCK_CAP - fill) take = LANDFILL_BLOCK_CAP - fill;
                    if (take > 0) {
                        fill += take;
                        TrashPending -= take;
                        LandfillFill[x][y] = (uint16_t)fill;
                        int32_t newStage = fill / LANDFILL_STAGE_CAP;
                        if (newStage != stage) {
                            PlopBlock(x, y, newStage);
                        }
                    }
                }
            }

            if (fill == 0) {
                spare++;
            }
        }
    }

    LandfillPop = (int16_t)blocks;
    LandfillAtCapacity = connected > 0 && roomy == 0 && TrashPending > 0;

    if (TrashPending > 0 && roomy == 0 && spare == 0 && fullX >= 0) {
        static const int32_t step[4][2] = { {3, 0}, {-3, 0}, {0, 3}, {0, -3} };
        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = fullX + step[i][0];
            int32_t ny = fullY + step[i][1];
            if (CanPlaceBlock(nx, ny)) {
                PlopBlock(nx, ny, 0);
                LandfillFill[nx][ny] = 0;
                break;
            }
        }
    }
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
