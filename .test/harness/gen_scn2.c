/* Throwaway: lays a water network (towers + pipes) that supplies the demand zones, then seeds landfill.
 * usage: gen_scn2 IN.scn OUT_ROOT DEDUCT(0|1)
 * GEN_UNLIMITED=1 runs with unlimited funds to measure the full coverage cost. Not part of the repo. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"
#include "scenario.h"

void Audio_MakeSound(const char *channel, const char *sound_name) { (void)channel; (void)sound_name; }

extern int32_t Scenario_LoadFile(const char *path);
extern int32_t Scenario_ReadDef(const char *path, ScenarioDef *def);

#define WT_REACH 4
#define LF_ZONE_WANT 6
#define MAX_ITER 3000

static int32_t Dist[WORLD_X][WORLD_Y];
static int32_t Q[WORLD_X * WORLD_Y];
static bool Skip[WORLD_X][WORLD_Y];

static const int32_t DX[4] = { 1, -1, 0, 0 };
static const int32_t DY[4] = { 0, 0, 1, -1 };
static const int32_t EDGE[12][2] = {
    {-1, -2}, {0, -2}, {1, -2}, {-1, 2}, {0, 2}, {1, 2},
    {-2, -1}, {-2, 0}, {-2, 1}, {2, -1}, {2, 0}, {2, 1},
};

/* Residential, commercial, and industrial zones. Ports, power plants, and landfill sit above PORTBASE. */
static bool IsDemandTile(uint16_t t)
{
    int32_t l = t & LOMASK;
    return l >= RESBASE && l < PORTBASE;
}

static bool IsWTTile(uint16_t t)
{
    int32_t l = t & LOMASK;
    return l >= WATERTOWER && l < WATERTOWER + WATERTOWER_TILES;
}

static bool IsRiverTile(uint16_t t)
{
    int32_t l = t & LOMASK;
    return l >= RIVER && l <= LASTRIVEDGE;
}

/* Mirrors PipeCanLay in s_water.c: pipe runs anywhere except zone buildings and water towers. */
static bool Passable(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    uint16_t t = Map[x][y];
    return !((t & LOMASK) >= RESBASE && (t & LOMASK) <= LASTZONE) && !IsWTTile(t);
}

static bool Unsupplied(int32_t x, int32_t y)
{
    return TestBounds(x, y) && IsDemandTile(Map[x][y]) && !WaterIsSupplied(x, y) && !Skip[x][y];
}

static void MarkSkip(int32_t zx, int32_t zy)
{
    for (int32_t dy = -2; dy <= 2; dy++)
        for (int32_t dx = -2; dx <= 2; dx++)
            if (TestBounds(zx + dx, zy + dy)) Skip[zx + dx][zy + dy] = true;
}

/* Pipe distance from the seeds: the whole network when use_network, else the 3x3 footprint at (fx,fy). */
static void BuildDist(bool use_network, int32_t fx, int32_t fy)
{
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            Dist[x][y] = -1;

    int32_t head = 0, tail = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            bool seed = use_network ? (WaterHasPipe(x, y) || IsWTTile(Map[x][y]))
                                    : (abs(x - fx) <= 1 && abs(y - fy) <= 1);
            if (seed) {
                Dist[x][y] = 0;
                Q[tail++] = x * WORLD_Y + y;
            }
        }
    }
    while (head < tail) {
        int32_t idx = Q[head++];
        int32_t x = idx / WORLD_Y, y = idx % WORLD_Y;
        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = x + DX[i], ny = y + DY[i];
            if (Passable(nx, ny) && Dist[nx][ny] < 0) {
                Dist[nx][ny] = Dist[x][y] + 1;
                Q[tail++] = nx * WORLD_Y + ny;
            }
        }
    }
}

/* Lays pipe from the cheapest tile touching an unsupplied demand tile back to the seeds.
 * Returns pipes laid, 0 if the zone already touches the seeds, -1 if none is reachable, -2 if out of funds. */
static int32_t RouteToZone(bool use_network, int32_t fx, int32_t fy, int32_t *zx, int32_t *zy)
{
    BuildDist(use_network, fx, fy);

    int32_t bx = -1, by = -1, bd = 0x7fffffff;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if (Dist[x][y] < 0 || Dist[x][y] >= bd || !Passable(x, y)) continue;
            for (int32_t i = 0; i < 4; i++) {
                int32_t nx = x + DX[i], ny = y + DY[i];
                if (Unsupplied(nx, ny)) {
                    bd = Dist[x][y];
                    bx = x; by = y;
                    *zx = nx; *zy = ny;
                    break;
                }
            }
        }
    }
    if (bx < 0) return -1;
    if (bd == 0) return 0;

    int32_t laid = 0;
    int32_t cx = bx, cy = by;
    while (Dist[cx][cy] > 0) {
        int32_t r = LayPipe(cx, cy);
        if (r == -2) return -2;
        if (r == 1) laid++;
        int32_t moved = 0;
        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = cx + DX[i], ny = cy + DY[i];
            if (Passable(nx, ny) && Dist[nx][ny] == Dist[cx][cy] - 1) {
                cx = nx; cy = ny; moved = 1;
                break;
            }
        }
        if (!moved) break;
    }
    return laid;
}

/* Lays pipe from the tile at (cx,cy) down the Dist gradient to the seed footprint. Returns pipes laid, -2 if out of funds. */
static int32_t LayAlongDist(int32_t cx, int32_t cy)
{
    int32_t laid = 0;
    while (Dist[cx][cy] > 0) {
        int32_t r = LayPipe(cx, cy);
        if (r == -2) return -2;
        if (r == 1) laid++;
        int32_t moved = 0;
        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = cx + DX[i], ny = cy + DY[i];
            if (Passable(nx, ny) && Dist[nx][ny] == Dist[cx][cy] - 1) {
                cx = nx; cy = ny; moved = 1;
                break;
            }
        }
        if (!moved) break;
    }
    return laid;
}

/* Joins a freshly placed tower's footprint to the existing pipe network. Returns pipes laid, 0 if no network, -2 if out of funds. */
static int32_t ConnectTower(int32_t fx, int32_t fy)
{
    BuildDist(false, fx, fy);
    int32_t bx = -1, by = -1, bd = 0x7fffffff;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if (Dist[x][y] < 1 || Dist[x][y] >= bd || !Passable(x, y)) continue;
            for (int32_t i = 0; i < 4; i++) {
                int32_t nx = x + DX[i], ny = y + DY[i];
                if (TestBounds(nx, ny) && WaterHasPipe(nx, ny) && Dist[nx][ny] != 0) {
                    bd = Dist[x][y];
                    bx = x; by = y;
                    break;
                }
            }
        }
    }
    if (bx < 0) return 0;
    return LayAlongDist(bx, by);
}

static bool TowerSiteOK(int32_t cx, int32_t cy)
{
    if (cx < 2 || cy < 2 || cx > WORLD_X - 3 || cy > WORLD_Y - 3) return false;
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            uint16_t t = Map[cx + dx][cy + dy];
            if ((t & LOMASK) != DIRT || (t & (ZONEBIT | ANIMBIT))) return false;
        }
    }
    return true;
}

static bool TouchesWater(int32_t x, int32_t y)
{
    return IsRiverTile(Map[x][y]) || (TestBounds(x + 1, y) && IsRiverTile(Map[x + 1][y])) ||
           (TestBounds(x - 1, y) && IsRiverTile(Map[x - 1][y])) ||
           (TestBounds(x, y + 1) && IsRiverTile(Map[x][y + 1])) ||
           (TestBounds(x, y - 1) && IsRiverTile(Map[x][y - 1]));
}

/* Nearest passable pipe tile (distance >= 1 from the footprint) that touches river, lake, or ocean. */
static bool FindWaterTap(int32_t fx, int32_t fy, int32_t *tx, int32_t *ty)
{
    BuildDist(false, fx, fy);
    int32_t bd = 0x7fffffff;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if (Dist[x][y] < 1 || Dist[x][y] >= bd || !Passable(x, y) || !TouchesWater(x, y)) continue;
            bd = Dist[x][y];
            *tx = x; *ty = y;
        }
    }
    return bd < 0x7fffffff;
}

/* Pipes a tower footprint to water. Returns 1 if sourced (new or existing pipe), 0 if no route, -2 if out of funds. */
static int32_t RouteTowerToWater(int32_t fx, int32_t fy)
{
    int32_t tx = -1, ty = -1;
    if (!FindWaterTap(fx, fy, &tx, &ty)) return 0;
    if (LayAlongDist(tx, ty) == -2) return -2;
    return 1;
}

/* Places the tower site nearest (zx,zy). Returns 1 placed, 0 no site, -2 out of funds. */
static void SiteDebug(void)
{
    if (getenv("GEN_DEBUG")) {
        int32_t free_dirt = 0, free_river = 0;
        for (int32_t cx = 2; cx <= WORLD_X - 3; cx++)
            for (int32_t cy = 2; cy <= WORLD_Y - 3; cy++) {
                bool ok = true;
                for (int32_t dy = -1; dy <= 1 && ok; dy++)
                    for (int32_t dx = -1; dx <= 1 && ok; dx++) {
                        uint16_t t = Map[cx + dx][cy + dy];
                        if ((t & LOMASK) != DIRT || (t & (ZONEBIT | ANIMBIT))) ok = false;
                    }
                if (!ok) continue;
                free_dirt++;
                if (TowerSiteOK(cx, cy)) free_river++;
            }
        fprintf(stderr, "  free 3x3 dirt=%d tower-ok=%d\n", free_dirt, free_river);
    }
}

static int32_t PlaceTowerNear(int32_t zx, int32_t zy, int32_t *ox, int32_t *oy)
{
    int32_t best = -1, bx = -1, by = -1;
    for (int32_t cx = 2; cx <= WORLD_X - 3; cx++) {
        for (int32_t cy = 2; cy <= WORLD_Y - 3; cy++) {
            if (!TowerSiteOK(cx, cy)) continue;
            int32_t d = abs(cx - zx) > abs(cy - zy) ? abs(cx - zx) : abs(cy - zy);
            if (best >= 0 && d >= best) continue;
            int32_t tx = -1, ty = -1;
            if (!FindWaterTap(cx, cy, &tx, &ty)) continue;
            best = d;
            bx = cx; by = cy;
        }
    }
    if (bx < 0) { if (getenv("GEN_DEBUG")) printf("  no site\n"); return 0; }
    int32_t r = LayWaterTower(bx, by);
    if (r == -2) return -2;
    if (r != 1) { if (getenv("GEN_DEBUG")) printf("  tower at (%d,%d) refused r=%d\n", bx, by, r); return 0; }
    int32_t w = RouteTowerToWater(bx, by);
    if (w == -2) return -2;
    if (w == 0) { if (getenv("GEN_DEBUG")) printf("  tower at (%d,%d) has no water route\n", bx, by); return 0; }
    *ox = bx; *oy = by;
    return 1;
}

static int32_t CountZoneCenters(int32_t *supplied)
{
    int32_t total = 0;
    *supplied = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            uint16_t t = Map[x][y];
            if (!IsDemandTile(t) || !(t & ZONEBIT)) continue;
            total++;
            if (WaterIsSupplied(x, y)) (*supplied)++;
        }
    }
    return total;
}

static int32_t CountUnsuppliedTiles(void)
{
    int32_t n = 0;
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            if (IsDemandTile(Map[x][y]) && !WaterIsSupplied(x, y)) n++;
    return n;
}

static void DiagConductive(void)
{
    int32_t demand = 0, cond = 0, cond_unsup = 0, nocond_unsup = 0, nocond_sup = 0;
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if (!IsDemandTile(Map[x][y])) continue;
            demand++;
            bool c = (Map[x][y] & CONDBIT) != 0;
            bool s = WaterIsSupplied(x, y);
            if (c) { cond++; if (!s) cond_unsup++; }
            else { if (s) nocond_sup++; else nocond_unsup++; }
        }
    printf("  diag demand=%d condbit=%d condbit_unsupplied=%d no_condbit_supplied=%d no_condbit_unsupplied=%d\n",
           demand, cond, cond_unsup, nocond_sup, nocond_unsup);
}

static bool IsPlantTile(int32_t x, int32_t y)
{
    return TestBounds(x, y) && TILE_IS_POWER_PLANT(Map[x][y]);
}

/* Distance from pipes the water flood has already reached, through passable tiles. Towers always seed. */
static void BuildSuppliedDist(void)
{
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            Dist[x][y] = -1;
    int32_t head = 0, tail = 0;
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            if ((WaterHasPipe(x, y) && WaterIsSupplied(x, y)) || IsWTTile(Map[x][y])) {
                Dist[x][y] = 0;
                Q[tail++] = x * WORLD_Y + y;
            }
    while (head < tail) {
        int32_t idx = Q[head++];
        int32_t x = idx / WORLD_Y, y = idx % WORLD_Y;
        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = x + DX[i], ny = y + DY[i];
            if (Passable(nx, ny) && Dist[nx][ny] < 0) {
                Dist[nx][ny] = Dist[x][y] + 1;
                Q[tail++] = nx * WORLD_Y + ny;
            }
        }
    }
}

/* Pipes every power plant the water flood does not reach yet. Plants never take pipe directly (zone range), so
 * the pipe ends on a passable tile beside the plant. A tower goes beside a plant only if no supplied pipe is in range.
 * Returns pipes laid, -2 if out of funds, -3 if the plant cannot be connected. */
static int32_t ConnectPlants(void)
{
    int32_t total = 0;
    for (int32_t round = 0; round < MAX_ITER; round++) {
        DoWaterScan();
        int32_t px = -1, py = -1;
        for (int32_t x = 0; x < WORLD_X && px < 0; x++)
            for (int32_t y = 0; y < WORLD_Y && px < 0; y++)
                if (IsPlantTile(x, y) && !WaterPlantSupplied(x, y)) { px = x; py = y; }
        if (px < 0) break;

        BuildSuppliedDist();
        int32_t bx = -1, by = -1, bd = 0x7fffffff;
        for (int32_t x = 0; x < WORLD_X; x++) {
            for (int32_t y = 0; y < WORLD_Y; y++) {
                if (Dist[x][y] < 0 || Dist[x][y] >= bd || !Passable(x, y)) continue;
                for (int32_t i = 0; i < 4; i++) {
                    int32_t nx = x + DX[i], ny = y + DY[i];
                    if (IsPlantTile(nx, ny) && abs(nx - px) <= 3 && abs(ny - py) <= 3) {
                        bd = Dist[x][y];
                        bx = x; by = y;
                        break;
                    }
                }
            }
        }

        if (bx < 0) {
            int32_t cx = 0, cy = 0;
            int32_t t = PlaceTowerNear(px, py, &cx, &cy);
            if (t == -2) return -2;
            if (t == 0) return -3;
            int32_t p = ConnectTower(cx, cy);
            if (p == -2) return -2;
            if (p <= 0) return -3;
            total += p;
            continue;
        }
        int32_t laid = LayAlongDist(bx, by);
        if (laid == -2) return -2;
        if (laid <= 0) return -3;
        total += laid;
    }
    return total;
}

/* Mirror COAL_ZONES_SUPPLIED / NUCLEAR_ZONES_SUPPLIED in core/s_power.c; keep them in step. */
#define GEN_COAL_ZONES 24
#define GEN_NUCLEAR_ZONES 48
#define GEN_NODES (2 * WORLD_X * WORLD_Y)
#define PLANT_NODE(g) (WORLD_X * WORLD_Y + (g))

static int32_t Lab[WORLD_X][WORLD_Y];
static int32_t Grp[WORLD_X][WORLD_Y];
static int32_t UFPar[GEN_NODES];
static int32_t NodeDem[GEN_NODES];
static int32_t NodeCap[GEN_NODES];
static bool NodeSkip[GEN_NODES];
static int32_t GrpCap[WORLD_X * WORLD_Y];
static bool GrpWatered[WORLD_X * WORLD_Y];
static int32_t GrpSeedX[WORLD_X * WORLD_Y];
static int32_t GrpSeedY[WORLD_X * WORLD_Y];
static int32_t GrpCount;

/* Same conduction rule as IsConductiveTile in core/s_power.c. */
static bool IsCondTile(int32_t x, int32_t y)
{
    uint16_t t = Map[x][y];
    return (t & CONDBIT) && !TILE_IS_NUCLEAR(t) && ((t & LOMASK) != POWERPLANT);
}

static bool IsZoneCenter(uint16_t t)
{
    return (t & ZONEBIT) && !TILE_IS_POWER_PLANT(t);
}

static int32_t UFFind(int32_t a)
{
    while (UFPar[a] != a) {
        UFPar[a] = UFPar[UFPar[a]];
        a = UFPar[a];
    }
    return a;
}

static void UFUnion(int32_t a, int32_t b)
{
    a = UFFind(a);
    b = UFFind(b);
    if (a != b) UFPar[b] = a;
}

/* Rebuilds conductor networks, joins each watered plant to the networks it touches, and totals demand and capacity per grid.
 * Plants are grouped by 4-connected footprint; a group counts as (tiles / 16) plants. */
static void AnalyzeGrids(void)
{
    DoWaterScan();
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++) { Lab[x][y] = -1; Grp[x][y] = -1; }

    int32_t nc = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if (Lab[x][y] >= 0 || !IsCondTile(x, y)) continue;
            int32_t head = 0, tail = 0;
            Q[tail++] = x * WORLD_Y + y;
            Lab[x][y] = nc;
            while (head < tail) {
                int32_t idx = Q[head++];
                int32_t cx = idx / WORLD_Y, cy = idx % WORLD_Y;
                for (int32_t i = 0; i < 4; i++) {
                    int32_t nx = cx + DX[i], ny = cy + DY[i];
                    if (TestBounds(nx, ny) && Lab[nx][ny] < 0 && IsCondTile(nx, ny)) {
                        Lab[nx][ny] = nc;
                        Q[tail++] = nx * WORLD_Y + ny;
                    }
                }
            }
            nc++;
        }
    }

    GrpCount = 0;
    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            if (Grp[x][y] >= 0 || !IsPlantTile(x, y)) continue;
            int32_t g = GrpCount++;
            GrpSeedX[g] = x;
            GrpSeedY[g] = y;
            int32_t head = 0, tail = 0, units = 0;
            bool nuclear = false;
            Q[tail++] = x * WORLD_Y + y;
            Grp[x][y] = g;
            while (head < tail) {
                int32_t idx = Q[head++];
                int32_t cx = idx / WORLD_Y, cy = idx % WORLD_Y;
                units++;
                if (TILE_IS_NUCLEAR(Map[cx][cy])) nuclear = true;
                for (int32_t i = 0; i < 4; i++) {
                    int32_t nx = cx + DX[i], ny = cy + DY[i];
                    if (IsPlantTile(nx, ny) && Grp[nx][ny] < 0) {
                        Grp[nx][ny] = g;
                        Q[tail++] = nx * WORLD_Y + ny;
                    }
                }
            }
            GrpCap[g] = ((nuclear ? GEN_NUCLEAR_ZONES : GEN_COAL_ZONES) * units + 8) / 16;
            GrpWatered[g] = WaterPlantSupplied(x, y);
        }
    }

    for (int32_t i = 0; i < GEN_NODES; i++) { UFPar[i] = i; NodeDem[i] = 0; NodeCap[i] = 0; NodeSkip[i] = false; }

    for (int32_t x = 0; x < WORLD_X; x++) {
        for (int32_t y = 0; y < WORLD_Y; y++) {
            int32_t g = Grp[x][y];
            if (g < 0 || !GrpWatered[g]) continue;
            for (int32_t i = 0; i < 4; i++) {
                int32_t nx = x + DX[i], ny = y + DY[i];
                if (TestBounds(nx, ny) && Lab[nx][ny] >= 0) UFUnion(PLANT_NODE(g), Lab[nx][ny]);
            }
        }
    }

    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            if (Lab[x][y] >= 0 && IsZoneCenter(Map[x][y])) NodeDem[UFFind(Lab[x][y])]++;
    for (int32_t g = 0; g < GrpCount; g++)
        if (GrpWatered[g]) NodeCap[UFFind(PLANT_NODE(g))] += GrpCap[g];
}

static int32_t ShortGrids(int32_t *unpowered)
{
    AnalyzeGrids();
    int32_t short_grids = 0;
    for (int32_t i = 0; i < GEN_NODES; i++)
        if (UFFind(i) == i && NodeDem[i] > NodeCap[i]) {
            short_grids++;
            if (getenv("GEN_DEBUG")) {
                int32_t fx = -1, fy = -1;
                for (int32_t x = 0; x < WORLD_X && fx < 0; x++)
                    for (int32_t y = 0; y < WORLD_Y && fx < 0; y++)
                        if (Lab[x][y] >= 0 && UFFind(Lab[x][y]) == i) { fx = x; fy = y; }
                fprintf(stderr, "  short grid root=%d dem=%d cap=%d skip=%d first_tile=(%d,%d)\n",
                        i, NodeDem[i], NodeCap[i], (int)NodeSkip[i], fx, fy);
            }
        }
    *unpowered = 0;
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            if (Lab[x][y] < 0 && IsZoneCenter(Map[x][y])) (*unpowered)++;
    return short_grids;
}

static int32_t WireDist[WORLD_X][WORLD_Y];
static int32_t WirePar[WORLD_X][WORLD_Y];

/* Tiles where LayWire succeeds without water or river work: bare dirt and the plain road and rail crossings. */
static bool WireOK(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    uint16_t t = Map[x][y];
    if (t & (ZONEBIT | CONDBIT)) return false;
    uint16_t lo = t & LOMASK;
    if (lo >= 64 && lo <= 207) {
        uint16_t n = (uint16_t)((lo & 0x000F) + 64);
        return n == 66 || n == 67;
    }
    return lo == DIRT || lo == 226 || lo == 227;
}

/* Already-conductive tiles (zones, existing lines) carry power, so the router passes through them without laying wire.
 * Mirrors IsConductiveTile in core/s_power.c. */
static bool WireThrough(int32_t x, int32_t y)
{
    if (!TestBounds(x, y)) return false;
    uint16_t t = Map[x][y];
    return (t & CONDBIT) && !TILE_IS_NUCLEAR(t) && ((t & LOMASK) != POWERPLANT);
}

static bool SiteFree(int32_t x, int32_t y)
{
    for (int32_t dy = -1; dy <= 2; dy++)
        for (int32_t dx = -1; dx <= 2; dx++) {
            uint16_t t = Map[x + dx][y + dy];
            if ((t & LOMASK) != DIRT || (t & (ZONEBIT | ANIMBIT))) return false;
        }
    return true;
}

/* Lays a power line from grid root r over WireOK tiles to a free 4x4 site, keeping the line off the footprint.
 * Returns 1 laid, 0 no route, -2 out of funds. */
static int32_t LayWireToSite(int32_t r, int32_t *ox, int32_t *oy)
{
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++) { WireDist[x][y] = -1; WirePar[x][y] = -1; }
    int32_t head = 0, tail = 0;
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            if (Lab[x][y] >= 0 && UFFind(Lab[x][y]) == r) { WireDist[x][y] = 0; Q[tail++] = x * WORLD_Y + y; }
    while (head < tail) {
        int32_t idx = Q[head++];
        int32_t cx = idx / WORLD_Y, cy = idx % WORLD_Y;
        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = cx + DX[i], ny = cy + DY[i];
            if ((!WireOK(nx, ny) && !WireThrough(nx, ny)) || WireDist[nx][ny] >= 0) continue;
            WireDist[nx][ny] = WireDist[cx][cy] + 1;
            WirePar[nx][ny] = idx;
            Q[tail++] = nx * WORLD_Y + ny;
        }
    }

    if (getenv("GEN_DEBUG")) {
        int32_t reached = 0, sites = 0, boundary[256] = {0};
        for (int32_t x = 0; x < WORLD_X; x++)
            for (int32_t y = 0; y < WORLD_Y; y++) {
                if (WireDist[x][y] >= 0) reached++;
                if (x >= 2 && x <= WORLD_X - 4 && y >= 2 && y <= WORLD_Y - 4 && SiteFree(x, y)) sites++;
            }
        for (int32_t x = 0; x < WORLD_X; x++)
            for (int32_t y = 0; y < WORLD_Y; y++) {
                if (WireDist[x][y] < 0) continue;
                for (int32_t i = 0; i < 4; i++) {
                    int32_t nx = x + DX[i], ny = y + DY[i];
                    if (TestBounds(nx, ny) && WireDist[nx][ny] < 0) boundary[Map[nx][ny] & LOMASK & 255]++;
                }
            }
        fprintf(stderr, "  wire root=%d reached=%d free4x4=%d\n", r, reached, sites);
        for (int32_t k = 0; k < 256; k++) if (boundary[k]) fprintf(stderr, "    boundary tile %d x%d\n", k, boundary[k]);
    }

    int32_t best = 0x7fffffff, bx = -1, by = -1, bxr = -1, byr = -1;
    for (int32_t x = 2; x <= WORLD_X - 4; x++) {
        for (int32_t y = 2; y <= WORLD_Y - 4; y++) {
            if (!SiteFree(x, y)) continue;
            for (int32_t k = -1; k <= 2; k++) {
                int32_t ring[4][2] = { { x + k, y - 2 }, { x + k, y + 3 }, { x - 2, y + k }, { x + 3, y + k } };
                for (int32_t j = 0; j < 4; j++) {
                    int32_t rx = ring[j][0], ry = ring[j][1];
                    if (!TestBounds(rx, ry) || WireDist[rx][ry] <= 0 || WireDist[rx][ry] >= best) continue;
                    bool clear = true;
                    for (int32_t cx = rx, cy = ry; WireDist[cx][cy] > 0 && clear; ) {
                        if (cx >= x - 1 && cx <= x + 2 && cy >= y - 1 && cy <= y + 2) clear = false;
                        int32_t p = WirePar[cx][cy];
                        cx = p / WORLD_Y; cy = p % WORLD_Y;
                    }
                    if (!clear) continue;
                    best = WireDist[rx][ry];
                    bx = x; by = y; bxr = rx; byr = ry;
                }
            }
        }
    }
    if (bx < 0) return 0;

    for (int32_t cx = bxr, cy = byr; WireDist[cx][cy] > 0; ) {
        if (!WireThrough(cx, cy) && LayWire(cx, cy) == -2) return -2;
        int32_t p = WirePar[cx][cy];
        cx = p / WORLD_Y; cy = p % WORLD_Y;
    }
    *ox = bx;
    *oy = by;
    return 1;
}

/* A 4x4 nuclear footprint at (x,y) covers x-1..x+2, y-1..y+2 (as PlaceZone does). It must be bare dirt and must touch a conductor of grid root r. */
static bool FindPlantSite(int32_t r, int32_t *ox, int32_t *oy)
{
    int32_t best = 0x7fffffff, bx = -1, by = -1;
    for (int32_t x = 2; x <= WORLD_X - 4; x++) {
        for (int32_t y = 2; y <= WORLD_Y - 4; y++) {
            if (!SiteFree(x, y)) continue;

            bool touches = false;
            int32_t score = 0x7fffffff;
            for (int32_t k = -1; k <= 2; k++) {
                int32_t ring[4][2] = { { x + k, y - 2 }, { x + k, y + 3 }, { x - 2, y + k }, { x + 3, y + k } };
                for (int32_t j = 0; j < 4; j++) {
                    int32_t rx = ring[j][0], ry = ring[j][1];
                    if (!TestBounds(rx, ry)) continue;
                    if (Lab[rx][ry] >= 0 && UFFind(Lab[rx][ry]) == r) touches = true;
                    if (Dist[rx][ry] >= 0 && Dist[rx][ry] < score) score = Dist[rx][ry];
                }
            }
            if (!touches) continue;
            if (score < best) { best = score; bx = x; by = y; }
        }
    }
    if (bx < 0) return false;
    *ox = bx;
    *oy = by;
    return true;
}

/* Places one nuclear plant on the grid with the largest shortfall. Returns 1 placed, 0 nothing to do, -1 no site, -2 out of funds. */
static int32_t PlaceNuclearForDeficit(void)
{
    AnalyzeGrids();
    BuildSuppliedDist();
    for (;;) {
        int32_t target = -1, worst = 0;
        for (int32_t i = 0; i < GEN_NODES; i++) {
            if (UFFind(i) != i || NodeSkip[i]) continue;
            int32_t def = NodeDem[i] - NodeCap[i];
            if (def > worst) { worst = def; target = i; }
        }
        if (target < 0) return 0;

        int32_t ox = -1, oy = -1;
        if (!FindPlantSite(target, &ox, &oy)) {
            int32_t w = LayWireToSite(target, &ox, &oy);
            if (w == -2) return -2;
            if (w == 0) { if (getenv("GEN_DEBUG")) fprintf(stderr, "  skip node %d def=%d: no site/wire\n", target, worst); NodeSkip[target] = true; continue; }
        }
        int32_t r = PlaceZone(ox, oy, nuclearState);
        if (r == -2) return -2;
        if (r != 1) { if (getenv("GEN_DEBUG")) fprintf(stderr, "  skip node %d def=%d: place r=%d at (%d,%d)\n", target, worst, r, ox, oy); NodeSkip[target] = true; continue; }
        return 1;
    }
}

static uint8_t Reach[WORLD_X][WORLD_Y];

/* Tiles the game's uncapped water flood can reach from the existing towers: pipes, footprints, and conductive demand tiles. */
static void BuildReach(void)
{
    memset(Reach, 0, sizeof(Reach));
    int32_t head = 0, tail = 0;
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            if (IsWTTile(Map[x][y])) { Reach[x][y] = 1; Q[tail++] = x * WORLD_Y + y; }
    while (head < tail) {
        int32_t idx = Q[head++];
        int32_t x = idx / WORLD_Y, y = idx % WORLD_Y;
        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = x + DX[i], ny = y + DY[i];
            if (!TestBounds(nx, ny) || Reach[nx][ny]) continue;
            if (WaterHasPipe(nx, ny) || (IsDemandTile(Map[nx][ny]) && (Map[nx][ny] & CONDBIT))) {
                Reach[nx][ny] = 1;
                Q[tail++] = nx * WORLD_Y + ny;
            }
        }
    }
}

/* Distance from the network (reached pipes and footprints) through passable tiles. */
static void BuildNetworkDist(void)
{
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            Dist[x][y] = -1;
    int32_t head = 0, tail = 0;
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            if (Reach[x][y] && (WaterHasPipe(x, y) || IsWTTile(Map[x][y]))) {
                Dist[x][y] = 0;
                Q[tail++] = x * WORLD_Y + y;
            }
    while (head < tail) {
        int32_t idx = Q[head++];
        int32_t x = idx / WORLD_Y, y = idx % WORLD_Y;
        for (int32_t i = 0; i < 4; i++) {
            int32_t nx = x + DX[i], ny = y + DY[i];
            if (Passable(nx, ny) && Dist[nx][ny] < 0) {
                Dist[nx][ny] = Dist[x][y] + 1;
                Q[tail++] = nx * WORLD_Y + ny;
            }
        }
    }
}

/* Pipes every demand block that the network cannot reach yet. Returns pipes laid, -2 if out of funds. */
static int32_t ConnectAllZones(void)
{
    int32_t total = 0;
    for (int32_t round = 0; round < MAX_ITER; round++) {
        BuildReach();
        BuildNetworkDist();
        int32_t bx = -1, by = -1, bd = 0x7fffffff;
        for (int32_t x = 0; x < WORLD_X; x++) {
            for (int32_t y = 0; y < WORLD_Y; y++) {
                if (Dist[x][y] < 0 || Dist[x][y] >= bd || !Passable(x, y)) continue;
                for (int32_t i = 0; i < 4; i++) {
                    int32_t nx = x + DX[i], ny = y + DY[i];
                    if (TestBounds(nx, ny) && IsDemandTile(Map[nx][ny]) && !Reach[nx][ny]) {
                        bd = Dist[x][y];
                        bx = x; by = y;
                        break;
                    }
                }
            }
        }
        if (bx < 0) break;
        int32_t laid = LayAlongDist(bx, by);
        if (laid == -2) return -2;
        if (laid <= 0) break;
        total += laid;
    }
    return total;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: gen_scn2 IN.scn OUT_ROOT DEDUCT(0|1)\n");
        return 2;
    }
    const char *in = argv[1];
    const char *out_root = argv[2];
    int32_t deduct = atoi(argv[3]);

    AllocateMapBuffers();

    ScenarioDef def;
    if (!Scenario_ReadDef(in, &def)) { fprintf(stderr, "bad header %s\n", in); return 1; }
    if (!Scenario_LoadFile(in)) { fprintf(stderr, "load failed %s\n", in); return 1; }
    if (getenv("GEN_UNLIMITED")) TotalFunds = 1000000000;
    if (getenv("GEN_DESC")) {
        strncpy(def.description, getenv("GEN_DESC"), SCN_DESC_BYTES - 1);
        def.description[SCN_DESC_BYTES - 1] = '\0';
    }

    int32_t start_funds = TotalFunds;
    int32_t sup0, zc0 = CountZoneCenters(&sup0);
    int32_t towers = 0, pipes = 0, skipped = 0, landfills = 0, out_of_funds = 0, plant_unreached = 0, plant_site_failed = 0;

    /* Phase 1: a first tower beside the populated area, so there is a source to pipe from. */
    if (!WaterHasPipe(0, 0)) {
        int32_t fzx = -1, fzy = -1;
        for (int32_t x = 0; x < WORLD_X && fzx < 0; x++)
            for (int32_t y = 0; y < WORLD_Y && fzx < 0; y++)
                if (IsDemandTile(Map[x][y])) { fzx = x; fzy = y; }
        int32_t cx = 0, cy = 0;
        if (fzx >= 0 && PlaceTowerNear(fzx, fzy, &cx, &cy) == 1) towers++;
    }

    /* Phase 2: every populated block is piped to the network, whether or not a tower covers it yet. */
    int32_t cp = ConnectAllZones();
    if (cp == -2) out_of_funds = 1;
    else pipes += cp;

    /* Phase 3: towers on every remaining site, each near a block the network still leaves dry. */
    for (int32_t iter = 0; iter < MAX_ITER && !out_of_funds; iter++) {
        DoWaterScan();
        int32_t zx = -1, zy = -1;
        for (int32_t x = 0; x < WORLD_X && zx < 0; x++)
            for (int32_t y = 0; y < WORLD_Y && zx < 0; y++)
                if (Unsupplied(x, y) && Reach[x][y]) { zx = x; zy = y; }
        if (zx < 0) break;

        int32_t cx = 0, cy = 0;
        int32_t t = PlaceTowerNear(zx, zy, &cx, &cy);
        if (t == -2) { out_of_funds = 1; break; }
        if (t == 0) break;
        towers++;

        int32_t p = ConnectTower(cx, cy);
        if (p == -2) { out_of_funds = 1; break; }
        if (p > 0) pipes += p;
        if (getenv("GEN_DEBUG")) {
            DoWaterScan();
            int32_t s, z = CountZoneCenters(&s);
            printf("  tower#%d at (%d,%d) joined=%d pipes_total=%d supplied=%d/%d\n",
                   towers, cx, cy, p, pipes, s, z);
        }
    }

    SiteDebug();
    /* Phase 4: every power plant gets water, so its grid can generate power. */
    if (getenv("GEN_DEBUG")) {
        DoWaterScan();
        BuildReach();
        BuildNetworkDist();
        int32_t dry = 0, dry_reach = 0, dry_cond = 0, dry_skip = 0, dry_touch = 0, dry_touch_sat = 0;
        for (int32_t x = 0; x < WORLD_X; x++)
            for (int32_t y = 0; y < WORLD_Y; y++) {
                if (!IsZoneCenter(Map[x][y]) || WaterIsSupplied(x, y)) continue;
                dry++;
                if (Reach[x][y]) dry_reach++;
                if (Map[x][y] & CONDBIT) dry_cond++;
                if (Skip[x][y]) dry_skip++;
                bool t = false;
                for (int32_t dx = -1; dx <= 1; dx++)
                    for (int32_t dy = -1; dy <= 1; dy++)
                        if (TestBounds(x + dx, y + dy) && Dist[x + dx][y + dy] >= 0) t = true;
                if (t) dry_touch++;
                if (t && Reach[x][y]) dry_touch_sat++;
            }
        fprintf(stderr, "  dry zones=%d reach=%d cond=%d skip=%d near_dist=%d near_dist_reach=%d\n",
                dry, dry_reach, dry_cond, dry_skip, dry_touch, dry_touch_sat);
    }

    if (!out_of_funds) {
        int32_t cpl = ConnectPlants();
        if (cpl == -2) out_of_funds = 1;
        else if (cpl == -3) plant_unreached = 1;
        else pipes += cpl;
    }

    /* Phase 5: nuclear plants until every grid's watered capacity covers its zones. */
    int32_t plants_added = 0;
    if (!out_of_funds) {
        for (int32_t iter = 0; iter < MAX_ITER; iter++) {
            int32_t r = PlaceNuclearForDeficit();
            if (r == 0) break;
            if (r == -2) { out_of_funds = 1; break; }
            if (r < 0) { plant_site_failed = 1; break; }
            plants_added++;
            int32_t cpl = ConnectPlants();
            if (cpl == -2) { out_of_funds = 1; break; }
            if (cpl == -3) { plant_unreached = 1; break; }
            pipes += cpl;
        }
    }

    /* Landfill: a starting allowance so trash has somewhere to go. The game expands it when full. */
    int32_t want_lf = (TotalPop + 299) / 300;
    if (want_lf < 2) want_lf = 2;
    for (int32_t n = 0; n < want_lf; n++) {
        int32_t bx = -1, by = -1, bscore = -1;
        for (int32_t cx = 2; cx < WORLD_X - 2; cx++) {
            for (int32_t cy = 2; cy < WORLD_Y - 2; cy++) {
                int32_t dirt_ok = 1, pipe_ok = 1;
                for (int32_t dy = -1; dy <= 1 && dirt_ok; dy++)
                    for (int32_t dx = -1; dx <= 1; dx++) {
                        if (Map[cx + dx][cy + dy] != DIRT) dirt_ok = 0;
                        if (WaterHasPipe(cx + dx, cy + dy)) pipe_ok = 0;
                    }
                if (!dirt_ok || !pipe_ok) continue;
                int32_t road = 0;
                for (int32_t i = 0; i < 12; i++) {
                    int32_t ex = cx + EDGE[i][0], ey = cy + EDGE[i][1];
                    if (TestBounds(ex, ey) && RoadTest(Map[ex][ey])) { road = 1; break; }
                }
                if (!road) continue;
                int32_t zd = LF_ZONE_WANT + 1;
                for (int32_t dy = -LF_ZONE_WANT; dy <= LF_ZONE_WANT; dy++)
                    for (int32_t dx = -LF_ZONE_WANT; dx <= LF_ZONE_WANT; dx++) {
                        int32_t zxx = cx + dx, zyy = cy + dy;
                        if (TestBounds(zxx, zyy) && IsDemandTile(Map[zxx][zyy])) {
                            int32_t d = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
                            if (d < zd) zd = d;
                        }
                    }
                if (zd > bscore) { bscore = zd; bx = cx; by = cy; }
            }
        }
        if (bx < 0) break;
        if (PlaceZone(bx, by, landfillState) != 1) break;
        landfills++;
    }

    CityRescan();
    int32_t sup1, zc1 = CountZoneCenters(&sup1);
    int32_t unsup_tiles = CountUnsuppliedTiles();
    int32_t plants_total = 0, plants_watered = 0;
    DoWaterScan();
    for (int32_t x = 0; x < WORLD_X; x++)
        for (int32_t y = 0; y < WORLD_Y; y++)
            if (IsPlantTile(x, y)) { plants_total++; if (WaterPlantSupplied(x, y)) plants_watered++; }
    if (getenv("GEN_DEBUG")) DiagConductive();
    int32_t spent = start_funds - TotalFunds;
    int32_t grids_short = 0, unpowered = 0;
    grids_short = ShortGrids(&unpowered);
    if (deduct) def.funds = def.funds - spent;

    char saved[1200] = "";
    SetDataRoot(out_root);
    int32_t ok = Scenario_SaveToDirectory(&def, saved, sizeof(saved));

    printf("%s: towers=%d pipes=%d skipped=%d landfill=%d spent=%d funds_left=%d "
           "centers_before=%d/%d centers_after=%d/%d unsupplied_demand_tiles=%d plants_watered=%d/%d "
           "plants_added=%d grids_short=%d unpowered_zones=%d%s%s %s%s%s\n",
           in, towers, pipes, skipped, landfills, spent, TotalFunds,
           sup0, zc0, sup1, zc1, unsup_tiles, plants_watered, plants_total,
           plants_added, grids_short, unpowered,
           plant_unreached ? " (plant unreachable)" : "",
           plant_site_failed ? " (no plant site)" : "",
           ok ? "saved=" : "SAVE FAILED", ok ? saved : "",
           out_of_funds ? " (stopped: out of funds)" : "");
    return ok ? 0 : 1;
}
