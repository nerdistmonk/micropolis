#include <stdio.h>
#include "sim.h"
#include "scenario.h"
void Audio_MakeSound(const char *c, const char *s) { (void)c; (void)s; }
extern int16_t Fcycle, Scycle, NewPower;
int main(int argc, char **argv) {
    AllocateMapBuffers();
    Scenario_LoadFile(argv[1]);
    int towers = 0, plants = 0, rivers = 0;
    for (int x = 0; x < WORLD_X; x++) for (int y = 0; y < WORLD_Y; y++) {
        uint16_t c = Map[x][y] & LOMASK;
        if (c == WATERTOWER) towers++;
        if (c == POWERPLANT || c == NUCLEAR) plants++;
        if (c == RIVER) rivers++;
    }
    printf("towers=%d plants=%d rivers=%d \n", towers, plants, rivers);
    NoDisasters = 1; SimSpeed = 3; Fcycle = 0; Scycle = 0;
    for (int s = 0; s < 3000; s++) { if (++Fcycle > 1023) Fcycle = 0; Simulate(Fcycle & 15); }
    for (int x = 0; x < WORLD_X; x++) for (int y = 0; y < WORLD_Y; y++)
        if ((Map[x][y] & LOMASK) == WATERTOWER) { int32_t d=-1,c=-1; int ok=WaterNetworkStats(x,y,&d,&c); printf("tower(%d,%d) src=%d net ok=%d d=%d c=%d\n", x, y, WaterTowerHasSource(x, y), ok, d, c); }
    for (int x = 0; x < WORLD_X; x++) for (int y = 0; y < WORLD_Y; y++) if ((Map[x][y] & LOMASK) == POWERPLANT) printf("plant(%d,%d) watered=%d\n", x, y, WaterPlantSupplied(x, y));
    return 0;
}
