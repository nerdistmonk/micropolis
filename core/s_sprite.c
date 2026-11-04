/* s_sprite.c: Dynamic entity simulation and step logic for vehicles and disasters.
 *
 * Modernized C11 implementation ported from Micropolis.
 */

#include "sim.h"

extern void Audio_MakeSound(const char *channel, const char *sound_name);
extern void ClearMes(void);
extern void SendMesAt(int16_t msg, int16_t x, int16_t y);

static SimSprite active_sprites[OBJN];

/* Turn sprite smoothly towards target direction (1..8) */
static int16_t TurnTo(int16_t current, int16_t target)
{
    if (current == target) return current;
    if (current < target) {
        if ((target - current) < 4) current++;
        else current--;
    } else {
        if ((current - target) < 4) current--;
        else current++;
    }
    if (current > 8) current = 1;
    if (current < 1) current = 8;
    return current;
}

/* Calculate 8-way direction from source to destination (1=N, 2=NE, 3=E... 8=NW) */
static int16_t GetDir(int32_t orgX, int32_t orgY, int32_t desX, int32_t desY)
{
    int32_t dx = desX - orgX;
    int32_t dy = desY - orgY;
    int32_t adx = abs(dx);
    int32_t ady = abs(dy);

    if (adx > (ady * 2)) {
        return (dx > 0) ? 3 : 7; /* East : West */
    }
    if (ady > (adx * 2)) {
        return (dy > 0) ? 5 : 1; /* South : North */
    }
    if (dx > 0) {
        return (dy > 0) ? 4 : 2; /* SE : NE */
    }
    return (dy > 0) ? 6 : 8;     /* SW : NW */
}

void InitSprites(void)
{
    for (int i = 0; i < OBJN; i++) {
        active_sprites[i].frame = 0;
        active_sprites[i].count = 0;
        active_sprites[i].dir = 0;
    }
}

static int16_t GetChar(int32_t px, int32_t py)
{
    int32_t tx = px >> 4;
    int32_t ty = py >> 4;
    if (!TestBounds(tx, ty)) return -1;
    return (int16_t)(Map[tx][ty] & LOMASK);
}

static void DestroyTile(int32_t tx, int32_t ty)
{
    if (!TestBounds(tx, ty)) return;

    uint16_t tile = Map[tx][ty];
    uint16_t low = tile & LOMASK;

    /* Leave water untouched */
    if (low == RIVER || low == REDGE || low == CHANNEL ||
        (low >= FIRSTRIVEDGE && low <= LASTRIVEDGE)) {
        return;
    }

    /* Water crossings: revert to pure water */
    if (low == HBRIDGE || low == VBRIDGE || low == BRWH || low == BRWV ||
        low == HPOWER || low == VPOWER || low == HRAIL || low == VRAIL) {
        Map[tx][ty] = RIVER;
        return;
    }

    if (low > DIRT) {
        Map[tx][ty] = (uint16_t)((RUBBLE + (Rand16() & 3)) | BULLBIT);
    }
}

void GenerateTrain(int16_t x, int16_t y)
{
    SimSprite *s = &active_sprites[TRA];
    if (s->frame != 0) return;

    s->type = TRA;
    s->frame = 1;
    s->x = (x << 4);
    s->y = (y << 4);
    s->width = 32;
    s->height = 32;
    s->x_offset = -8;
    s->y_offset = -8;
    s->dir = 4;
    s->count = 0;
}

void GenerateCopter(int16_t x, int16_t y)
{
    SimSprite *s = &active_sprites[COP];
    if (s->frame != 0) return;

    s->type = COP;
    s->frame = 1;
    s->x = (x << 4);
    s->y = (y << 4);
    s->width = 32;
    s->height = 32;
    s->x_offset = -8;
    s->y_offset = -8;
    s->dest_x = (Rand(WORLD_X) << 4);
    s->dest_y = (Rand(WORLD_Y) << 4);
    s->count = 1500;
}

void GeneratePlane(int16_t x, int16_t y)
{
    SimSprite *s = &active_sprites[AIR];
    if (s->frame != 0) return;

    s->type = AIR;
    s->frame = 1;
    s->x = (x << 4);
    s->y = (y << 4);
    s->width = 48;
    s->height = 48;
    s->x_offset = -16;
    s->y_offset = -16;
    s->dest_x = (Rand(WORLD_X) << 4);
    s->dest_y = (Rand(WORLD_Y) << 4);
    s->count = 1000;
}

static void SpawnShip(int32_t x, int32_t y, int16_t frame, int16_t dir)
{
    SimSprite *s = &active_sprites[SHI];
    s->type = SHI;
    s->frame = frame;
    s->dir = dir;
    s->x = (x << 4);
    s->y = (y << 4);
    s->width = 48;
    s->height = 48;
    s->x_offset = -16;
    s->y_offset = -16;
    s->count = 300;
}

void GenerateShip(void)
{
    SimSprite *s = &active_sprites[SHI];
    if (s->frame != 0) return;

    /* 1. North Edge (headed South) */
    for (int32_t x = 4; x < (WORLD_X - 4); x++) {
        if ((Map[x][0] & LOMASK) == CHANNEL || (Map[x][1] & LOMASK) == CHANNEL) {
            SpawnShip(x, 1, 5, 2);
            return;
        }
    }

    /* 2. West Edge (Golden Gate / San Francisco headed East) */
    for (int32_t y = 4; y < (WORLD_Y - 4); y++) {
        if ((Map[0][y] & LOMASK) == CHANNEL || (Map[1][y] & LOMASK) == CHANNEL) {
            SpawnShip(1, y, 3, 1);
            return;
        }
    }

    /* 3. South Edge (headed North) */
    for (int32_t x = 4; x < (WORLD_X - 4); x++) {
        if ((Map[x][WORLD_Y - 1] & LOMASK) == CHANNEL || (Map[x][WORLD_Y - 2] & LOMASK) == CHANNEL) {
            SpawnShip(x, WORLD_Y - 2, 1, 0);
            return;
        }
    }

    /* 4. East Edge (headed West) */
    for (int32_t y = 4; y < (WORLD_Y - 4); y++) {
        if ((Map[WORLD_X - 1][y] & LOMASK) == CHANNEL || (Map[WORLD_X - 2][y] & LOMASK) == CHANNEL) {
            SpawnShip(WORLD_X - 2, y, 7, 3);
            return;
        }
    }

    /* Fallback: any open channel in the world */
    for (int32_t y = 10; y < (WORLD_Y - 10); y += 4) {
        for (int32_t x = 10; x < (WORLD_X - 10); x += 4) {
            if ((Map[x][y] & LOMASK) == CHANNEL) {
                SpawnShip(x, y, 3, 1);
                return;
            }
        }
    }
}

void MakeExplosionAt(int32_t x, int32_t y)
{
    SimSprite *s = &active_sprites[EXP];
    s->type = EXP;
    s->frame = 1;
    s->x = x;
    s->y = y;
    s->width = 48;
    s->height = 48;
    s->x_offset = -16;
    s->y_offset = -16;
    s->count = 0;
}

void MakeMonster(void)
{
    SimSprite *s = &active_sprites[GOD];
    if (s->frame != 0) {
        s->count = 1000;
        return;
    }

    s->type = GOD;
    s->frame = 1;
    s->step = 0;
    s->dir = 0;
    s->width = 48;
    s->height = 48;
    s->x_offset = -16;
    s->y_offset = -16;
    s->count = 1200;

    bool found = false;
    for (int32_t z = 0; z < 300; z++) {
        int32_t x = Rand(WORLD_X - 20) + 10;
        int32_t y = Rand(WORLD_Y - 20) + 10;
        if ((Map[x][y] & LOMASK) == RIVER) {
            s->x = (x << 4);
            s->y = (y << 4);
            found = true;
            break;
        }
    }
    if (!found) {
        s->x = (HWLDX << 4);
        s->y = (HWLDY << 4);
    }

    s->dest_x = (Rand(WORLD_X) << 4);
    s->dest_y = (Rand(WORLD_Y) << 4);

    ClearMes();
    SendMesAt(-21, s->x >> 4, s->y >> 4);
    Audio_MakeSound("city", "monster");
}

void MakeTornado(void)
{
    SimSprite *s = &active_sprites[TOR];
    if (s->frame != 0) {
        s->count = 400;
        return;
    }

    s->type = TOR;
    s->frame = 1;
    s->step = 0;
    s->width = 48;
    s->height = 48;
    s->x_offset = -16;
    s->y_offset = -16;
    s->x = (Rand(WORLD_X - 30) + 15) << 4;
    s->y = (Rand(WORLD_Y - 30) + 15) << 4;
    s->count = 800;

    ClearMes();
    SendMesAt(-22, s->x >> 4, s->y >> 4);
    Audio_MakeSound("city", "siren");
}

static void StepTrain(SimSprite *s)
{
    static const int16_t Cx[4] = { 0, 16, 0, -16 };
    static const int16_t Cy[4] = { -16, 0, 16, 0 };
    static const int16_t Dx[5] = { 0, 4, 0, -4, 0 };
    static const int16_t Dy[5] = { -4, 0, 4, 0, 0 };
    static const int16_t TrainPic[5] = { 1, 2, 1, 2, 1 };

    s->x += Dx[s->dir];
    s->y += Dy[s->dir];

    if (!(Cycle & 3)) {
        int16_t dir = Rand16() & 3;
        for (int16_t z = dir; z < (dir + 4); z++) {
            int16_t dir2 = z & 3;

            if (s->dir != 4) {
                if (dir2 == ((s->dir + 2) & 3)) continue;
            }

            int16_t c = GetChar(s->x + Cx[dir2] + 8, s->y + Cy[dir2] + 8);
            if (((c >= RAILBASE) && (c <= LASTRAIL)) ||
                (c == RAILVPOWERH) || (c == RAILHPOWERV)) {
                s->dir = dir2;
                s->frame = TrainPic[dir2];
                return;
            }
        }

        if (s->dir == 4) {
            s->frame = 0;
            return;
        }

        s->dir = 4;
    }
}

static void StepCopter(SimSprite *s)
{
    static const int16_t CDx[9] = { 0, 0, 2, 3, 2, 0, -2, -3, -2 };
    static const int16_t CDy[9] = { 0, -3, -2, 0, 2, 3, 2, 0, -2 };

    if (!(Cycle & 3)) {
        int16_t target_dir = GetDir(s->x, s->y, s->dest_x, s->dest_y);
        s->frame = TurnTo(s->frame, target_dir);
    }

    s->x += CDx[s->frame];
    s->y += CDy[s->frame];

    if (abs(s->dest_x - s->x) < 24 && abs(s->dest_y - s->y) < 24) {
        s->dest_x = (Rand(WORLD_X) << 4);
        s->dest_y = (Rand(WORLD_Y) << 4);
    }
    if (--s->count <= 0) s->frame = 0;
}

static void StepPlane(SimSprite *s)
{
    static const int16_t CDx[9] = { 0, 0, 3, 4, 3, 0, -3, -4, -3 };
    static const int16_t CDy[9] = { 0, -4, -3, 0, 3, 4, 3, 0, -3 };

    if (!(Cycle & 3)) {
        int16_t target_dir = GetDir(s->x, s->y, s->dest_x, s->dest_y);
        s->frame = TurnTo(s->frame, target_dir);
    }

    s->x += CDx[s->frame];
    s->y += CDy[s->frame];

    if (abs(s->dest_x - s->x) < 32 && abs(s->dest_y - s->y) < 32) {
        s->dest_x = (Rand(WORLD_X) << 4);
        s->dest_y = (Rand(WORLD_Y) << 4);
    }
    if (--s->count <= 0) s->frame = 0;
}

static void StepShip(SimSprite *s)
{
    static const int16_t Dx[4] = { 0, 2, 0, -2 };
    static const int16_t Dy[4] = { -2, 0, 2, 0 };
    static const int16_t ShipFrames[4] = { 1, 3, 5, 7 }; /* N, E, S, W */

    s->x += Dx[s->dir & 3];
    s->y += Dy[s->dir & 3];
    s->frame = ShipFrames[s->dir & 3];

    /* Probe 1 tile ahead for navigable waterway */
    int16_t ahead = GetChar(s->x + (Dx[s->dir & 3] * 16) + 8, s->y + (Dy[s->dir & 3] * 16) + 8);
    if (ahead != CHANNEL && ahead != RIVER && ahead != HBRIDGE && ahead != VBRIDGE &&
        ahead != BRWH && ahead != BRWV) {
        int16_t d1 = (s->dir + 1) & 3;
        int16_t d2 = (s->dir + 3) & 3;
        int16_t c1 = GetChar(s->x + (Dx[d1] * 16) + 8, s->y + (Dy[d1] * 16) + 8);
        if (c1 == CHANNEL || c1 == RIVER || c1 == HBRIDGE || c1 == VBRIDGE || c1 == BRWH || c1 == BRWV) {
            s->dir = d1;
        } else {
            s->dir = d2;
        }
    }

    /* Keep within bounds */
    if (s->x < 0 || s->x >= (WORLD_X << 4) || s->y < 0 || s->y >= (WORLD_Y << 4)) {
        s->frame = 0;
    }
}

static void StepExplosion(SimSprite *s)
{
    if (++s->count > 3) {
        s->count = 0;
        if (++s->frame > 6) {
            s->frame = 0;
        }
    }
}

static void StepMonster(SimSprite *s)
{
    /* Frames 1-12 are four facings of three walk frames (dir * 3 + walk + 1); 13-16 are turn frames, unused here. */
    static const int32_t MDx[4] = { 1, 1, -1, -1 };
    static const int32_t MDy[4] = { -1, 1, 1, -1 };

    int32_t z = (s->frame - 1) % 3;
    if (Cycle % 5 == 0) {
        if (z == 2) s->step = 0;
        if (z == 0) s->step = 1;
        z = s->step ? z + 1 : z - 1;
    }

    if (Cycle & 1) {
        int32_t dx = (s->dest_x > s->x) ? 1 : ((s->dest_x < s->x) ? -1 : 0);
        int32_t dy = (s->dest_y > s->y) ? 1 : ((s->dest_y < s->y) ? -1 : 0);
        s->x += dx;
        s->y += dy;
        for (int32_t d = 0; d < 4; d++) {
            if (dx != 0 && dy != 0 && MDx[d] == dx && MDy[d] == dy) s->dir = d;
        }
    }

    s->frame = 1 + s->dir * 3 + z;

    DestroyTile((s->x + 24) >> 4, (s->y + 24) >> 4);

    if (abs(s->dest_x - s->x) < 16 && abs(s->dest_y - s->y) < 16) {
        s->dest_x = (Rand(WORLD_X) << 4);
        s->dest_y = (Rand(WORLD_Y) << 4);
    }
    if (--s->count <= 0) s->frame = 0;
}

static void StepTornado(SimSprite *s)
{
    static const int16_t CDx[6] = { 2, 3, 2, 0, -2, -3 };
    static const int16_t CDy[6] = { -2, 0, 2, 3, 2, 0 };

    int16_t z = Rand(5);
    s->x += CDx[z];
    s->y += CDy[z];
    s->frame = 1 + ((s->frame) % 3);

    DestroyTile((s->x + 24) >> 4, (s->y + 24) >> 4);

    if (--s->count <= 0) s->frame = 0;
}

void MoveObjects(void)
{
    if (SimSpeed == 0) return;
    Cycle++;

    /* Periodic background ship traffic check */
    if ((Cycle & 127) == 0 && active_sprites[SHI].frame == 0) {
        GenerateShip();
    }

    if (active_sprites[TRA].frame) StepTrain(&active_sprites[TRA]);
    if (active_sprites[COP].frame) StepCopter(&active_sprites[COP]);
    if (active_sprites[AIR].frame) StepPlane(&active_sprites[AIR]);
    if (active_sprites[SHI].frame) StepShip(&active_sprites[SHI]);
    if (active_sprites[EXP].frame) StepExplosion(&active_sprites[EXP]);
    if (active_sprites[GOD].frame) StepMonster(&active_sprites[GOD]);
    if (active_sprites[TOR].frame) StepTornado(&active_sprites[TOR]);
}

SimSprite *GetSprite(int32_t type)
{
    if (type >= 0 && type < OBJN) {
        return &active_sprites[type];
    }
    return NULL;
}