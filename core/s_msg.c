/* s_msg.c: City notifications, advice system, and problem warning dispatch.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"
#include "scenario.h"

extern void Audio_MakeSound(const char *channel, const char *sound_name);

/* Internal Message State Tracking */
static QUAD    LastCityPop = 0;
static int16_t LastCategory = 0;
static bool    TreasurySampled = false;
static bool    TreasuryEmptyFired = false;
static bool    TreasuryBustedFired = false;
static int32_t LastTreasury = 0;
#define TREASURY_BUSTED_LEVEL 5000
#define TREASURY_REARM 50000
/* Stored trash at which the land-value penalty maxes out (LandfillTrashPenalty), and the level it must drop below to re-arm. */
#define TRASH_PILE_LEVEL 5000
#define TRASH_REARM_LEVEL 1000
#define UNEMPLOYMENT_LEVEL 170
/* ProblemTable[PROB_RECREATION] runs 0-255 (share of residents with no park coverage). */
#define RECREATION_LEVEL 64
static bool    trashPiled = false;

#define NEWS_COND_SLOTS 256
#define NEWS_THROTTLE_TICKS 48
static bool    news_cond_active[NEWS_COND_SLOTS];
static int32_t news_last_tick[NEWS_COND_SLOTS];

/* Fires once when a condition turns on, and again only after it turns off and on. Returns true on that rising edge. */
static bool NewsCondition(int16_t event_id, bool active)
{
    bool rising = active && !news_cond_active[event_id];
    if (rising && EngineCallbacks.news_event) EngineCallbacks.news_event(event_id);
    news_cond_active[event_id] = active;
    return rising;
}

void NewsThrottled(int16_t event_id)
{
    if (CityTime - news_last_tick[event_id] >= NEWS_THROTTLE_TICKS) {
        news_last_tick[event_id] = CityTime;
        if (EngineCallbacks.news_event) EngineCallbacks.news_event(event_id);
    }
}

/* Ports and airports share one pool of zone capacity; commercial and industrial zones beyond it are unserviced. */
static bool ZonesOverPortCapacity(void)
{
    int32_t capacity;
    int32_t demand;
    PortSystemStats(&capacity, &demand);
    return demand > capacity;
}

/* Ticker advisory that repeats while `need` holds. The newspaper gets one story per rising edge. */
static void Advisory(int16_t mnum, int16_t news_id, bool need)
{
    NewsCondition(news_id, need);
    if (need) SendMes(mnum);
}

/* Forward Declarations of Internal Functions */
static void CheckGrowth(void);
static void CheckTreasury(void);
static void CheckTrash(void);
static void DoScenarioScore(int32_t type);
static void DoAutoGoto(int16_t x, int16_t y, const char *msg);
static const char *GetMessageText(int16_t id);

/* Static Message String Resource (replaces stri.301 resource loader) */
static const char * const SimMessages[] = {
    [0]  = "",
    [1]  = "More residential zones needed.",
    [2]  = "More commercial zones needed.",
    [3]  = "More industrial zones needed.",
    [4]  = "More roads required.",
    [5]  = "Inadequate rail system.",
    [6]  = "Build a Power Plant.",
    [7]  = "Residents demand a Stadium.",
    [8]  = "Industry requires a Sea Port.",
    [9]  = "Commerce requires an Airport.",
    [10] = "Pollution very high.",
    [11] = "Crime very high.",
    [12] = "Frequent traffic jams reported.",
    [13] = "Citizens demand a Fire Department.",
    [14] = "Citizens demand a Police Department.",
    [15] = "Blackouts reported. Check power map.",
    [16] = "Citizens upset. The tax rate is too high.",
    [17] = "Roads deteriorating, due to lack of funds.",
    [18] = "Fire departments need funding.",
    [19] = "Police departments need funding.",
    [20] = "Fire reported !",
    [21] = "A Monster has been sighted !!",
    [22] = "Tornado reported !!",
    [23] = "Major earthquake reported !!!",
    [24] = "A plane has crashed !",
    [25] = "Shipwreck reported !",
    [26] = "A train crashed !",
    [27] = "A helicopter crashed !",
    [28] = "Unemployment rate is high.",
    [29] = "YOUR CITY HAS GONE BROKE!",
    [30] = "Firebombing reported !",
    [31] = "Need more parks.",
    [32] = "Explosion detected !",
    [33] = "Insufficient funds to build that.",
    [34] = "Area must be bulldozed first.",
    [35] = "Population has reached 2,000.",
    [36] = "Population has reached 10,000.",
    [37] = "Population has reached 50,000.",
    [38] = "Population has reached 100,000.",
    [39] = "Population has reached 500,000.",
    [40] = "Brownouts, build another Power Plant.",
    [41] = "Heavy Traffic reported.",
    [42] = "Flooding reported !!",
    [43] = "A Nuclear Meltdown has occurred !!!",
    [44] = "They're rioting in the streets !!",
    [45] = "Hospitals need funding.",
    [46] = "Citizens demand a Hospital.",
    [47] = "Parks go short of funding.",
    [48] = "Ports and airports are over capacity.",
    [49] = "Garage needs an adjacent road.",
    [50] = "Station needs an adjacent rail line.",
    [51] = "Gridlock reported. Commuters cannot get through."
};

static const char *GetMessageText(int16_t id)
{
    if (id < 0) id = (int16_t)(-id);
    if ((size_t)id < sizeof(SimMessages) / sizeof(SimMessages[0]) && SimMessages[id]) {
        return SimMessages[id];
    }
    return "";
}

void SendMessages(void)
{
    int16_t power_pop;
    float total_zones;

    if (ScenarioID && ScoreType && ScoreWait) {
        ScoreWait--;
        if (!ScoreWait) {
            DoScenarioScore(ScoreType);
        }
    }

    Scenario_CheckEvents();
    Scenario_CheckYear();

    CheckGrowth();
    CheckTreasury();
    CheckTrash();

    TotalZPop = (int16_t)(ResZPop + ComZPop + IndZPop);
    power_pop = (int16_t)(NuclearPop + CoalPop);

    int32_t z = CityTime & 63;

    switch (z) {
    case 1:
        Advisory(1, NEWS_EVENT_ADV_RESIDENTIAL, (TotalZPop >> 2) >= ResZPop);
        break;
    case 2:
        Advisory(45, NEWS_EVENT_HEALTH_SHORT, TotalPop > 60 && HealthEffect < 700);
        break;
    case 5:
        Advisory(2, NEWS_EVENT_ADV_COMMERCIAL, (TotalZPop >> 3) >= ComZPop);
        break;
    case 10:
        Advisory(3, NEWS_EVENT_ADV_INDUSTRIAL, (TotalZPop >> 3) >= IndZPop);
        break;
    case 14:
        Advisory(4, NEWS_EVENT_ADV_ROADS, (TotalZPop > 10) && ((TotalZPop << 1) > RoadTotal));
        break;
    case 16:
        Advisory(46, NEWS_EVENT_ADV_HOSPITAL, (TotalPop > 60) && (HospPop == 0));
        break;
    case 18:
        Advisory(5, NEWS_EVENT_ADV_RAIL, (TotalZPop > 50) && (TotalZPop > RailTotal));
        break;
    case 22:
        Advisory(6, NEWS_EVENT_ADV_POWER, (TotalZPop > 10) && (power_pop == 0));
        break;
    case 26: {
        bool need = (ResPop > 500) && (StadiumPop == 0);
        Advisory(7, NEWS_EVENT_ADV_STADIUM, need);
        ResCap = need ? 1 : 0;
        break;
    }
    case 28: {
        bool need = (IndPop > 70) && (PortPop == 0);
        Advisory(8, NEWS_EVENT_ADV_SEAPORT, need);
        IndCap = ((IndPop > 70) && (PortPop == 0 || ZonesOverPortCapacity())) ? 1 : 0;
        break;
    }
    case 29:
        Advisory(48, NEWS_EVENT_ADV_PORT_CAPACITY, (PortPop + APortPop > 0) && ZonesOverPortCapacity());
        break;
    case 30: {
        bool need = (ComPop > 100) && (APortPop == 0);
        Advisory(9, NEWS_EVENT_ADV_AIRPORT, need);
        ComCap = ((ComPop > 100) && (APortPop == 0 || ZonesOverPortCapacity())) ? 1 : 0;
        break;
    }
    case 32:
        total_zones = (float)(unPwrdZCnt + PwrdZCnt);
        Advisory(15, NEWS_EVENT_ADV_BLACKOUT,
                 total_zones > 0.0f && ((float)PwrdZCnt / total_zones) < 0.7f);
        break;
    case 35: {
        bool polluted = NewsCondition(10, PolluteAverage > 60);
        NewsCondition(NEWS_EVENT_POLLUTION_LOW, TotalPop > 60 && PolluteAverage < 15);
        if (polluted) {
            SendMesTickerAt(-10, 0, 0);
        }
        break;
    }
    case 38:
        Advisory(28, NEWS_EVENT_ADV_UNEMPLOYMENT, ProblemTable[5] > UNEMPLOYMENT_LEVEL);
        break;
    case 21:
        Advisory(47, NEWS_EVENT_ADV_PARKS_FUNDING, (RecEffect < 700) && (TotalPop > 20));
        break;
    case 40:
        Advisory(31, NEWS_EVENT_ADV_PARKS, (ResPop > 500) && (ProblemTable[PROB_RECREATION] > RECREATION_LEVEL));
        break;
    case 42: {
        bool criminal = NewsCondition(11, CrimeAverage > 100);
        NewsCondition(NEWS_EVENT_CRIME_LOW, TotalPop > 60 && CrimeAverage < 20);
        if (criminal) {
            SendMesTickerAt(-11, 0, 0);
        }
        break;
    }
    case 45:
        Advisory(13, NEWS_EVENT_ADV_FIRE_DEPT, (TotalPop > 60) && (FireStPop == 0));
        break;
    case 48:
        Advisory(14, NEWS_EVENT_ADV_POLICE_DEPT, (TotalPop > 60) && (PolicePop == 0));
        break;
    case 51:
        Advisory(16, NEWS_EVENT_ADV_TAX, GetAverageTaxRate() > 12);
        break;
    case 54:
        Advisory(17, NEWS_EVENT_ADV_ROAD_DECAY, (RoadEffect < 20) && (RoadTotal > 30));
        break;
    case 57:
        Advisory(18, NEWS_EVENT_ADV_FIRE_FUNDING, (FireEffect < 700) && (TotalPop > 20));
        break;
    case 60:
        Advisory(19, NEWS_EVENT_ADV_POLICE_FUNDING, (PoliceEffect < 700) && (TotalPop > 20));
        break;
    case 44:
        Advisory(44, NEWS_EVENT_ADV_RIOTS, (TotalPop > 60) && (CrimeAverage > 150));
        break;
    case 63: {
        bool jammed = NewsCondition(12, TrafAtCapacity());
        bool gridlocked = NewsCondition(51, TrafGridlocked());
        if (gridlocked) {
            SendMesTickerAt(-51, 0, 0);
        } else if (jammed) {
            SendMesTickerAt(-12, 0, 0);
        }
        break;
    }
    default:
        break;
    }
}

static void CheckGrowth(void)
{
    QUAD this_city_pop;
    int16_t z;

    if (!(CityTime & 3)) {
        z = 0;
        this_city_pop = ((QUAD)ResPop + ((QUAD)ComPop * 8) + ((QUAD)IndPop * 8)) * 20;

        if (LastCityPop > 0) {
            if ((LastCityPop < 2000)   && (this_city_pop >= 2000))   z = 35;
            if ((LastCityPop < 10000)  && (this_city_pop >= 10000))  z = 36;
            if ((LastCityPop < 50000)  && (this_city_pop >= 50000))  z = 37;
            if ((LastCityPop < 100000) && (this_city_pop >= 100000)) z = 38;
            if ((LastCityPop < 500000) && (this_city_pop >= 500000)) z = 39;
        }

        if (z && (z != LastCategory)) {
            SendMesAt((int16_t)(-z), 0, 0);
            LastCategory = z;
        }
        LastCityPop = this_city_pop;
    }
}

/* Empty and nearly-busted headlines fire once per dip and re-arm after funds recover to TREASURY_REARM. The million headline fires on each upward crossing, as before. */
static void CheckTreasury(void)
{
    if (!TreasurySampled) {
        TreasuryEmptyFired = TotalFunds <= 0;
        TreasuryBustedFired = TotalFunds < TREASURY_BUSTED_LEVEL;
        TreasurySampled = true;
        LastTreasury = TotalFunds;
        return;
    }
    if (!TreasuryEmptyFired && TotalFunds <= 0) {
        TreasuryEmptyFired = true;
        if (EngineCallbacks.news_event) EngineCallbacks.news_event(NEWS_EVENT_TREASURY_EMPTY);
        SendMesTickerAt(-29, 0, 0);
    } else if (TreasuryEmptyFired && TotalFunds >= TREASURY_REARM) {
        TreasuryEmptyFired = false;
    }
    if (!TreasuryBustedFired && TotalFunds < TREASURY_BUSTED_LEVEL) {
        TreasuryBustedFired = true;
        if (EngineCallbacks.news_event) EngineCallbacks.news_event(NEWS_EVENT_TREASURY_BUSTED);
    } else if (TreasuryBustedFired && TotalFunds >= TREASURY_REARM) {
        TreasuryBustedFired = false;
    }
    if (LastTreasury < 1000000 && TotalFunds >= 1000000) {
        if (EngineCallbacks.news_event) EngineCallbacks.news_event(NEWS_EVENT_TREASURY_MILLION);
    }
    LastTreasury = TotalFunds;
}

/* The pile headline re-arms only after stored trash drops below TRASH_REARM_LEVEL. The capacity headline follows LandfillAtCapacity. */
static void CheckTrash(void)
{
    if (TrashPending >= TRASH_PILE_LEVEL) {
        trashPiled = true;
    } else if (TrashPending < TRASH_REARM_LEVEL) {
        trashPiled = false;
    }
    NewsCondition(NEWS_EVENT_GARBAGE_PILE, trashPiled);
    NewsCondition(NEWS_EVENT_LANDFILL_FULL, LandfillAtCapacity);
}

void News_ResetSession(void)
{
    TreasurySampled = false;
    TreasuryEmptyFired = false;
    TreasuryBustedFired = false;
    LastTreasury = 0;
    trashPiled = false;
    memset(news_cond_active, 0, sizeof(news_cond_active));
    for (size_t i = 0; i < NEWS_COND_SLOTS; i++) news_last_tick[i] = -NEWS_THROTTLE_TICKS;
    if (EngineCallbacks.news_reset) EngineCallbacks.news_reset();
}

static void DoScenarioScore(int32_t type)
{
    int16_t z = -200; /* Defeat threshold */

    switch (type) {
    case 1: /* Dullsville */
    case 2: /* San Francisco */
    case 3: /* Hamburg */
        if (CityClass >= 4) z = -100;
        break;
    case 4: /* Bern */
        if (TrafficAverage < 80) z = -100;
        break;
    case 5: /* Tokyo */
    case 7: /* Boston */
    case 8: /* Rio de Janeiro */
        if (CityScore > 500) z = -100;
        break;
    case 6: /* Detroit */
        if (CrimeAverage < 60) z = -100;
        break;
    default:
        break;
    }

    ClearMes();
    SendMes(z);

    if (z == -200) {
        DoLoseGame();
    } else if (z == -100) {
        DoWinGame();
    }
}

void ResetMilestoneState(void)
{
    LastCityPop = 0;
    LastCategory = 0;
}

void ClearMes(void)
{
    MessagePort = 0;
    MesX = 0;
    MesY = 0;
}

int32_t SendMes(int16_t mnum)
{
    /* Disasters always post, so a repeat is still reported; advisories wait for a free port. */
    if (mnum >= 0 && MessagePort) return 0;

    MessagePort = mnum;
    MesX = 0;
    MesY = 0;
    return 1;
}

static void PlayMessageSound(int16_t mnum)
{
    switch (mnum < 0 ? -mnum : mnum) {
    case 12:
        if (Rand(5) == 1) {
            Audio_MakeSound("city", "HonkHonk-Med");
        } else if (Rand(5) == 1) {
            Audio_MakeSound("city", "HonkHonk-Low");
        } else if (Rand(5) == 1) {
            Audio_MakeSound("city", "HonkHonk-High");
        }
        break;
    case 11:
    case 20:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 44:
        Audio_MakeSound("city", "siren");
        break;
    case 21: {
        char sound[64];
        snprintf(sound, sizeof(sound), "monster -speed %d", (int)MonsterSpeed());
        Audio_MakeSound("city", sound);
        break;
    }
    case 30:
        Audio_MakeSound("city", "Explosion-Low");
        Audio_MakeSound("city", "siren");
        break;
    case 43:
        Audio_MakeSound("city", "Explosion-High");
        Audio_MakeSound("city", "Explosion-Low");
        Audio_MakeSound("city", "siren");
        break;
    default:
        break;
    }
}

void SendMesTickerAt(int16_t mnum, int16_t x, int16_t y)
{
    if (SendMes(mnum)) {
        MesX = x;
        MesY = y;
        if (EngineCallbacks.show_message) {
            EngineCallbacks.show_message(mnum, x, y);
        }
    }
}

void SendMesAt(int16_t mnum, int16_t x, int16_t y)
{
    /* Reported before the repeat check in SendMes, so every disaster reaches the newspaper. */
    if (EngineCallbacks.news_event && mnum != 0) {
        EngineCallbacks.news_event((int16_t)(mnum < 0 ? -mnum : mnum));
    }
    SendMesTickerAt(mnum, x, y);
}

/* Where the ticker's "go to" click lands for notices that the core sends with no coordinates. */
static void AlertTarget(int16_t mnum, int16_t *x, int16_t *y)
{
    switch (mnum) {
    case -10: *x = PolMaxX;   *y = PolMaxY;   break;
    case -11: *x = CrimeMaxX; *y = CrimeMaxY; break;
    case -12: case -51: *x = TrafMaxX; *y = TrafMaxY; break;
    case -35: case -36: case -37: case -38: case -39: case -40:
        *x = CCx; *y = CCy;
        break;
    default:
        break;
    }
}

static void DoAutoGoto(int16_t x, int16_t y, const char *msg)
{
    if (EngineCallbacks.auto_goto) {
        EngineCallbacks.auto_goto(x, y, msg);
    }
}

void doMessage(void)
{
    const char *text;
    int16_t mnum;
    int16_t sent_x, sent_y;
    int16_t target_x, target_y;

    if (!MessagePort) return;

    mnum = MessagePort;
    MessagePort = 0;
    MesNum = mnum;

    sent_x = MesX;
    sent_y = MesY;
    MesX = 0;
    MesY = 0;

    PlayMessageSound(mnum);
    text = GetMessageText(mnum);

    target_x = sent_x;
    target_y = sent_y;
    if (target_x == 0 && target_y == 0) AlertTarget(mnum, &target_x, &target_y);
    SetMessageField(text, target_x, target_y, mnum < 0 || sent_x != 0 || sent_y != 0);

    if (autoGo && (sent_x || sent_y)) {
        DoAutoGoto(sent_x, sent_y, text);
    }
}

/* Event notices bump MessageSerial even when their text repeats, so the ticker shows each one. */
void SetMessageField(const char *str, int16_t x, int16_t y, bool is_event)
{
    if (!str || !DoMessages) return;

    if (is_event || !HaveLastMessage || strcmp(LastMessage, str) != 0) {
        strncpy(LastMessage, str, sizeof(LastMessage) - 1);
        LastMessage[sizeof(LastMessage) - 1] = '\0';
        HaveLastMessage = 1;
        MessageTargetX = x;
        MessageTargetY = y;
        MessageSerial++;
    }
}

void DoLoseGame(void)
{
    if (EngineCallbacks.show_message) {
        EngineCallbacks.show_message(-200, 0, 0);
    }
}

void DoWinGame(void)
{
    if (EngineCallbacks.show_message) {
        EngineCallbacks.show_message(-100, 0, 0);
    }
}