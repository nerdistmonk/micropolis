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
static int16_t LastPicNum = 0;
static bool    TreasurySampled = false;
static bool    TreasuryEmptyFired = false;
static bool    TreasuryBustedFired = false;
static int32_t LastTreasury = 0;
#define TREASURY_BUSTED_LEVEL 5000
#define TREASURY_REARM 50000
/* Trash pool at which the land-value penalty maxes out (LandfillTrashPenalty), and the level it must drop below to re-arm. */
#define TRASH_PILE_LEVEL 5000
#define TRASH_REARM_LEVEL 1000
static bool    trashPiled = false;

#define NEWS_COND_SLOTS 128
#define NEWS_THROTTLE_TICKS 48
static bool    news_cond_active[NEWS_COND_SLOTS];
static int32_t news_last_tick[NEWS_COND_SLOTS];

/* Fires once when a condition turns on, and again only after it turns off and on. */
static void NewsCondition(int16_t event_id, bool active)
{
    if (active && !news_cond_active[event_id]) {
        if (EngineCallbacks.news_event) EngineCallbacks.news_event(event_id);
    }
    news_cond_active[event_id] = active;
}

void NewsThrottled(int16_t event_id)
{
    if (CityTime - news_last_tick[event_id] >= NEWS_THROTTLE_TICKS) {
        news_last_tick[event_id] = CityTime;
        if (EngineCallbacks.news_event) EngineCallbacks.news_event(event_id);
    }
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
    [45] = "End of Demo !!"
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
        if ((TotalZPop >> 2) >= ResZPop) {
            SendMes(1);
        }
        break;
    case 2:
        NewsCondition(NEWS_EVENT_HEALTH_SHORT, TotalPop > 60 && HealthEffect < 700);
        break;
    case 5:
        if ((TotalZPop >> 3) >= ComZPop) {
            SendMes(2);
        }
        break;
    case 10:
        if ((TotalZPop >> 3) >= IndZPop) {
            SendMes(3);
        }
        break;
    case 14:
        if ((TotalZPop > 10) && ((TotalZPop << 1) > RoadTotal)) {
            SendMes(4);
        }
        break;
    case 18:
        if ((TotalZPop > 50) && (TotalZPop > RailTotal)) {
            SendMes(5);
        }
        break;
    case 22:
        if ((TotalZPop > 10) && (power_pop == 0)) {
            SendMes(6);
        }
        break;
    case 26:
        if ((ResPop > 500) && (StadiumPop == 0)) {
            SendMes(7);
            ResCap = 1;
        } else {
            ResCap = 0;
        }
        break;
    case 28:
        if ((IndPop > 70) && (PortPop == 0)) {
            SendMes(8);
            IndCap = 1;
        } else {
            IndCap = 0;
        }
        break;
    case 30:
        if ((ComPop > 100) && (APortPop == 0)) {
            SendMes(9);
            ComCap = 1;
        } else {
            ComCap = 0;
        }
        break;
    case 32:
        total_zones = (float)(unPwrdZCnt + PwrdZCnt);
        if (total_zones > 0.0f) {
            if (((float)PwrdZCnt / total_zones) < 0.7f) {
                SendMes(15);
            }
        }
        break;
    case 35:
        NewsCondition(10, PolluteAverage > 60);
        NewsCondition(NEWS_EVENT_POLLUTION_LOW, TotalPop > 60 && PolluteAverage < 15);
        if (PolluteAverage > 60) {
            SendMesTickerAt(-10, 0, 0);
        }
        break;
    case 42:
        NewsCondition(11, CrimeAverage > 100);
        NewsCondition(NEWS_EVENT_CRIME_LOW, TotalPop > 60 && CrimeAverage < 20);
        if (CrimeAverage > 100) {
            SendMesTickerAt(-11, 0, 0);
        }
        break;
    case 45:
        if ((TotalPop > 60) && (FireStPop == 0)) {
            SendMes(13);
        }
        break;
    case 48:
        if ((TotalPop > 60) && (PolicePop == 0)) {
            SendMes(14);
        }
        break;
    case 51:
        if (GetAverageTaxRate() > 12) {
            SendMes(16);
        }
        break;
    case 54:
        if ((RoadEffect < 20) && (RoadTotal > 30)) {
            SendMes(17);
        }
        break;
    case 57:
        if ((FireEffect < 700) && (TotalPop > 20)) {
            SendMes(18);
        }
        break;
    case 60:
        if ((PoliceEffect < 700) && (TotalPop > 20)) {
            SendMes(19);
        }
        break;
    case 63:
        NewsCondition(12, TrafficAverage > 60);
        if (TrafficAverage > 60) {
            SendMesTickerAt(-12, 0, 0);
        }
        break;
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

/* The pile headline re-arms only after the pool drops below TRASH_REARM_LEVEL. The capacity headline follows LandfillAtCapacity. */
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
    LastPicNum = 0;
}

int32_t SendMes(int16_t mnum)
{
    if (mnum < 0) {
        if (mnum != LastPicNum) {
            MessagePort = mnum;
            MesX = 0;
            MesY = 0;
            LastPicNum = mnum;
            return 1;
        }
    } else {
        if (!MessagePort) {
            MessagePort = mnum;
            MesX = 0;
            MesY = 0;
            return 1;
        }
    }
    return 0;
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

void doMessage(void)
{
    const char *text;
    bool first_time = false;

    if (MessagePort) {
        MesNum = MessagePort;
        MessagePort = 0;
        first_time = true;
    } else {
        if (MesNum == 0) return;
        if (MesNum < 0) {
            MesNum = (int16_t)(-MesNum);
        }
    }

    if (MesNum == 0) return;

    if (first_time) PlayMessageSound(MesNum);
    text = GetMessageText(MesNum);
    SetMessageField(text);

    if (autoGo && (MesX || MesY)) {
        DoAutoGoto(MesX, MesY, text);
        MesX = 0;
        MesY = 0;
    }
}

static void DoAutoGoto(int16_t x, int16_t y, const char *msg)
{
    SetMessageField(msg);
    if (EngineCallbacks.auto_goto) {
        EngineCallbacks.auto_goto(x, y, msg);
    }
}

void SetMessageField(const char *str)
{
    if (!str || !DoMessages) return;

    if (!HaveLastMessage || strcmp(LastMessage, str) != 0) {
        strncpy(LastMessage, str, sizeof(LastMessage) - 1);
        LastMessage[sizeof(LastMessage) - 1] = '\0';
        HaveLastMessage = 1;
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