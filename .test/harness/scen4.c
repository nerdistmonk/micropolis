#include <stdio.h>
#include <string.h>
#include "sim.h"
#include "scenario.h"
void Audio_MakeSound(const char *c, const char *s) { (void)c; (void)s; }
extern int16_t Fcycle, Scycle, NewPower;
int main(int argc, char **argv) {
    AllocateMapBuffers();
    Scenario_LoadFile(argv[1]);
    NoDisasters = 1; SimSpeed = 3; Fcycle = 0; Scycle = 0;
    int32_t period = argc > 2 ? atoi(argv[2]) : 4000;
    int32_t total = argc > 3 ? atoi(argv[3]) : 40000;
    for (int s = 0; s < total; s++) {
        if (++Fcycle > 1023) Fcycle = 0;
        Simulate(Fcycle & 15);
        if (s % period == period - 1) {
            int plants = 0, zones = 0, watered_zones = 0, roads = 0;
            for (int x = 0; x < WORLD_X; x++) for (int y = 0; y < WORLD_Y; y++) {
                uint16_t c = Map[x][y] & LOMASK;
                if (c == POWERPLANT || c == NUCLEAR) plants++;
                if (Map[x][y] & ZONEBIT) { zones++; if (WaterIsSupplied(x, y)) watered_zones++; }
                if (c >= ROADBASE && c <= LASTROAD) roads++;
            }
            printf("cyc=%6d pop=%5d res=%5d com=%4d ind=%4d zones=%4d watered=%4d plants=%2d roads=%5d pwrd=%4d unpwrd=%4d unwatered=%4d\n",
                   s + 1, TotalPop, ResPop, ComPop, IndPop, zones, watered_zones, plants, roads,
                   PwrdZCnt, unPwrdZCnt, UnwateredZCnt);
        }
    }
    return 0;
}
