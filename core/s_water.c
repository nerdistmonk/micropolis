/* s_water.c: Water towers, underground pipes, and zone water supply.
 *
 * A tower draws from a water tile within WATER_TOWER_REACH of its center. Pipes are
 * kept in their own layer, separate from the surface Map, so they are only visible in
 * the underground overlay. Water floods outward from a tower through pipes and zone
 * buildings the same way power floods through wires. Power plants are sinks: water
 * reaches them but does not pass through. Each tower serves up to WATER_TOWER_ZONES
 * zone centers.
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
    return t >= RIVER && t <= LASTRIVEDGE;
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

/* Conducts water like a wire conducts power: zone buildings carry CONDBIT, plants and ports do not. */
static bool IsWaterConductor(uint16_t tile)
{
    int32_t t = tile & LOMASK;
    if (!(tile & CONDBIT)) return false;
    if (t < RESBASE || t > LASTZONE) return false;
    if (t >= PORTBASE && t < COALBASE) return false;
    return !TILE_IS_POWER_PLANT(tile);
}

static bool WaterCanEnter(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    uint16_t tile = Map[x][y];
    return pipe_map[x][y] || TILE_IS_POWER_PLANT(tile) || IsWaterConductor(tile);
}

/* A 4x4 plant is watered as one unit: find its center around (x,y) and mark every footprint tile. */
static void MarkPlantFootprint(int32_t x, int32_t y)
{
    for (int32_t cy = y - 2; cy <= y + 1; cy++) {
        for (int32_t cx = x - 2; cx <= x + 1; cx++) {
            if (!TestBounds(cx, cy)) continue;
            uint16_t center = Map[cx][cy] & LOMASK;
            if (center != POWERPLANT && center != NUCLEAR) continue;

            int32_t min_x = cx - 1;
            int32_t min_y = cy - 1;
            if (!TestBounds(min_x, min_y) || !TestBounds(min_x + 3, min_y + 3)) continue;

            for (int32_t dy = 0; dy < 4; dy++) {
                for (int32_t dx = 0; dx < 4; dx++) {
                    int32_t px = min_x + dx;
                    int32_t py = min_y + dy;
                    if (TILE_IS_POWER_PLANT(Map[px][py])) water_map[px][py] = 1;
                }
            }
            return;
        }
    }
}

static void SupplyFromTower(int32_t tx, int32_t ty)
{
    static const int32_t step_x[4] = { 1, -1, 0, 0 };
    static const int32_t step_y[4] = { 0, 0, 1, -1 };
    int32_t head = 0;
    int32_t tail = 0;
    int32_t zones = 0;

    memset(seen_map, 0, sizeof(seen_map));

    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            int32_t x = tx + dx;
            int32_t y = ty + dy;
            seen_map[x][y] = 1;
            pipe_queue[tail++] = x * WORLD_Y + y;
        }
    }

    while (head < tail) {
        int32_t idx = pipe_queue[head++];
        int32_t x = idx / WORLD_Y;
        int32_t y = idx % WORLD_Y;

        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = x + step_x[i];
            int32_t ny = y + step_y[i];
            if (!WaterCanEnter(nx, ny) || seen_map[nx][ny]) continue;
            seen_map[nx][ny] = 1;

            uint16_t tile = Map[nx][ny];
            if (TILE_IS_POWER_PLANT(tile)) {
                water_map[nx][ny] = 1;
                MarkPlantFootprint(nx, ny);
                continue;
            }
            if ((tile & ZONEBIT) && zones >= WATER_TOWER_ZONES) continue;
            if (tile & ZONEBIT) zones++;

            water_map[nx][ny] = 1;
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

bool WaterCanConnect(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    if (WaterCanEnter(x, y)) return true;
    int32_t t = Map[x][y] & LOMASK;
    return t >= WATERTOWER && t < WATERTOWER + WATERTOWER_TILES;
}

bool WaterPlantSupplied(int32_t x, int32_t y)
{
    for (int32_t dy = -3; dy <= 3; dy++) {
        for (int32_t dx = -3; dx <= 3; dx++) {
            int32_t px = x + dx;
            int32_t py = y + dy;
            if (TestBounds(px, py) && water_map[px][py] && TILE_IS_POWER_PLANT(Map[px][py])) return true;
        }
    }
    return false;
}

bool WaterTowerHasSource(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    int32_t off = (Map[x][y] & LOMASK) - WATERTOWER;
    if (off < 0 || off >= WATERTOWER_TILES) return false;
    return TowerHasSource(x + 1 - off % 3, y + 1 - off / 3);
}

/* Pipe runs underground beneath anything except a building, so it can cross roads, rail, and water. Buildings attach pipe at their edge, like power lines. */
static bool PipeCanLay(uint16_t raw)
{
    int32_t t = raw & LOMASK;
    if (t >= RESBASE && t <= LASTZONE) return false;
    return !(t >= WATERTOWER && t < WATERTOWER + WATERTOWER_TILES);
}

int32_t LayPipe(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return 0;
    if (pipe_map[x][y]) return 0;
    if (!PipeCanLay(Map[x][y])) return 0;
    if (TotalFunds < CostOf[pipeState]) return -2;

    pipe_map[x][y] = 1;
    TotalFunds -= CostOf[pipeState];
    MustUpdateFunds = 1;
    return 1;
}

int32_t LayWaterTower(int32_t x, int32_t y)
{
    if (!TestBounds(x - 1, y - 1) || !TestBounds(x + 1, y + 1)) return 0;

    int32_t doze_cost = 0;
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            uint16_t tile = Map[x + dx][y + dy];
            uint16_t low = tile & LOMASK;
            if (low != DIRT) {
                if (autoBulldoze && AutoDozeCanClear(low)) {
                    doze_cost++;
                } else {
                    return 0;
                }
            }
            if (tile & (ZONEBIT | ANIMBIT)) return 0;
        }
    }
    if (TotalFunds < CostOf[waterTowerState] + doze_cost) return -2;

    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            uint16_t tile = (uint16_t)(WATERTOWER + (dy + 1) * 3 + (dx + 1));
            Water_RemovePipe(x + dx, y + dy);
            Map[x + dx][y + dy] = (uint16_t)(tile | BULLBIT);
        }
    }
    TotalFunds -= CostOf[waterTowerState] + doze_cost;
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
