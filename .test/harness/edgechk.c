#include "sim.h"
#include "scenario.h"
#include <stdio.h>
#include <stdlib.h>
int Scenario_LoadFile(const char *);
void DoPowerScan(void); void DoWaterScan(void); void Simulate(int);
extern int16_t Fcycle, Scycle;
void Audio_MakeSound(const char *c, const char *s) { (void)c; (void)s; }
int main(int argc, char **argv) {
    AllocateMapBuffers();
    Scenario_LoadFile(argv[1]);
    NoDisasters = 1; SimSpeed = 3; Fcycle = 0; Scycle = 0;
    for (int s = 0; s < 3000; s++) { if (++Fcycle > 1023) Fcycle = 0; Simulate(Fcycle & 15); }
    for (int x = 0; x < WORLD_X; x++) for (int y = 0; y < WORLD_Y; y++) {
        if ((Map[x][y] & LOMASK) != WATERTOWER + 4) continue;
        int any = 0; char buf[256]; int n = 0;
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int edge = (abs(dx) == 2 && abs(dy) <= 1) || (abs(dy) == 2 && abs(dx) <= 1);
            if (!edge) continue;
            int tx = x + dx, ty = y + dy;
            if (!TestBounds(tx, ty)) continue;
            uint16_t t = Map[tx][ty];
            int pb = PowerMap[POWERWORD(tx, ty)] & (1u << (tx & 15)) ? 1 : 0;
            n += snprintf(buf + n, sizeof(buf) - n, " (%d,%d)t=%d c=%d p=%d", tx, ty, t & LOMASK, (t & CONDBIT) ? 1 : 0, pb);
            if (pb) any = 1;
        }
        printf("tower(%d,%d) powered=%d%s\n", x, y, any, buf);
    }
    return 0;
}
