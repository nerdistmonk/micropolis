/* s_disast.c: Disasters engine (fires, floods, earthquakes, tornadoes, meltdowns, monsters).
 *
 * Modernized C11 implementation ported from Micropolis.
 */

#include "sim.h"

int16_t ShakeNow = 0;
int16_t FloodCnt = 0;
int16_t FloodX = 0;
int16_t FloodY = 0;

extern void FireZone(int32_t xloc, int32_t yloc, int32_t ch);
extern void DoMeltdown(int32_t sx, int32_t sy);
extern void ClearMes(void);
extern void SendMesAt(int16_t msg, int16_t x, int16_t y);
extern void Audio_MakeSound(const char *channel, const char *sound_name);

static bool Vulnerable(uint16_t tile)
{
    uint16_t low = tile & LOMASK;
    if (low < RESBASE || low > LASTZONE || (tile & ZONEBIT)) {
        return false;
    }
    return true;
}

void MakeExplosion(int32_t x, int32_t y)
{
    if (TestBounds(x, y)) {
        MakeExplosionAt((x << 4) + 8, (y << 4) + 8);
    }
    if (EngineCallbacks.play_sound) {
        EngineCallbacks.play_sound(EXP, (int16_t)x, (int16_t)y);
    }
}

void SetFire(void)
{
    int32_t x = Rand(WORLD_X - 1);
    int32_t y = Rand(WORLD_Y - 1);
    uint16_t z = Map[x][y];

    if (!(z & ZONEBIT)) {
        uint16_t low = z & LOMASK;
        if (low > LHTHR && low < LASTZONE) {
            Map[x][y] = (uint16_t)(FIRE + ANIMBIT + (Rand16() & 7));
            CrashX = (int16_t)x;
            CrashY = (int16_t)y;
            SendMesAt(-20, (int16_t)x, (int16_t)y);
            Audio_MakeSound("city", "fire");
        }
    }
}

void MakeFire(void)
{
    for (int32_t t = 0; t < 40; t++) {
        int32_t x = Rand(WORLD_X - 1);
        int32_t y = Rand(WORLD_Y - 1);
        uint16_t z = Map[x][y];

        if (!(z & ZONEBIT) && (z & BURNBIT)) {
            uint16_t low = z & LOMASK;
            if (low > 21 && low < LASTZONE) {
                Map[x][y] = (uint16_t)(FIRE + ANIMBIT + (Rand16() & 7));
                SendMesAt(20, (int16_t)x, (int16_t)y);
                Audio_MakeSound("city", "fire");
                return;
            }
        }
    }
}

void MakeFlood(void)
{
    static const int16_t Dx[4] = { 0, 1, 0, -1 };
    static const int16_t Dy[4] = { -1, 0, 1, 0 };

    for (int32_t z = 0; z < 300; z++) {
        int32_t x = Rand(WORLD_X - 1);
        int32_t y = Rand(WORLD_Y - 1);
        uint16_t c = Map[x][y] & LOMASK;

        /* If river edge tile */
        if (c > 4 && c < 21) {
            for (int32_t t = 0; t < 4; t++) {
                int32_t xx = x + Dx[t];
                int32_t yy = y + Dy[t];

                if (TestBounds(xx, yy)) {
                    uint16_t target = Map[xx][yy];
                    if (target == DIRT || ((target & BULLBIT) && (target & BURNBIT))) {
                        Map[xx][yy] = FLOOD;
                        FloodCnt = 30;
                        FloodX = (int16_t)xx;
                        FloodY = (int16_t)yy;
                        SendMesAt(-42, (int16_t)xx, (int16_t)yy);
                        Audio_MakeSound("city", "siren");
                        return;
                    }
                }
            }
        }
    }
}

void DoFlood(void)
{
    static const int16_t Dx[4] = { 0, 1, 0, -1 };
    static const int16_t Dy[4] = { -1, 0, 1, 0 };

    if (FloodCnt > 0) {
        for (int32_t z = 0; z < 4; z++) {
            if (!(Rand16() & 7)) {
                int32_t xx = SMapX + Dx[z];
                int32_t yy = SMapY + Dy[z];

                if (TestBounds(xx, yy)) {
                    uint16_t c = Map[xx][yy];
                    uint16_t t = c & LOMASK;

                    if ((c & BURNBIT) || c == 0 || (t >= WOODS5 && t < FLOOD)) {
                        if (c & ZONEBIT) {
                            FireZone(xx, yy, (int32_t)c);
                        }
                        Map[xx][yy] = (uint16_t)(FLOOD + Rand(2));
                    }
                }
            }
        }
    } else {
        if (!(Rand16() & 15)) {
            Map[SMapX][SMapY] = DIRT;
        }
    }
}

void MakeEarthquake(void)
{
    ShakeNow = 40; /* Viewport tremors */

    SendMesAt(-23, CCx, CCy);
    Audio_MakeSound("city", "Explosion-Low");

    int32_t duration = Rand(700) + 300;
    for (int32_t z = 0; z < duration; z++) {
        int32_t x = Rand(WORLD_X - 1);
        int32_t y = Rand(WORLD_Y - 1);

        if (!TestBounds(x, y)) continue;

        if (Vulnerable(Map[x][y])) {
            if (z & 0x03) {
                Map[x][y] = (uint16_t)(RUBBLE + BULLBIT + (Rand16() & 3));
            } else {
                Map[x][y] = (uint16_t)(FIRE + ANIMBIT + (Rand16() & 7));
            }
        }
    }
}

void MakeMeltdown(void)
{
    for (int32_t x = 0; x < WORLD_X - 1; x++) {
        for (int32_t y = 0; y < WORLD_Y - 1; y++) {
            if ((Map[x][y] & LOMASK) == NUCLEAR) {
                DoMeltdown(x, y);
                Audio_MakeSound("city", "Explosion-High");
                return;
            }
        }
    }
}

void FireBomb(void)
{
    CrashX = (int16_t)Rand(WORLD_X - 1);
    CrashY = (int16_t)Rand(WORLD_Y - 1);
    MakeExplosion(CrashX, CrashY);
    ClearMes();
    SendMesAt(-30, CrashX, CrashY);
}

/* Unix schedules these four bombs over 0-900 ms; with no timers they drop in one step. */
static void DropFireBombs(void)
{
    for (int32_t i = 0; i < 4; i++) {
        FireBomb();
    }
}

static void ScenarioDisaster(void)
{
    switch (DisasterEvent) {
    case 2: /* San Francisco */
        if (DisasterWait == 1) MakeEarthquake();
        break;
    case 3: /* Hamburg */
        DropFireBombs();
        break;
    case 5: /* Tokyo */
        if (DisasterWait == 1) MakeMonster();
        break;
    case 7: /* Boston */
        if (DisasterWait == 1) MakeMeltdown();
        break;
    case 8: /* Rio */
        if ((DisasterWait % 24) == 0) MakeFlood();
        break;
    default:
        break;
    }

    if (DisasterWait) {
        DisasterWait--;
    } else {
        DisasterEvent = 0;
    }
}

void DoDisasters(void)
{
    static const int16_t DisChance[3] = { 10 * 48, 5 * 48, 60 };

    if (FloodCnt > 0) FloodCnt--;

    if (DisasterEvent) ScenarioDisaster();

    int16_t lvl = GameLevel;
    if (lvl > 2 || lvl < 0) lvl = 0;

    if (NoDisasters) return;

    if (!Rand(DisChance[lvl])) {
        int16_t choice = Rand(8);
        switch (choice) {
        case 0:
        case 1: SetFire(); break;
        case 2:
        case 3: MakeFlood(); break;
        case 5: MakeTornado(); break;
        case 6: MakeEarthquake(); break;
        case 7:
        case 8:
            if (PolluteAverage > 60) MakeMonster();
            break;
        default: break;
        }
    }
}