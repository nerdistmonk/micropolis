#include <stdio.h>
#include <string.h>
#include "sim.h"
#include "scenario.h"
void Audio_MakeSound(const char *c, const char *s) { (void)c; (void)s; }
extern int16_t Fcycle, Scycle, NewPower;
int main(int argc, char **argv) {
    AllocateMapBuffers();
    for (int a = 1; a < argc; a++) {
        const char *nm = strrchr(argv[a], '/') ? strrchr(argv[a], '/') + 1 : argv[a];
        Scenario_LoadFile(argv[a]);
        NoDisasters = 1; SimSpeed = 3; Fcycle = 0; Scycle = 0;
        DoWaterScan();
        DoPowerScan();
        int plants = 0, watered = 0, short_plants = 0;
        printf("== %s pop=%d\n", nm, TotalPop);
        for (int x = 0; x < WORLD_X; x++) for (int y = 0; y < WORLD_Y; y++) {
            uint16_t c = Map[x][y] & LOMASK;
            if (c == POWERPLANT || c == NUCLEAR) {
                int32_t d = 0, cap = 0;
                plants++;
                if (WaterPlantSupplied(x, y)) watered++;
                if (PowerGridStats(x, y, &d, &cap)) {
                    if (d > cap) short_plants++;
                    if (d > cap) printf("  plant(%d,%d) %s demand=%d cap=%d\n", x, y, c == NUCLEAR ? "nuc" : "coal", d, cap);
                }
            }
        }
        printf("  plants=%d watered=%d short_grid_plants=%d\n", plants, watered, short_plants);
    }
    return 0;
}
