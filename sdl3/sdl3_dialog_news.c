/* sdl3_dialog_news.c: Newspaper headlines modal, opened by clicking the status-bar date.
 * Shows only the headlines for the current in-game year, so the list changes as the year rolls over.
 * Real-world entries come first; game events (disasters, population, treasury) follow in the order they happened.
 * The list scrolls when it is longer than NEWS_VISIBLE_ROWS. Clear drops the game events; unread game events are marked read on open.
 */

#include "sdl3_dialog_common.h"

#define NEWS_DLG_W 620.0f
#define NEWS_TITLE_Y 16.0f
#define NEWS_LIST_TOP 52.0f
#define NEWS_LINE_PITCH 30.0f
#define NEWS_TEXT_X 28.0f
#define NEWS_VISIBLE_ROWS 8
#define NEWS_FOOTER_H 48.0f

typedef struct {
    int32_t year;
    const char *headline;
} NewsItem;

static const NewsItem news_items[] = {
    { 1900, "Hurricane devastates Galveston, Texas" },
    { 1901, "Queen Victoria dies; Edward VII takes the throne" },
    { 1903, "Wright brothers make first powered flight at Kitty Hawk" },
    { 1906, "San Francisco shaken by earthquake and fire" },
    { 1908, "Ford introduces the Model T automobile" },
    { 1911, "Amundsen reaches the South Pole" },
    { 1912, "Titanic sinks on maiden voyage" },
    { 1914, "Archduke assassinated in Sarajevo; Europe on edge" },
    { 1914, "Panama Canal opens to shipping" },
    { 1917, "Bolsheviks seize power in Russia" },
    { 1918, "Armistice signed; the Great War ends" },
    { 1919, "Treaty of Versailles signed" },
    { 1920, "Women win the right to vote with 19th Amendment" },
    { 1927, "Lindbergh lands in Paris after solo Atlantic flight" },
    { 1928, "Fleming discovers penicillin in a London lab" },
    { 1929, "Stock market crashes; Black Tuesday" },
    { 1930, "Tombaugh discovers Pluto at Lowell Observatory" },
    { 1935, "President signs Social Security Act" },
    { 1937, "Hindenburg bursts into flames over New Jersey" },
    { 1939, "Germany invades Poland; war begins in Europe" },
    { 1941, "Pearl Harbor attacked; U.S. enters the war" },
    { 1944, "Allies storm the beaches of Normandy" },
    { 1945, "Atomic bomb destroys Hiroshima" },
    { 1945, "Germany surrenders; Europe celebrates V-E Day" },
    { 1945, "Japan surrenders; World War II ends" },
    { 1947, "Jackie Robinson breaks baseball's color barrier" },
    { 1948, "State of Israel declared" },
    { 1953, "Watson and Crick publish the DNA double helix" },
    { 1954, "Bannister breaks the four-minute mile" },
    { 1955, "Disneyland opens in Anaheim" },
    { 1957, "Sputnik launched; the space age begins" },
    { 1959, "Alaska becomes the 49th state" },
    { 1959, "Hawaii becomes the 50th state" },
    { 1963, "President Kennedy assassinated in Dallas" },
    { 1964, "Beatles arrive in America; Beatlemania erupts" },
    { 1965, "Voting Rights Act signed into law" },
    { 1968, "Martin Luther King Jr. assassinated in Memphis" },
    { 1968, "Apollo 8 orbits the Moon" },
    { 1969, "Apollo 11 lands on the Moon" },
    { 1972, "Five arrested in Watergate break-in" },
    { 1973, "Oil embargo sends gas prices soaring" },
    { 1974, "Nixon resigns the presidency" },
    { 1976, "Viking 1 lands on Mars" },
    { 1979, "Partial meltdown at Three Mile Island" },
    { 1981, "Space shuttle Columbia makes first flight" },
    { 1984, "Apple introduces the Macintosh" },
    { 1986, "Space shuttle Challenger lost after launch" },
    { 1986, "Chernobyl reactor explodes" },
    { 1989, "Berlin Wall falls" },
    { 1990, "Germany reunites as one nation" },
    { 1990, "Hubble Space Telescope launched" },
    { 1991, "Soviet Union dissolves" },
    { 1994, "Nelson Mandela elected president of South Africa" },
    { 1995, "Bombing in Oklahoma City kills 168" },
    { 1997, "Scientists clone a sheep named Dolly" },
    { 1999, "Euro becomes official currency in 11 European nations" },
    { 2000, "Draft of the human genome completed" },
    { 2001, "Terror attacks on New York and Washington" },
    { 2003, "Space shuttle Columbia breaks up on re-entry" },
    { 2007, "Apple unveils the iPhone" },
    { 2010, "Earthquake devastates Haiti" },
    { 2011, "Tsunami and meltdowns strike Japan's Fukushima" },
};

#define NEWS_ITEM_COUNT (sizeof(news_items) / sizeof(news_items[0]))

#define NEWS_LOG_CAP 16
#define NEWS_GAME_ROWS 8

typedef struct {
    int16_t event_id;
    const char *headline;
} NewsGameHeadline;

static const NewsGameHeadline game_headlines[] = {
    { 20, "Fire breaks out in the city" },
    { 21, "Monster sighted, stomping through the city" },
    { 22, "Tornado tears through the city" },
    { 23, "Major earthquake rocks the city" },
    { 24, "Plane crashes near the city" },
    { 25, "Shipwreck reported offshore" },
    { 26, "Train derails in a crash" },
    { 27, "Helicopter crashes in the city" },
    { 30, "Firebombing rocks the city" },
    { 35, "City population reaches 2,000" },
    { 36, "City population reaches 10,000" },
    { 37, "City population reaches 50,000" },
    { 38, "City population reaches 100,000" },
    { 39, "City population reaches 500,000" },
    { 42, "Floodwaters spread through the city" },
    { 43, "Nuclear meltdown at the power plant" },
    { NEWS_EVENT_TREASURY_EMPTY, "City treasury runs dry" },
    { NEWS_EVENT_TREASURY_MILLION, "City treasury tops $1,000,000" },
};

typedef struct {
    int32_t year;
    int16_t event_id;
    bool unread;
} NewsLogEntry;

static NewsLogEntry news_log[NEWS_LOG_CAP];
static size_t news_log_head = 0;
static size_t news_log_count = 0;
static size_t news_scroll = 0;

static int32_t NewsCurrentYear(void)
{
    return StartingYear + (CityTime / 48);
}

static const char *NewsGameText(int16_t event_id)
{
    for (size_t i = 0; i < sizeof(game_headlines) / sizeof(game_headlines[0]); i++) {
        if (game_headlines[i].event_id == event_id) return game_headlines[i].headline;
    }
    return NULL;
}

/* Index of the i-th oldest entry in the ring. */
static size_t NewsLogIndex(size_t i)
{
    return (news_log_head + NEWS_LOG_CAP - news_log_count + i) % NEWS_LOG_CAP;
}

void News_OnGameEvent(int16_t event_id)
{
    if (!NewsGameText(event_id)) return;

    news_log[news_log_head].year = NewsCurrentYear();
    news_log[news_log_head].event_id = event_id;
    news_log[news_log_head].unread = true;
    news_log_head = (news_log_head + 1) % NEWS_LOG_CAP;
    if (news_log_count < NEWS_LOG_CAP) news_log_count++;
}

void News_ResetLog(void)
{
    news_log_head = 0;
    news_log_count = 0;
}

static void NewsMarkAllRead(void)
{
    for (size_t i = 0; i < news_log_count; i++) news_log[NewsLogIndex(i)].unread = false;
}

bool News_HasUnread(void)
{
    int32_t year = NewsCurrentYear();
    for (size_t i = 0; i < news_log_count; i++) {
        const NewsLogEntry *e = &news_log[NewsLogIndex(i)];
        if (e->unread && e->year == year) return true;
    }
    return false;
}

/* Fills out[] with the newest game headlines for `year`, oldest first. Returns how many were written. */
static size_t NewsCollectGame(int32_t year, const char *out[NEWS_GAME_ROWS])
{
    size_t total = 0;
    for (size_t i = 0; i < news_log_count; i++) {
        if (news_log[NewsLogIndex(i)].year == year) total++;
    }

    size_t skip = total > NEWS_GAME_ROWS ? total - NEWS_GAME_ROWS : 0;
    size_t seen = 0;
    size_t n = 0;
    for (size_t i = 0; i < news_log_count; i++) {
        const NewsLogEntry *e = &news_log[NewsLogIndex(i)];
        if (e->year != year) continue;
        if (seen++ < skip) continue;
        out[n++] = NewsGameText(e->event_id);
    }
    return n;
}

#define NEWS_ROWS_MAX (NEWS_ITEM_COUNT + NEWS_GAME_ROWS)

typedef struct {
    float dx, dy, dw, dh;
    size_t total;
    size_t visible;
    size_t max_scroll;
} NewsGeom;

typedef struct {
    SDL_FRect clear, up, down, track;
} NewsRects;

/* Historical entries for the year first, then the game events. Returns the row count. */
static size_t NewsBuildRows(int32_t year, const char *out[NEWS_ROWS_MAX])
{
    size_t n = 0;
    for (size_t i = 0; i < NEWS_ITEM_COUNT; i++) {
        if (news_items[i].year == year) out[n++] = news_items[i].headline;
    }
    const char *game[NEWS_GAME_ROWS];
    size_t game_count = NewsCollectGame(year, game);
    for (size_t g = 0; g < game_count; g++) out[n++] = game[g];
    return n;
}

/* Also clamps the scroll offset so it never points past the last row. */
static NewsGeom NewsLayout(int32_t year)
{
    NewsGeom g;
    const char *rows[NEWS_ROWS_MAX];
    g.total = NewsBuildRows(year, rows);
    g.visible = g.total == 0 ? 1 : (g.total < NEWS_VISIBLE_ROWS ? g.total : NEWS_VISIBLE_ROWS);
    g.max_scroll = g.total > g.visible ? g.total - g.visible : 0;
    if (news_scroll > g.max_scroll) news_scroll = g.max_scroll;
    g.dw = NEWS_DLG_W;
    g.dh = NEWS_LIST_TOP + (float)g.visible * NEWS_LINE_PITCH + NEWS_FOOTER_H;
    g.dx = DialogCenteredX(g.dw);
    g.dy = DialogCenteredY(g.dh);
    return g;
}

static NewsRects NewsRectsFor(const NewsGeom *g)
{
    float list_y = g->dy + NEWS_LIST_TOP;
    float list_h = (float)g->visible * NEWS_LINE_PITCH;
    float bar_x = g->dx + g->dw - 30.0f;
    NewsRects r = {
        .clear = { g->dx + 16.0f, g->dy + g->dh - 38.0f, 120.0f, 26.0f },
        .up = { bar_x, list_y, 14.0f, 14.0f },
        .down = { bar_x, list_y + list_h - 14.0f, 14.0f, 14.0f },
        .track = { bar_x, list_y + 16.0f, 14.0f, list_h - 32.0f }
    };
    return r;
}

static bool NewsHit(SDL_FRect r, float mx, float my)
{
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

static void NewsDrawButton(SDL_Renderer *renderer, SDL_FRect rect, const char *label, bool enabled)
{
    SDL_SetRenderDrawColor(renderer, enabled ? 48 : 34, enabled ? 24 : 34, enabled ? 28 : 40, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, enabled ? 180 : 70, enabled ? 60 : 70, enabled ? 70 : 80, 255);
    SDL_RenderRect(renderer, &rect);
    GUI_DrawTextCentered(renderer, rect, label, enabled ? col_white : col_gray);
}

void Dialog_News_Open(void)
{
    news_scroll = 0;
    NewsMarkAllRead();
}

bool Dialog_News_HandleWheel(float wheel_y)
{
    NewsGeom g = NewsLayout(NewsCurrentYear());
    if (wheel_y > 0.0f && news_scroll > 0) news_scroll--;
    else if (wheel_y < 0.0f && news_scroll < g.max_scroll) news_scroll++;
    return true;
}

bool Dialog_News_HandleClick(float mx, float my)
{
    NewsGeom g = NewsLayout(NewsCurrentYear());
    NewsRects r = NewsRectsFor(&g);

    if (mx >= g.dx + g.dw - 32.0f && mx <= g.dx + g.dw - 8.0f &&
        my >= g.dy + 8.0f && my <= g.dy + 32.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    if (NewsHit(r.clear, mx, my)) {
        News_ResetLog();
        return true;
    }

    if (g.max_scroll > 0) {
        if (NewsHit(r.up, mx, my) && news_scroll > 0) news_scroll--;
        else if (NewsHit(r.down, mx, my) && news_scroll < g.max_scroll) news_scroll++;
    }

    return true;
}

void Dialog_News_Render(SDL_Renderer *renderer)
{
    int32_t year = NewsCurrentYear();
    NewsGeom g = NewsLayout(year);
    float dw = g.dw;
    float dh = g.dh;
    float dx = g.dx;
    float dy = g.dy;

    SDL_FRect scrim = { 0.0f, 0.0f, (float)Video_GetCanvasWidth(), (float)Video_GetCanvasHeight() };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 24, 28, 38, 250);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
    SDL_RenderRect(renderer, &win_rect);

    char title[128];
    snprintf(title, sizeof(title), "%s Herald - Headlines of %d", CityName, (int)year);
    GUI_DrawText(renderer, (int)dx + 16, (int)dy + (int)NEWS_TITLE_Y, title, col_gold);

    SDL_FRect close_rect = { dx + dw - 28.0f, dy + 8.0f, 20.0f, 20.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 6, (int)close_rect.y + 4, "X", col_white);

    const char *rows[NEWS_ROWS_MAX];
    size_t total = NewsBuildRows(year, rows);
    if (total == 0) {
        GUI_DrawText(renderer, (int)(dx + NEWS_TEXT_X), (int)(dy + NEWS_LIST_TOP), "No headlines this year.", col_gray);
    }
    for (size_t i = 0; i < g.visible && news_scroll + i < total; i++) {
        float y = dy + NEWS_LIST_TOP + (float)i * NEWS_LINE_PITCH;
        GUI_DrawText(renderer, (int)(dx + NEWS_TEXT_X), (int)y, rows[news_scroll + i], col_white);
    }

    NewsRects r = NewsRectsFor(&g);
    if (g.max_scroll > 0) {
        SDL_SetRenderDrawColor(renderer, 30, 36, 50, 255);
        SDL_RenderFillRect(renderer, &r.track);

        float thumb_h = r.track.h * (float)g.visible / (float)g.total;
        if (thumb_h < 16.0f) thumb_h = 16.0f;
        float thumb_y = r.track.y + (r.track.h - thumb_h) * ((float)news_scroll / (float)g.max_scroll);
        SDL_FRect thumb = { r.track.x, thumb_y, r.track.w, thumb_h };
        SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
        SDL_RenderFillRect(renderer, &thumb);

        NewsDrawButton(renderer, r.up, "^", news_scroll > 0);
        NewsDrawButton(renderer, r.down, "v", news_scroll < g.max_scroll);
    }

    NewsDrawButton(renderer, r.clear, "Clear", news_log_count > 0);
}
