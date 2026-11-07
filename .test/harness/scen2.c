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
        int32_t rc = Scenario_LoadFile(argv[a]);
        int plants0 = 0;
        for (int x = 0; x < WORLD_X; x++) for (int y = 0; y < WORLD_Y; y++) {
            uint16_t c = Map[x][y] & LOMASK;
            if (c == POWERPLANT || c == NUCLEAR) plants0++;
        }
        NoDisasters = 1; SimSpeed = 3; Fcycle = 0; Scycle = 0;
        int32_t pop0 = TotalPop; int zones0=0; for(int x=0;x<WORLD_X;x++)for(int y=0;y<WORLD_Y;y++) if(Map[x][y]&ZONEBIT) zones0++;
        for (int s = 0; s < 40000; s++) {
            if (++Fcycle > 1023) Fcycle = 0;
            Simulate(Fcycle & 15);
        }
        int plants1 = 0; int32_t dsum = 0, csum = 0;
        for (int x = 0; x < WORLD_X; x++) for (int y = 0; y < WORLD_Y; y++) {
            uint16_t c = Map[x][y] & LOMASK;
            if (c == POWERPLANT || c == NUCLEAR) {
                plants1++;
                int32_t d = 0, cap = 0;
                if (PowerGridStats(x, y, &d, &cap)) { dsum += d; csum += cap; }
            }
        }
        int zones1=0; for(int x=0;x<WORLD_X;x++)for(int y=0;y<WORLD_Y;y++) if(Map[x][y]&ZONEBIT) zones1++;
        printf("zones %d->%d  ", zones0, zones1);
        printf("%-18s rc=%d pop0=%d popEnd=%d plants %d->%d  demand/capacity (per plant grid sum) %d/%d\n",
               nm, rc, pop0, TotalPop, plants0, plants1, dsum, csum);
    }
    return 0;
}
