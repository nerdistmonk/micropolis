/* sdl3_dialog_news.c: Newspaper headlines modal, opened by clicking the status-bar date.
 * Shows only the headlines for the current in-game year, so the list changes as the year rolls over.
 */

#include "sdl3_dialog_common.h"

#define NEWS_DLG_W 620.0f
#define NEWS_TITLE_Y 16.0f
#define NEWS_LIST_TOP 52.0f
#define NEWS_LINE_PITCH 30.0f
#define NEWS_TEXT_X 28.0f
#define NEWS_FOOTER_PAD 24.0f

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

static int32_t NewsCurrentYear(void)
{
    return StartingYear + (CityTime / 48);
}

static size_t NewsCountForYear(int32_t year)
{
    size_t n = 0;
    for (size_t i = 0; i < NEWS_ITEM_COUNT; i++) {
        if (news_items[i].year == year) n++;
    }
    return n;
}

static float NewsDialogHeight(int32_t year)
{
    size_t rows = NewsCountForYear(year);
    if (rows == 0) rows = 1;
    return NEWS_LIST_TOP + (float)rows * NEWS_LINE_PITCH + NEWS_FOOTER_PAD;
}

bool Dialog_News_HandleClick(float mx, float my)
{
    float dw = NEWS_DLG_W;
    float dh = NewsDialogHeight(NewsCurrentYear());
    float dx = DialogCenteredX(dw);
    float dy = DialogCenteredY(dh);

    if (mx >= dx + dw - 32.0f && mx <= dx + dw - 8.0f &&
        my >= dy + 8.0f && my <= dy + 32.0f) {
        GUI_Dialogs_Close();
        return true;
    }

    return true;
}

void Dialog_News_Render(SDL_Renderer *renderer)
{
    int32_t year = NewsCurrentYear();
    float dw = NEWS_DLG_W;
    float dh = NewsDialogHeight(year);
    float dx = DialogCenteredX(dw);
    float dy = DialogCenteredY(dh);

    SDL_FRect scrim = { 0.0f, 0.0f, (float)Video_GetCanvasWidth(), (float)Video_GetCanvasHeight() };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win_rect = { dx, dy, dw, dh };
    SDL_SetRenderDrawColor(renderer, 24, 28, 38, 250);
    SDL_RenderFillRect(renderer, &win_rect);

    SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
    SDL_RenderRect(renderer, &win_rect);

    char title[64];
    snprintf(title, sizeof(title), "HEADLINES OF %d", (int)year);
    GUI_DrawText(renderer, (int)dx + 16, (int)dy + (int)NEWS_TITLE_Y, title, col_gold);

    SDL_FRect close_rect = { dx + dw - 28.0f, dy + 8.0f, 20.0f, 20.0f };
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_rect);
    GUI_DrawText(renderer, (int)close_rect.x + 6, (int)close_rect.y + 4, "X", col_white);

    size_t row = 0;
    for (size_t i = 0; i < NEWS_ITEM_COUNT; i++) {
        if (news_items[i].year != year) continue;
        float y = dy + NEWS_LIST_TOP + (float)row * NEWS_LINE_PITCH;
        GUI_DrawText(renderer, (int)(dx + NEWS_TEXT_X), (int)y, news_items[i].headline, col_white);
        row++;
    }

    if (row == 0) {
        GUI_DrawText(renderer, (int)(dx + NEWS_TEXT_X), (int)(dy + NEWS_LIST_TOP), "No headlines this year.", col_gray);
    }
}
