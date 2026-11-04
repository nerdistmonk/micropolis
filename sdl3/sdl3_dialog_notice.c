/* sdl3_dialog_notice.c: Non-modal picture notices for disasters and city milestones.
 */

#include "sdl3_dialog_common.h"

#define NOTICE_W 440.0f
#define NOTICE_X ((WINDOW_DEFAULT_WIDTH - NOTICE_W) / 2.0f)
#define NOTICE_Y 70.0f
#define NOTICE_PAD 14.0f
#define NOTICE_LINE_H 18.0f
#define NOTICE_BODY_Y 44.0f
#define NOTICE_BTN_W 110.0f
#define NOTICE_BTN_H 28.0f
#define NOTICE_MAX_LINES 24
#define NOTICE_LINE_LEN 160
#define NOTICE_AUTO_CLEAR_MS 15000

typedef struct {
    int16_t id;
    SDL_Color color;
    const char *title;
    const char *body;
} NoticeEntry;

/* Historic Micropolis picture messages (res/micropolis.tcl). Only the ids the core sends are listed. */
static const NoticeEntry notice_table[] = {
    { 10, { 255, 79, 79, 255 }, "POLLUTION ALERT!",
      "Pollution in your city has exceeded the maximum allowable amounts established by the Micropolis Pollution Agency.  You are running the risk of grave ecological consequences.\n\nEither clean up your act or open a gas mask concession at city hall." },
    { 11, { 255, 79, 79, 255 }, "CRIME ALERT!",
      "Crime in your city is out of hand.  Angry mobs are looting and vandalizing the central city.  The president will send in the national guard soon if you cannot control the problem." },
    { 12, { 255, 79, 79, 255 }, "TRAFFIC WARNING!",
      "Traffic in this city is horrible.  The city gridlock is expanding.  The commuters are getting militant.\n\nEither build more roads and rails or get a bulletproof limo." },
    { 20, { 255, 79, 79, 255 }, "FIRE REPORTED!",
      "A fire has been reported!" },
    { 21, { 255, 79, 79, 255 }, "MONSTER ATTACK!",
      "A large reptilian creature has been spotted in the water.  It seems to be attracted to areas of high pollution.  There is a trail of destruction wherever it goes.  All you can do is wait till he leaves, then rebuild from the rubble." },
    { 22, { 255, 79, 79, 255 }, "TORNADO ALERT!",
      "A tornado has been reported!  There's nothing you can do to stop it, so you'd better prepare to clean up after the disaster!" },
    { 23, { 255, 79, 79, 255 }, "EARTHQUAKE!",
      "A major earthquake has occurred!  Put out the fires as quickly as possible, before they spread, then reconnect the power grid and rebuild the city." },
    { 30, { 255, 79, 79, 255 }, "FIREBOMBING REPORTED!",
      "Firebombs are falling!!" },
    { 35, { 127, 255, 127, 255 }, "TOWN",
      "Congratulations, your village has grown to town status.  You now have 2,000 citizens." },
    { 36, { 127, 255, 127, 255 }, "CITY",
      "Your town has grown into a full sized city, with a current population of 10,000.  Keep up the good work!" },
    { 37, { 127, 255, 127, 255 }, "CAPITAL",
      "Your city has become a capital.  The current population here is 50,000.  Your political future looks bright." },
    { 38, { 127, 255, 127, 255 }, "METROPOLIS",
      "Your capital city has now achieved the status of metropolis.  The current population is 100,000.  With your management skills, you should seriously consider running for governor." },
    { 39, { 127, 255, 127, 255 }, "MEGALOPOLIS",
      "Congratulations, you have reached the highest category of urban development, the megalopolis.\n\nIf you manage to reach this level, send us email at micropolis@laptop.org or send us a copy of your city.  We might do something interesting with it." },
    { 42, { 255, 79, 79, 255 }, "FLOODING REPORTED!",
      "Flooding has been reported along the water's edge!" },
    { 43, { 255, 79, 79, 255 }, "NUCLEAR MELTDOWN!",
      "A nuclear meltdown has occurred at your power plant.  You are advised to avoid the area until the radioactive isotopes decay.\n\nMany generations will confront this problem before it goes away, so don't hold your breath." },
};

/* One notice at a time; a new picture replaces the one on screen. */
static bool notice_active = false;
static const NoticeEntry *notice_entry = NULL;
static bool notice_has_target = false;
static int32_t notice_tile_x = 0;
static int32_t notice_tile_y = 0;
static char notice_lines[NOTICE_MAX_LINES][NOTICE_LINE_LEN];
static int notice_line_count = 0;
static uint64_t notice_shown_at = 0;

static const NoticeEntry *FindNotice(int16_t id)
{
    for (size_t i = 0; i < sizeof(notice_table) / sizeof(notice_table[0]); i++) {
        if (notice_table[i].id == id) return &notice_table[i];
    }
    return NULL;
}

static void AppendNoticeLine(const char *text)
{
    if (notice_line_count >= NOTICE_MAX_LINES) return;
    snprintf(notice_lines[notice_line_count], NOTICE_LINE_LEN, "%s", text);
    notice_line_count++;
}

/* Word-wraps body into notice_lines. '\n' forces a line break; a blank line is kept for '\n\n'. */
static void WrapNoticeBody(const char *body, int max_px)
{
    char cur[2 * NOTICE_LINE_LEN + 2] = { 0 };
    char trial[2 * NOTICE_LINE_LEN + 2];
    char word[NOTICE_LINE_LEN];
    const char *p = body;

    notice_line_count = 0;
    while (*p) {
        if (*p == '\n') {
            AppendNoticeLine(cur);
            cur[0] = '\0';
            p++;
            continue;
        }
        if (*p == ' ') {
            p++;
            continue;
        }

        const char *start = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        size_t len = (size_t)(p - start);
        if (len >= sizeof(word)) len = sizeof(word) - 1;
        memcpy(word, start, len);
        word[len] = '\0';

        size_t cl = strlen(cur);
        size_t wl = strlen(word);
        bool fits = false;
        if (cl + 1 + wl < sizeof(trial)) {
            memcpy(trial, cur, cl);
            trial[cl] = ' ';
            memcpy(trial + cl + 1, word, wl + 1);
            fits = GUI_GetTextWidth(trial) <= max_px;
        }

        if (cur[0] == '\0') {
            memcpy(cur, word, wl + 1);
        } else if (fits) {
            memcpy(cur, trial, cl + 1 + wl + 1);
        } else {
            AppendNoticeLine(cur);
            memcpy(cur, word, wl + 1);
        }
    }
    if (cur[0] != '\0') AppendNoticeLine(cur);
}

static float NoticeButtonY(void)
{
    return NOTICE_Y + NOTICE_BODY_Y + (float)notice_line_count * NOTICE_LINE_H + 10.0f;
}

static float NoticeHeight(void)
{
    return NoticeButtonY() + NOTICE_BTN_H + NOTICE_PAD - NOTICE_Y;
}

static SDL_FRect NoticeViewRect(void)
{
    SDL_FRect r = { NOTICE_X + NOTICE_PAD, NoticeButtonY(), NOTICE_BTN_W, NOTICE_BTN_H };
    return r;
}

static SDL_FRect NoticeCloseRect(void)
{
    SDL_FRect r = { NOTICE_X + NOTICE_W - NOTICE_PAD - NOTICE_BTN_W, NoticeButtonY(), NOTICE_BTN_W, NOTICE_BTN_H };
    return r;
}

static bool NoticeContains(SDL_FRect r, float mx, float my)
{
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

static void DrawNoticeButton(SDL_Renderer *renderer, SDL_FRect r, const char *label)
{
    SDL_SetRenderDrawColor(renderer, 44, 55, 75, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 255);
    SDL_RenderRect(renderer, &r);
    GUI_DrawText(renderer, (int)r.x + 12, (int)r.y + 6, label, col_white);
}

void Dialog_Notice_Show(int16_t picture_id, int32_t tile_x, int32_t tile_y)
{
    const NoticeEntry *entry = FindNotice(picture_id);
    if (!entry) return;

    notice_entry = entry;
    notice_tile_x = tile_x;
    notice_tile_y = tile_y;
    notice_has_target = (tile_x > 0 || tile_y > 0);
    WrapNoticeBody(entry->body, (int)(NOTICE_W - 2.0f * NOTICE_PAD));
    notice_shown_at = SDL_GetTicks();
    notice_active = true;
}

void Dialog_Notice_Update(uint64_t now)
{
    if (notice_active && now - notice_shown_at >= NOTICE_AUTO_CLEAR_MS) {
        notice_active = false;
    }
}

bool Dialog_Notice_HandleKeyDown(SDL_Keycode key)
{
    if (!notice_active) return false;
    if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        notice_active = false;
        return true;
    }
    return false;
}

bool Dialog_Notice_HandleClick(float mx, float my)
{
    if (!notice_active) return false;

    SDL_FRect box = { NOTICE_X, NOTICE_Y, NOTICE_W, NoticeHeight() };
    if (!NoticeContains(box, mx, my)) return false;

    if (notice_has_target && NoticeContains(NoticeViewRect(), mx, my)) {
        Video_CenterOnTile(notice_tile_x, notice_tile_y);
        return true;
    }
    if (NoticeContains(NoticeCloseRect(), mx, my)) {
        notice_active = false;
    }
    return true;
}

void Dialog_Notice_Render(SDL_Renderer *renderer)
{
    if (!notice_active || !notice_entry) return;

    SDL_FRect box = { NOTICE_X, NOTICE_Y, NOTICE_W, NoticeHeight() };
    SDL_SetRenderDrawColor(renderer, 20, 24, 34, 235);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, notice_entry->color.r, notice_entry->color.g, notice_entry->color.b, 255);
    SDL_RenderRect(renderer, &box);

    int text_x = (int)(NOTICE_X + NOTICE_PAD);
    GUI_DrawText(renderer, text_x, (int)(NOTICE_Y + 12.0f), notice_entry->title, notice_entry->color);

    for (int i = 0; i < notice_line_count; i++) {
        int line_y = (int)(NOTICE_Y + NOTICE_BODY_Y + (float)i * NOTICE_LINE_H);
        GUI_DrawText(renderer, text_x, line_y, notice_lines[i], col_white);
    }

    if (notice_has_target) {
        DrawNoticeButton(renderer, NoticeViewRect(), "View");
    }
    DrawNoticeButton(renderer, NoticeCloseRect(), "Close");
}
