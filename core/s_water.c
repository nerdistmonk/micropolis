/* s_water.c: Water towers, underground pipes, and zone water supply.
 *
 * A tower draws from a water tile within WATER_TOWER_REACH of its center. Pipes are
 * kept in their own layer, separate from the surface Map, so they are only visible in
 * the underground overlay. Each tower supplies up to WATER_TOWER_ZONES zone buildings
 * that touch its connected pipe network.
 */

#include <string.h>
#include "sim.h"

#define WATER_TOWER_REACH 4
#define WATER_TOWER_ZONES 9
#define WATER_TOWER_CENTER (WATERTOWER + 4)
#define WATER_PIPE_BYTES (WORLD_X * WORLD_Y)

static uint8_t pipe_map[WORLD_X][WORLD_Y];
static uint8_t water_map[WORLD_X][WORLD_Y];
static uint8_t seen_map[WORLD_X][WORLD_Y];
static int32_t pipe_queue[WORLD_X * WORLD_Y];

static bool IsWaterSource(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    int32_t t = Map[x][y] & LOMASK;
    return t >= RIVER && t <= CHANNEL;
}

static bool TowerHasSource(int32_t cx, int32_t cy)
{
    for (int32_t dy = -WATER_TOWER_REACH; dy <= WATER_TOWER_REACH; dy++) {
        for (int32_t dx = -WATER_TOWER_REACH; dx <= WATER_TOWER_REACH; dx++) {
            if (IsWaterSource(cx + dx, cy + dy)) return true;
        }
    }
    return false;
}

/* A zone center is watered if a pipe lies within its footprint or the ring around it. */
static void ClaimZonesNear(int32_t px, int32_t py, int32_t *claimed)
{
    for (int32_t dy = -2; dy <= 2; dy++) {
        for (int32_t dx = -2; dx <= 2; dx++) {
            int32_t x = px + dx;
            int32_t y = py + dy;
            if (!TestBounds(x, y)) continue;
            if (!(Map[x][y] & ZONEBIT) || water_map[x][y]) continue;
            if (*claimed >= WATER_TOWER_ZONES) return;
            water_map[x][y] = 1;
            (*claimed)++;
        }
    }
}

static void SupplyFromTower(int32_t tx, int32_t ty)
{
    static const int32_t step_x[4] = { 1, -1, 0, 0 };
    static const int32_t step_y[4] = { 0, 0, 1, -1 };
    int32_t head = 0;
    int32_t tail = 0;
    int32_t claimed = 0;

    memset(seen_map, 0, sizeof(seen_map));

    for (int32_t dy = -2; dy <= 2; dy++) {
        for (int32_t dx = -2; dx <= 2; dx++) {
            int32_t x = tx + dx;
            int32_t y = ty + dy;
            if (!TestBounds(x, y) || !pipe_map[x][y] || seen_map[x][y]) continue;
            seen_map[x][y] = 1;
            pipe_queue[tail++] = x * WORLD_Y + y;
        }
    }

    while (head < tail) {
        int32_t idx = pipe_queue[head++];
        int32_t x = idx / WORLD_Y;
        int32_t y = idx % WORLD_Y;

        ClaimZonesNear(x, y, &claimed);

        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = x + step_x[i];
            int32_t ny = y + step_y[i];
            if (!TestBounds(nx, ny) || !pipe_map[nx][ny] || seen_map[nx][ny]) continue;
            seen_map[nx][ny] = 1;
            pipe_queue[tail++] = nx * WORLD_Y + ny;
        }
    }
}

void DoWaterScan(void)
{
    memset(water_map, 0, sizeof(water_map));

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if ((Map[x][y] & LOMASK) != WATER_TOWER_CENTER) continue;
            if (!TowerHasSource(x, y)) continue;
            SupplyFromTower(x, y);
        }
    }
}

bool WaterIsSupplied(int32_t x, int32_t y)
{
    return TestBounds(x, y) && water_map[x][y] != 0;
}

bool WaterHasPipe(int32_t x, int32_t y)
{
    return TestBounds(x, y) && pipe_map[x][y] != 0;
}

int32_t LayPipe(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    if (pipe_map[x][y]) return 0;
    if (TotalFunds < CostOf[pipeState]) return -2;

    pipe_map[x][y] = 1;
    TotalFunds -= CostOf[pipeState];
    MustUpdateFunds = 1;
    return 1;
}

int32_t LayWaterTower(int32_t x, int32_t y)
{
    if (!TestBounds(x - 1, y - 1) || !TestBounds(x + 1, y + 1)) return 0;

    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            if ((Map[x + dx][y + dy] & LOMASK) != DIRT) return 0;
        }
    }
    if (TotalFunds < CostOf[waterTowerState]) return -2;

    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            uint16_t tile = (uint16_t)(WATERTOWER + (dy + 1) * 3 + (dx + 1));
            Map[x + dx][y + dy] = (uint16_t)(tile | BULLBIT);
        }
    }
    TotalFunds -= CostOf[waterTowerState];
    MustUpdateFunds = 1;
    return 1;
}

void Water_ClearPipes(void)
{
    memset(pipe_map, 0, sizeof(pipe_map));
    memset(water_map, 0, sizeof(water_map));
}

void Water_RemovePipe(int32_t x, int32_t y)
{
    if (TestBounds(x, y)) pipe_map[x][y] = 0;
}

size_t Water_PipeBlockBytes(void)
{
    return WATER_PIPE_BYTES;
}

void Water_CopyPipes(uint8_t *dst)
{
    memcpy(dst, pipe_map, WATER_PIPE_BYTES);
}

void Water_LoadPipes(const uint8_t *src)
{
    memcpy(pipe_map, src, WATER_PIPE_BYTES);
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            pipe_map[x][y] = pipe_map[x][y] ? 1 : 0;
        }
    }
}
