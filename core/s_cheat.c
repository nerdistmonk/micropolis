/* s_cheat.c: typed cheat codes ported from Unix w_keys.c (fund, fart, nuke).
 */

#include "sim.h"
#include <string.h>

extern void Audio_MakeSound(const char *channel, const char *sound_name);

/* Bridge tile groups used only by nuke; not defined in sim.h */
#define BRWXXX1 111
#define BRWXXX2 127
#define BRWXXX3 143
#define BRWXXX4 159
#define BRWXXX5 175
#define BRWXXX6 191
#define BRWXXX7 207

static char last_keys[5] = "    ";
static int32_t punish_count = 0;

void Cheat_Reset(void)
{
    strcpy(last_keys, "    ");
    punish_count = 0;
}

static void CheatFund(void)
{
    TotalFunds += 10000;
    MustUpdateFunds = 1;
    if (EngineCallbacks.update_funds) {
        EngineCallbacks.update_funds(TotalFunds);
    }

    /* Repeated cheating brings an earthquake */
    punish_count++;
    if (punish_count == 5) {
        punish_count = 0;
        MakeEarthquake();
    }
}

static void CheatFart(void)
{
    Audio_MakeSound("city", "Explosion-High");
    Audio_MakeSound("city", "Explosion-Low");
    MakeFire();
    MakeFlood();
    MakeTornado();
    MakeEarthquake();
    MakeMonster();
}

static bool IsBridgeTile(int32_t tile)
{
    static const int32_t bridge_groups[] = {
        BRWXXX1, BRWXXX2, BRWXXX3, BRWXXX4, BRWXXX5, BRWXXX6, BRWXXX7, BRWV
    };

    if (tile >= HBRIDGE && tile <= VBRIDGE) return true;
    if (tile >= BRWH && tile <= LTRFBASE + 1) return true;
    for (size_t i = 0; i < sizeof(bridge_groups) / sizeof(bridge_groups[0]); i++) {
        if (tile >= bridge_groups[i] && tile <= bridge_groups[i] + 2) return true;
    }
    return false;
}

static void CheatNuke(void)
{
    Audio_MakeSound("city", "Explosion-High");
    Audio_MakeSound("city", "Explosion-Low");

    for (int32_t i = 0; i < WORLD_X; i++) {
        for (int32_t j = 0; j < WORLD_Y; j++) {
            int32_t tile = Map[i][j] & LOMASK;
            if (tile < RUBBLE) continue;
            if (tile >= CHURCH - 4 && tile <= CHURCH + 4) continue;

            if (IsBridgeTile(tile)) {
                Map[i][j] = RIVER;
            } else {
                Map[i][j] = TINYEXP + ANIMBIT + BULLBIT + Rand(2);
            }
        }
    }
}

/* Mirrors Unix doKeyDown: keys are shifted through a 4-char window and
 * matched after each press. Non-letter keys should be passed as ' '. */
void Cheat_KeyPressed(char key)
{
    last_keys[0] = last_keys[1];
    last_keys[1] = last_keys[2];
    last_keys[2] = last_keys[3];
    last_keys[3] = key;

    if (strcmp(last_keys, "fund") == 0) {
        CheatFund();
        last_keys[0] = '\0';
    } else if (strcmp(last_keys, "fart") == 0) {
        CheatFart();
        last_keys[0] = '\0';
    } else if (strcmp(last_keys, "nuke") == 0) {
        CheatNuke();
        last_keys[0] = '\0';
    }
}
