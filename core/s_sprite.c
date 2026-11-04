/* s_sprite.c: Dynamic entity simulation and step logic for vehicles and disasters.
 *
 * Modernized C11 implementation ported from Micropolis.
 */

#include "sim.h"

extern void Audio_MakeSound(const char *channel, const char *sound_name);
extern void ClearMes(void);
extern void SendMesAt(int16_t msg, int16_t x, int16_t y);

static SimSprite active_sprites[OBJN];
static SimSprite explosions[EXP_SLOTS];

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

/* Unix GetDir: 8-way heading (1=N, 2=NE, 4=SE, 5=S, 6=SW, 8=NW) toward the destination.
 * Sets the global absDist to the Manhattan distance, as Unix does; callers read it
 * even when it was computed on an earlier call. */
static int16_t GetDir(int32_t orgX, int32_t orgY, int32_t desX, int32_t desY)
{
    static const int16_t Gdtab[13] = { 0, 3, 2, 1, 3, 4, 5, 7, 6, 5, 7, 8, 1 };
    int32_t dispX = desX - orgX;
    int32_t dispY = desY - orgY;
    int32_t z;

    if (dispX < 0) {
        z = (dispY < 0) ? 11 : 8;
    } else {
        z = (dispY < 0) ? 2 : 5;
    }

    if (dispX < 0) dispX = -dispX;
    if (dispY < 0) dispY = -dispY;

    absDist = (int32_t)(dispX + dispY);

    if ((dispX << 1) < dispY) z++;

    return Gdtab[z];
}

void InitSprites(void)
{
    memset(active_sprites, 0, sizeof(active_sprites));
    memset(explosions, 0, sizeof(explosions));
    for (int i = 0; i < OBJN; i++) {
        active_sprites[i].type = i;
        GlobalSprites[i] = &active_sprites[i];
    }
}

static int16_t GetChar(int32_t px, int32_t py)
{
    int32_t tx = px >> 4;
    int32_t ty = py >> 4;
    if (!TestBounds(tx, ty)) return -1;
    return (int16_t)(Map[tx][ty] & LOMASK);
}

void GenerateTrain(int16_t x, int16_t y)
{
    SimSprite *s = &active_sprites[TRA];
    if (TotalPop <= 20 || s->frame != 0 || Rand(25) != 0) return;

    /* Unix TRA_GROOVE (-39, +6) converted to core logical space with the TRA shift (40, -8). */
    s->type = TRA;
    s->frame = 1;
    s->x = (x << 4) + 1;
    s->y = (y << 4) - 2;
    s->width = 32;
    s->height = 32;
    s->x_offset = -8;
    s->y_offset = -8;
    s->x_hot = 0;
    s->y_hot = 0;
    s->dir = 4;
    s->count = 0;
}

void GenerateCopter(int16_t x, int16_t y)
{
    SimSprite *s = &active_sprites[COP];
    if (s->frame != 0) return;

    /* Unix MakeSprite(COP, x<<4, (y<<4)+30) and InitSprite orig = (x-30, y), in Unix logical space.
     * Converted to core space with the COP shift (40, -8). */
    s->type = COP;
    s->frame = 5;
    s->x = (x << 4) + 40;
    s->y = (y << 4) + 22;
    s->width = 32;
    s->height = 32;
    s->x_offset = -8;
    s->y_offset = -8;
    s->x_hot = 0;
    s->y_hot = 0;
    s->orig_x = (x << 4) + 10;
    s->orig_y = s->y;
    s->dest_x = Rand((WORLD_X << 4) - 1) + 40;
    s->dest_y = Rand((WORLD_Y << 4) - 1) - 8;
    s->count = 1500;
    s->control = -1;
    s->sound_count = 0;
}

void GeneratePlane(int16_t x, int16_t y)
{
    SimSprite *s = &active_sprites[AIR];
    if (s->frame != 0) return;

    /* Unix InitSprite(AIR) in Unix logical space (shift to core is +40, +16). A plane
     * spawned near the east edge flies west from frame 7; otherwise it takes off east from frame 11. */
    int32_t ux = (x << 4) + 48;
    int32_t uy = (y << 4) + 12;
    int16_t frame;
    int32_t dest_ux;
    if (ux > ((WORLD_X - 20) << 4)) {
        ux -= 100 + 48;
        dest_ux = ux - 200;
        frame = 7;
    } else {
        dest_ux = ux + 200;
        frame = 11;
    }

    s->type = AIR;
    s->frame = frame;
    s->x = (int16_t)(ux + 40);
    s->y = (int16_t)(uy + 16);
    s->width = 48;
    s->height = 48;
    s->x_offset = -16;
    s->y_offset = -16;
    s->x_hot = 8;
    s->y_hot = 0;
    s->dest_x = (int16_t)(dest_ux + 40);
    s->dest_y = s->y;
    s->count = 0;
}

static void SpawnShip(int32_t tx, int32_t ty)
{
    /* Unix MakeShipHere: MakeSprite(SHI, (tx<<4)-47, ty<<4); InitSprite picks the heading from the edge.
     * Converted to core space with the SHI shift (48, 0). */
    int32_t ux = (tx << 4) - 47;
    int32_t uy = ty << 4;
    int16_t frame;
    if (ux < (4 << 4)) frame = 3;
    else if (ux >= ((WORLD_X - 4) << 4)) frame = 7;
    else if (uy < (4 << 4)) frame = 5;
    else if (uy >= ((WORLD_Y - 4) << 4)) frame = 1;
    else frame = 3;

    SimSprite *s = &active_sprites[SHI];
    s->type = SHI;
    s->frame = frame;
    s->new_dir = frame;
    s->dir = 10;
    s->x = ux + 48;
    s->y = uy;
    s->width = 48;
    s->height = 48;
    s->x_offset = -16;
    s->y_offset = -16;
    s->x_hot = 0;
    s->y_hot = 0;
    s->count = 1;
    s->sound_count = 0;
}

void GenerateShip(void)
{
    SimSprite *s = &active_sprites[SHI];
    if (s->frame != 0) return;

    /* Unix GenerateShip: each edge is tried with a 1-in-4 chance; the first channel tile found spawns a ship. */
    int32_t x, y;
    if (!(Rand16() & 3))
        for (x = 4; x < WORLD_X - 2; x++)
            if ((Map[x][0] & LOMASK) == CHANNEL) { SpawnShip(x, 0); return; }
    if (!(Rand16() & 3))
        for (y = 1; y < WORLD_Y - 2; y++)
            if ((Map[0][y] & LOMASK) == CHANNEL) { SpawnShip(0, y); return; }
    if (!(Rand16() & 3))
        for (x = 4; x < WORLD_X - 2; x++)
            if ((Map[x][WORLD_Y - 1] & LOMASK) == CHANNEL) { SpawnShip(x, WORLD_Y - 1); return; }
    if (!(Rand16() & 3))
        for (y = 1; y < WORLD_Y - 2; y++)
            if ((Map[WORLD_X - 1][y] & LOMASK) == CHANNEL) { SpawnShip(WORLD_X - 1, y); return; }
}

static SimSprite *FindFreeExplosion(void)
{
    for (int i = 0; i < EXP_SLOTS; i++) {
        if (explosions[i].frame == 0) return &explosions[i];
    }
    return NULL;
}

/* Takes an absolute map point, matching Unix MakeExplosionAt. */
void MakeExplosionAt(int32_t x, int32_t y)
{
    SimSprite *s = FindFreeExplosion();
    if (s == NULL) return;

    memset(s, 0, sizeof(*s));
    s->type = EXP;
    s->frame = 1;
    s->x = x - 40;
    s->y = y - 16;
    s->width = 48;
    s->height = 48;
    s->x_offset = 24;
    s->y_offset = 0;
    s->x_hot = 40;
    s->y_hot = 16;
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
    s->x_hot = 0;
    s->y_hot = 0;
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
        s->count = 200;
        return;
    }

    int32_t ux = Rand((WORLD_X << 4) - 800) + 400;
    int32_t uy = Rand((WORLD_Y << 4) - 200) + 100;

    s->type = TOR;
    s->frame = 1;
    s->step = 0; /* animation direction flag, Unix sprite->flag */
    s->width = 48;
    s->height = 48;
    s->x_offset = -16;
    s->y_offset = -16;
    s->x_hot = 0;
    s->y_hot = 20;
    s->x = ux + 40;
    s->y = uy + 16;
    s->count = 200;

    ClearMes();
    SendMesAt(-22, (int16_t)((ux >> 4) + 3), (int16_t)((uy >> 4) + 2));
    Audio_MakeSound("city", "siren");
}

static void StepTrain(SimSprite *s)
{
    static const int16_t Cx[4] = { 0, 16, 0, -16 };
    static const int16_t Cy[4] = { -16, 0, 16, 0 };
    static const int16_t Dx[5] = { 0, 4, 0, -4, 0 };
    static const int16_t Dy[5] = { -4, 0, 4, 0, 0 };
    static const int16_t TrainPic2[5] = { 1, 2, 1, 2, 5 };

    if (s->frame == 3 || s->frame == 4) s->frame = TrainPic2[s->dir];
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
                if (s->dir != dir2 && s->dir != 4) {
                    s->frame = (s->dir + dir2 == 3) ? 3 : 4;
                } else {
                    s->frame = TrainPic2[dir2];
                }

                if (c == RAILBASE || c == RAILBASE + 1) s->frame = 5;
                s->dir = dir2;
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

static bool SpriteNotInBounds(const SimSprite *s)
{
    int32_t x = s->x + s->x_hot;
    int32_t y = s->y + s->y_hot;
    return (x < 0) || (y < 0) || (x >= (WORLD_X << 4)) || (y >= (WORLD_Y << 4));
}

static bool CheckSpriteCollision(const SimSprite *a, const SimSprite *b)
{
    if (a->frame == 0 || b->frame == 0) return false;
    int32_t dx = (a->x + a->x_hot) - (b->x + b->x_hot);
    int32_t dy = (a->y + a->y_hot) - (b->y + b->y_hot);
    return (abs(dx) + abs(dy)) < 30;
}

static void ExplodeSprite(SimSprite *s)
{
    s->frame = 0;

    int32_t px = s->x + s->x_hot;
    int32_t py = s->y + s->y_hot;
    MakeExplosionAt(px, py);

    int16_t tx = (int16_t)(px >> 4);
    int16_t ty = (int16_t)(py >> 4);
    int16_t msg = 0;
    switch (s->type) {
    case AIR: msg = -24; break;
    case SHI: msg = -25; break;
    case TRA: msg = -26; break;
    case COP: msg = -27; break;
    default: break;
    }
    if (msg != 0) {
        CrashX = tx;
        CrashY = ty;
        SendMesAt(msg, tx, ty);
    }
    Audio_MakeSound("city", "Explosion-High");
}

static void StartFire(int32_t px, int32_t py)
{
    int32_t tx = px >> 4;
    int32_t ty = py >> 4;
    if (!TestBounds(tx, ty)) return;

    uint16_t z = Map[tx][ty];
    uint16_t t = z & LOMASK;
    if (!(z & BURNBIT) && t != 0) return;
    if (z & ZONEBIT) return;
    Map[tx][ty] = (uint16_t)(FIRE + (Rand16() & 3) + ANIMBIT);
}

static bool IsWetTile(int32_t t)
{
    return (t == POWERBASE) || (t == POWERBASE + 1) ||
           (t == RAILBASE) || (t == RAILBASE + 1) ||
           (t == BRWH) || (t == BRWV);
}

/* Tornado damage at an absolute map point, matching Unix Destroy. */
static void DestroyAt(int32_t px, int32_t py)
{
    int32_t tx = px >> 4;
    int32_t ty = py >> 4;
    if (!TestBounds(tx, ty)) return;

    uint16_t z = Map[tx][ty];
    int32_t t = z & LOMASK;
    if (t < TREEBASE) return;

    if (!(z & BURNBIT)) return;

    if (z & ZONEBIT) {
        FireZone(tx, ty, t);
        if (t > RZB) MakeExplosionAt(px, py);
    }

    if (IsWetTile(t)) {
        Map[tx][ty] = RIVER;
    } else {
        Map[tx][ty] = (uint16_t)((DoAnimation ? TINYEXP : (LASTTINYEXP - 3)) | BULLBIT | ANIMBIT);
    }
}

static void StepCopter(SimSprite *s)
{
    static const int16_t CDx[9] = { 0, 0, 3, 5, 3, 0, -3, -5, -3 };
    static const int16_t CDy[9] = { 0, -5, -3, 0, 3, 5, 3, 0, -3 };

    if (s->sound_count > 0) s->sound_count--;

    if (s->count > 0) s->count--;

    if (s->count == 0) {
        /* Attract toward the monster, then the tornado. The (0,-24) offset converts
         * the target's core position into the copter's coordinate space. */
        SimSprite *target = GetSprite(GOD);
        if (target == NULL || target->frame == 0) target = GetSprite(TOR);
        if (target != NULL && target->frame != 0) {
            s->dest_x = target->x;
            s->dest_y = target->y - 24;
        } else {
            s->dest_x = s->orig_x;
            s->dest_y = s->orig_y;
        }

        GetDir(s->x, s->y, s->orig_x, s->orig_y);
        if (absDist < 30) {
            s->frame = 0;
            return;
        }
    }

    if (s->sound_count == 0) {
        int32_t tx = (s->x + 8) >> 5;
        int32_t ty = (s->y + 8) >> 5;
        if (tx >= 0 && tx < HWLDX && ty >= 0 && ty < HWLDY) {
            if (TrfDensity[tx][ty] > 170 && (Rand16() & 7) == 0) {
                SendMesAt(-41, (int16_t)((tx << 1) + 1), (int16_t)((ty << 1) + 1));
                Audio_MakeSound("city", "HeavyTraffic");
                s->sound_count = 200;
            }
        }
    }

    if (!(Cycle & 3)) {
        int16_t target_dir = GetDir(s->x, s->y, s->dest_x, s->dest_y);
        s->frame = TurnTo(s->frame, target_dir);
    }

    s->x += CDx[s->frame];
    s->y += CDy[s->frame];

    if (s->count > 0 && abs(s->dest_x - s->x) < 24 && abs(s->dest_y - s->y) < 24) {
        s->dest_x = (Rand(WORLD_X) << 4);
        s->dest_y = (Rand(WORLD_Y) << 4);
    }
}

static void StepPlane(SimSprite *s)
{
    static const int16_t CDx[12] = { 0, 0, 6, 8, 6, 0, -6, -8, -6, 8, 8, 8 };
    static const int16_t CDy[12] = { 0, -8, -6, 0, 6, 8, 6, 0, -6, 0, 0, 0 };

    if (!(Cycle % 5)) {
        if (s->frame > 8) {
            /* Takeoff: frames 9-11 fly straight east, then settle into heading 3 */
            s->frame--;
            if (s->frame < 9) s->frame = 3;
        } else {
            int16_t target_dir = GetDir(s->x, s->y, s->dest_x, s->dest_y);
            s->frame = TurnTo(s->frame, target_dir);
        }
    }

    /* Unix tests the absDist left by the last GetDir call, so this can use a stale value */
    if (absDist < 50) {
        s->dest_x = (Rand(WORLD_X * 16 + 100) - 50) + 40;
        s->dest_y = (Rand(WORLD_Y * 16 + 100) - 50) + 16;
    }

    if (!NoDisasters && CheckSpriteCollision(s, &active_sprites[COP])) {
        ExplodeSprite(&active_sprites[COP]);
        ExplodeSprite(s);
        return;
    }

    s->x += CDx[s->frame];
    s->y += CDy[s->frame];

    if (SpriteNotInBounds(s)) s->frame = 0;
}

static bool ShipTryOther(int16_t tile, int32_t told, int32_t tnew)
{
    int32_t z = told + 4;
    if (z > 8) z -= 8;
    if (tnew != z) return false;
    return tile == POWERBASE || tile == POWERBASE + 1 || tile == RAILBASE || tile == RAILBASE + 1;
}

static void StepShip(SimSprite *s)
{
    /* Unix DoShipSprite tables; headings 1..8 are N, NE, E, SE, S, SW, W, NW */
    static const int16_t BDx[9] = { 0, 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int16_t BDy[9] = { 0, -1, -1, 0, 1, 1, 1, 0, -1 };
    static const int16_t BPx[9] = { 0, 0, 2, 2, 2, 0, -2, -2, -2 };
    static const int16_t BPy[9] = { 0, -2, -2, 0, 2, 2, 2, 0, -2 };
    static const int16_t BtClrTab[8] = { RIVER, CHANNEL, POWERBASE, POWERBASE + 1,
                                         RAILBASE, RAILBASE + 1, BRWH, BRWV };
    int16_t t = RIVER;

    if (s->sound_count > 0) s->sound_count--;
    if (s->sound_count == 0) {
        if ((Rand16() & 3) == 1) Audio_MakeSound("city", "HonkHonk-Low");
        s->sound_count = 200;
    }

    if (s->count > 0) s->count--;
    if (s->count == 0) {
        s->count = 9;
        if (s->frame != s->new_dir) {
            s->frame = TurnTo((int16_t)s->frame, (int16_t)s->new_dir);
            return;
        }

        int32_t tem = Rand16() & 7;
        int32_t pem;
        for (pem = tem; pem < tem + 8; pem++) {
            int32_t z = (pem & 7) + 1;
            if (z == s->dir) continue;

            /* Probe tile from the Unix logical position: core x is Unix x + 48, so (Unix x + 47) >> 4 is (core x - 1) >> 4 */
            int32_t tx = ((s->x - 1) >> 4) + BDx[z];
            int32_t ty = (s->y >> 4) + BDy[z];
            if (TestBounds(tx, ty)) {
                t = (int16_t)(Map[tx][ty] & LOMASK);
                if (t == CHANNEL || t == BRWH || t == BRWV || ShipTryOther(t, s->dir, z)) {
                    s->new_dir = z;
                    s->frame = TurnTo((int16_t)s->frame, (int16_t)s->new_dir);
                    s->dir = z + 4;
                    if (s->dir > 8) s->dir -= 8;
                    break;
                }
            }
        }
        if (pem == tem + 8) {
            s->dir = 10;
            s->new_dir = (Rand16() & 7) + 1;
        }
    } else if (s->frame == s->new_dir) {
        s->x += BPx[s->frame];
        s->y += BPy[s->frame];
    }

    if (SpriteNotInBounds(s)) {
        s->frame = 0;
        return;
    }

    /* Unix explodes the ship and calls Destroy when the last probed tile is not waterway or bridge */
    bool on_water = false;
    for (int32_t z = 0; z < 8; z++) {
        if (t == BtClrTab[z]) { on_water = true; break; }
    }
    if (!on_water) {
        ExplodeSprite(s);
        DestroyAt(s->x, s->y);
    }
}

static void StepExplosion(SimSprite *s)
{
    if (!(Cycle & 1)) {
        if (s->frame == 1) {
            Audio_MakeSound("city", "Explosion-High");
            int16_t mx = (int16_t)((s->x >> 4) + 3);
            int16_t my = (int16_t)(s->y >> 4);
            SendMesAt(32, mx, my);
        }
        s->frame++;
    }

    if (s->frame > 6) {
        s->frame = 0;
        StartFire(s->x + 40, s->y + 16);
        StartFire(s->x + 24, s->y);
        StartFire(s->x + 56, s->y);
        StartFire(s->x + 24, s->y + 32);
        StartFire(s->x + 56, s->y + 32);
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

    /* Unix Destroy(x+48, y+16) in Unix logical space; core monster shift is (40, 16) */
    DestroyAt(s->x + 8, s->y);

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

    int16_t z = s->frame;
    if (z == 2) {
        z = s->step ? 3 : 1;
    } else {
        s->step = (z == 1) ? 1 : 0;
        z = 2;
    }

    if (s->count > 0) s->count--;
    s->frame = z;

    for (int32_t i = 0; i < OBJN; i++) {
        SimSprite *other = &active_sprites[i];
        if (other->frame == 0) continue;
        if (other->type != AIR && other->type != COP && other->type != SHI && other->type != TRA) continue;
        if (CheckSpriteCollision(s, other)) ExplodeSprite(other);
    }

    z = Rand(5);
    s->x += CDx[z];
    s->y += CDy[z];
    if (SpriteNotInBounds(s)) s->frame = 0;

    if (s->count != 0 && !Rand(500)) s->frame = 0;

    DestroyAt(s->x + 8, s->y + 24);
}

void MoveObjects(void)
{
    if (SimSpeed == 0) return;
    Cycle++;

    if (active_sprites[TRA].frame) StepTrain(&active_sprites[TRA]);
    if (active_sprites[COP].frame) StepCopter(&active_sprites[COP]);
    if (active_sprites[AIR].frame) StepPlane(&active_sprites[AIR]);
    if (active_sprites[SHI].frame) StepShip(&active_sprites[SHI]);
    for (int32_t i = 0; i < EXP_SLOTS; i++) {
        if (explosions[i].frame) StepExplosion(&explosions[i]);
    }
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

SimSprite *GetSpriteSlot(int32_t type, int32_t index)
{
    if (type == EXP) {
        if (index >= 0 && index < EXP_SLOTS) return &explosions[index];
        return NULL;
    }
    if (index == 0) return GetSprite(type);
    return NULL;
}