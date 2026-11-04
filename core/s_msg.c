/* s_msg.c: City notifications, advice system, and problem warning dispatch.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"

extern void Audio_MakeSound(const char *channel, const char *sound_name);

/* Internal Message State Tracking */
static QUAD    LastCityPop = 0;
static int16_t LastCategory = 0;
static int16_t LastPicNum = 0;

/* Forward Declarations of Internal Functions */
static void CheckGrowth(void);
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

    CheckGrowth();

    TotalZPop = (int16_t)(ResZPop + ComZPop + IndZPop);
    power_pop = (int16_t)(NuclearPop + CoalPop);

    int32_t z = CityTime & 63;

    switch (z) {
    case 1:
        if ((TotalZPop >> 2) >= ResZPop) {
            SendMes(1);
        }
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
        if (PolluteAverage > 60) {
            SendMesAt(-10, 0, 0);
        }
        break;
    case 42:
        if (CrimeAverage > 100) {
            SendMesAt(-11, 0, 0);
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
        if (CityTax > 12) {
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
        if (TrafficAverage > 60) {
            SendMesAt(-12, 0, 0);
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

static void PlayTrafficHonk(void)
{
    if (Rand(5) == 1) {
        Audio_MakeSound("city", "HonkHonk-Med");
    } else if (Rand(5) == 1) {
        Audio_MakeSound("city", "HonkHonk-Low");
    } else if (Rand(5) == 1) {
        Audio_MakeSound("city", "HonkHonk-High");
    }
}

void SendMesAt(int16_t mnum, int16_t x, int16_t y)
{
    if (SendMes(mnum)) {
        if (mnum == 12 || mnum == -12) PlayTrafficHonk();
        MesX = x;
        MesY = y;
        if (EngineCallbacks.show_message) {
            EngineCallbacks.show_message(mnum, x, y);
        }
    }
}

void doMessage(void)
{
    const char *text;

    if (MessagePort) {
        MesNum = MessagePort;
        MessagePort = 0;
    } else {
        if (MesNum == 0) return;
        if (MesNum < 0) {
            MesNum = (int16_t)(-MesNum);
        }
    }

    if (MesNum == 0) return;

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