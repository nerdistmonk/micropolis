/* sim.h: Main include file for the SimCity / Micropolis engine
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#ifndef SIM_H
#define SIM_H

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Standard Boolean definitions */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Basic legacy type compatibility mappings */
typedef uint8_t Byte;
typedef int8_t SignedByte;
typedef void *Ptr;
typedef void **Handle;
typedef int32_t QUAD;

/* World Dimensions */
#ifdef MEGA
#define SimWidth (120 * MEGA)
#define SimHeight (100 * MEGA)
#else
#define SimWidth 120
#define SimHeight 100
#endif

#define WORLD_X SimWidth
#define WORLD_Y SimHeight
#define HWLDX (SimWidth >> 1)
#define HWLDY (SimHeight >> 1)
#define QWX (SimWidth >> 2)
#define QWY (SimHeight >> 2)
#define SmX (SimWidth >> 3)
#define SmY ((SimHeight + 7) >> 3)

#define EDITOR_W (WORLD_X * 16)
#define EDITOR_H (WORLD_Y * 16)
#define MAP_W (WORLD_X * 3)
#define MAP_H (WORLD_Y * 3)

#define NIL 0
#define HORIZ 1
#define VERT 0

#define PROB_RECREATION 11
#define PROB_ACTIVE 12
#define PROB_NONE PROB_ACTIVE
#define PROBNUM (PROB_ACTIVE + 1)
#define HISTLEN 480
#define MISCHISTLEN 240

/* Power grid bit arithmetic */
#define POWERMAPROW ((WORLD_X + 15) / 16)

#ifdef MEGA
#define POWERMAPLEN ((QUAD)(2 * POWERMAPROW * WORLD_Y))
#define POWERWORD(x, y) ((((QUAD)(x)) >> 4) + (((QUAD)(y)) * POWERMAPROW))
#else
#define POWERMAPLEN 1700
#define POWERWORD(x, y) (((x) >> 4) + ((y) << 3))
#endif

#define SETPOWERBIT(x, y) (PowerMap[POWERWORD((x), (y))] |= (uint16_t)(1u << ((x) & 15)))
#define PWRMAPSIZE (POWERMAPROW * WORLD_Y)
#define PWRSTKSIZE ((WORLD_X * WORLD_Y) / 4)

/* Map Overlay Indexes */
#define ALMAP 0 /* all */
#define REMAP 1 /* residential */
#define COMAP 2 /* commercial */
#define INMAP 3 /* industrial */
#define PRMAP 4 /* power */
#define RDMAP 5 /* road */
#define PDMAP 6 /* population density */
#define RGMAP 7 /* rate of growth */
#define TDMAP 8 /* traffic density */
#define PLMAP 9 /* pollution */
#define CRMAP 10 /* crime */
#define LVMAP 11 /* land value */
#define FIMAP 12 /* fire radius */
#define POMAP 13 /* police radius */
#define DYMAP 14 /* dynamic */
#define NMAPS 15

/* Frequency regulators in Simulate() */
#define VALVERATE 2
#define CENSUSRATE 4
#define TAXFREQ 48

/* 16 Standard Simulation Colors */
#define COLOR_WHITE 0
#define COLOR_YELLOW 1
#define COLOR_ORANGE 2
#define COLOR_RED 3
#define COLOR_DARKRED 4
#define COLOR_DARKBLUE 5
#define COLOR_LIGHTBLUE 6
#define COLOR_BROWN 7
#define COLOR_LIGHTGREEN 8
#define COLOR_DARKGREEN 9
#define COLOR_OLIVE 10
#define COLOR_LIGHTBROWN 11
#define COLOR_LIGHTGRAY 12
#define COLOR_MEDIUMGRAY 13
#define COLOR_DARKGRAY 14
#define COLOR_BLACK 15

/* Tile Status Bitmasks (Upper 6 bits of a 16-bit tile) */
#define PWRBIT 0x8000 /* Bit 15 - Power status */
#define CONDBIT 0x4000 /* Bit 14 - Conductive */
#define BURNBIT 0x2000 /* Bit 13 - Combustible */
#define BULLBIT 0x1000 /* Bit 12 - Bulldozable */
#define ANIMBIT 0x0800 /* Bit 11 - Animated */
#define ZONEBIT 0x0400 /* Bit 10 - Zone center */
#define ALLBITS 0xFC00 /* Mask for top 6 status bits */
#define LOMASK 0x03FF /* Mask for low 10 tile index bits */

#define BLBNBIT (BULLBIT | BURNBIT)
#define BLBNCNBIT (BULLBIT | BURNBIT | CONDBIT)
#define BNCNBIT (BURNBIT | CONDBIT)

/* Object / Sprite Numbers */
#define TRA 1
#define COP 2
#define AIR 3
#define SHI 4
#define GOD 5
#define TOR 6
#define EXP 7
#define BUS 8
#define OBJN 9
#define EXP_SLOTS 64
#define TRA_SLOTS 24
/* Most trains one linked neighbour side can run at once, matching the largest per-side count in s_sprite.c. */
#define NB_TRAINS_PER_SIDE 2

/* Graph History Indexes */
#define RES_HIST 0
#define COM_HIST 1
#define IND_HIST 2
#define MONEY_HIST 3
#define CRIME_HIST 4
#define POLLUTION_HIST 5
#define HISTORIES 6
#define ALL_HISTORIES ((1 << HISTORIES) - 1)

/* Simulation Tile IDs */
#define DIRT 0
#define RIVER 2
#define REDGE 3
#define CHANNEL 4
#define FIRSTRIVEDGE 5
#define LASTRIVEDGE 20
#define TREEBASE 21
#define LASTTREE 36
#define WOODS 37
#define UNUSED_TRASH1 38
#define UNUSED_TRASH2 39
#define WOODS2 40
#define WOODS3 41
#define WOODS4 42
#define WOODS5 43
#define RUBBLE 44
#define LASTRUBBLE 47
#define FLOOD 48
#define LASTFLOOD 51
#define RADTILE 52
#define UNUSED_TRASH3 53
#define UNUSED_TRASH4 54
#define UNUSED_TRASH5 55
#define FIRE 56
#define FIREBASE 56
#define LASTFIRE 63
#define ROADBASE 64
#define HBRIDGE 64
#define VBRIDGE 65
#define ROADS 66
#define INTERSECTION 76
#define HROADPOWER 77
#define VROADPOWER 78
#define BRWH 79
#define LTRFBASE 80
#define BRWV 95
#define HTRFBASE 144
#define LASTROAD 206
#define POWERBASE 208
#define HPOWER 208
#define VPOWER 209
#define LHPOWER 210
#define LVPOWER 211
#define RAILHPOWERV 221
#define RAILVPOWERH 222
#define LASTPOWER 222
#define RAILBASE 224
#define HRAIL 224
#define VRAIL 225
#define LHRAIL 226
#define LVRAIL 227
#define HRAILROAD 237
#define VRAILROAD 238
#define LASTRAIL 238
#define ROADVPOWERH 239
#define RESBASE 240
#define FREEZ 244
#define HOUSE 249
#define LHTHR 249
#define HHTHR 260
#define RZB 265
#define HOSPITAL 409
#define CHURCH 418
#define COMBASE 423
#define COMCLR 427
#define CZB 436
#define INDBASE 612
#define INDCLR 616
#define LASTIND 620
#define IND1 621
#define IZB 625
#define IND2 641
#define IND3 644
#define IND4 649
#define IND5 650
#define IND6 676
#define IND7 677
#define IND8 686
#define IND9 689
#define PORTBASE 693
#define PORT 698
#define LASTPORT 708
#define AIRPORTBASE 709
#define RADAR 711
#define AIRPORT 716
#define COALBASE 745
#define POWERPLANT 750
#define LASTPOWERPLANT 760
#define FIRESTBASE 761
#define FIRESTATION 765
#define POLICESTBASE 770
#define POLICESTATION 774
#define STADIUMBASE 779
#define STADIUM 784
#define FULLSTADIUM 800
#define NUCLEARBASE 811
#define NUCLEAR 816
#define LASTZONE 826
#define LIGHTNINGBOLT 827
#define HBRDG0 828
#define HBRDG1 829
#define HBRDG2 830
#define HBRDG3 831
#define RADAR0 832
#define RADAR1 833
#define RADAR2 834
#define RADAR3 835
#define RADAR4 836
#define RADAR5 837
#define RADAR6 838
#define RADAR7 839
#define FOUNTAIN 840
/* Park fountains animate through tiles FOUNTAIN..FOUNTAIN+3 (see animtab.h). */
#define FOUNTAIN_FRAMES 4
#define INDBASE2 844
#define TELEBASE 844
#define TELELAST 851
#define SMOKEBASE 852
#define TINYEXP 860
#define SOMETINYEXP 864
#define LASTTINYEXP 867
#define COALSMOKE1 916
#define COALSMOKE2 920
#define COALSMOKE3 924
#define COALSMOKE4 928
#define FOOTBALLGAME1 932
#define FOOTBALLGAME2 940
#define VBRDG0 948
#define VBRDG1 949
#define VBRDG2 950
#define VBRDG3 951

/* Water tower: 3x3 block of tiles 960-968 from images/water_tower.xpm, row-major from top-left */
#define WATERTOWER 960
#define WATERTOWER_TILES 9

/* Landfill: four 3x3 fill stages (empty, quarter, half, full) from images/landfill.xpm, tile IDs 970-1005.
 * Keep IDs below 1024 so they do not reach ZONEBIT (0x0400). */
#define LANDFILL 970
#define LANDFILL_STAGE_TILES 9
#define LANDFILL_STAGES 4
#define LANDFILL_TILES (LANDFILL_STAGE_TILES * LANDFILL_STAGES)
#define LANDFILL_CENTRE_OFFSET 4

/* Parking garage and passenger station: 3x3 traffic blocks, tile IDs 1006-1023.
 * Both are appended to the sheet after landfill, so the IDs fit under LOMASK. */
#define GARAGE 1006
#define GARAGE_TILES 9
#define STATION 1015
#define STATION_TILES 9

/* No-water icon: the tile after the water tower block, drawn on zones lacking water */
#define WATERDROP 969

/* Tool / Wand States */
#define residentialState 0
#define commercialState 1
#define industrialState 2
#define fireState 3
#define queryState 4
#define policeState 5
#define wireState 6
#define dozeState 7
#define rrState 8
#define roadState 9
#define chalkState 10
#define eraserState 11
#define stadiumState 12
#define parkState 13
#define seaportState 14
#define powerState 15
#define nuclearState 16
#define airportState 17
#define networkState 18
#define waterTowerState 19
#define pipeState 20
#define hospitalState 21
#define landfillState 22
#define connectState 23
#define garageState 24
#define stationState 25

#define firstState residentialState
#define lastState stationState

/* Global Bounds and Validity Macros */
#define ABS(x) (((x) < 0) ? (-(x)) : (x))
#define TestBounds(x, y) (((x) >= 0) && ((x) < WORLD_X) && ((y) >= 0) && ((y) < WORLD_Y))

#define TILE_IS_NUCLEAR(tile) (((tile) & LOMASK) == NUCLEAR)
#define TILE_IS_POWER_PLANT(tile) ((((tile) & LOMASK) >= COALBASE && ((tile) & LOMASK) <= LASTPOWERPLANT) || (((tile) & LOMASK) >= NUCLEARBASE && ((tile) & LOMASK) <= LASTZONE))
#define TILE_IS_WATER_TOWER(tile) ((((tile) & LOMASK) >= WATERTOWER) && (((tile) & LOMASK) < WATERTOWER + WATERTOWER_TILES))
#define TILE_IS_LANDFILL(tile) ((((tile) & LOMASK) >= LANDFILL) && (((tile) & LOMASK) < LANDFILL + LANDFILL_TILES))
#define TILE_IS_GARAGE(tile) ((((tile) & LOMASK) >= GARAGE) && (((tile) & LOMASK) < GARAGE + GARAGE_TILES))
#define TILE_IS_STATION(tile) ((((tile) & LOMASK) >= STATION) && (((tile) & LOMASK) < STATION + STATION_TILES))
#define TILE_IS_TRAFFIC_BLOCK(tile) (TILE_IS_GARAGE(tile) || TILE_IS_STATION(tile))
#define TILE_IS_PARK(tile) ((((tile) & LOMASK) >= FOUNTAIN) && (((tile) & LOMASK) < FOUNTAIN + FOUNTAIN_FRAMES))
#define TILE_IS_VULNERABLE(tile) (!((tile) & ZONEBIT) && (((tile) & LOMASK) >= RZB) && (((tile) & LOMASK) <= LASTZONE))
#define TILE_IS_ARSONABLE(tile) (!((tile) & ZONEBIT) && (((tile) & LOMASK) >= RZB) && (((tile) & LOMASK) <= LASTZONE))
#define TILE_IS_RIVER_EDGE(tile) ((((tile) & LOMASK) >= FIRSTRIVEDGE) && (((tile) & LOMASK) <= LASTRIVEDGE))
#define TILE_IS_FLOODABLE(tile) (((tile) == DIRT) || (((tile) & BULLBIT) && ((tile) & BURNBIT)))
#define TILE_IS_RUBBLE(tile) ((((tile) & LOMASK) >= RUBBLE) && (((tile) & LOMASK) <= LASTRUBBLE))

/* Core Engine Types */
typedef uint16_t SimTile;

typedef struct SimSprite {
    struct SimSprite *next;
    char *name;
    int32_t type;
    int32_t frame;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t x_offset;
    int32_t y_offset;
    int32_t x_hot;
    int32_t y_hot;
    int32_t orig_x;
    int32_t orig_y;
    int32_t dest_x;
    int32_t dest_y;
    int32_t count;
    int32_t sound_count;
    int32_t dir;
    int32_t new_dir;
    int32_t step;
    int32_t flag;
    int32_t control;
    int32_t turn;
    int32_t accel;
    int32_t speed;
} SimSprite;

/* Decoupled UI and Audio Callbacks */
typedef struct SimCallbacks {
    void (*update_funds)(int32_t funds);
    void (*update_date)(int32_t year, int32_t month);
    void (*show_message)(int16_t msg_num, int16_t x, int16_t y);
    void (*auto_goto)(int16_t x, int16_t y, const char *msg);
    void (*invalidate_maps)(void);
    void (*invalidate_editors)(void);
    void (*play_sound)(int16_t sound_id, int16_t x, int16_t y);
    void (*make_sound)(const char *channel, const char *sound_name);
    void (*request_budget_dialog)(void);
    void (*news_event)(int16_t event_id);
    void (*news_reset)(void);
} SimCallbacks;

/* Newspaper-only event ids for treasury changes. They are never shown in the message box. */
#define NEWS_EVENT_TREASURY_EMPTY   101
#define NEWS_EVENT_TREASURY_MILLION 102
#define NEWS_EVENT_CRIME_LOW        103
#define NEWS_EVENT_POLLUTION_LOW    104
#define NEWS_EVENT_HEALTH_SHORT     105
#define NEWS_EVENT_TREASURY_BUSTED  106
#define NEWS_EVENT_GARBAGE_PILE     107
#define NEWS_EVENT_LANDFILL_FULL    108

/* Newspaper-only ids for advisories. Each mirrors the ticker message with the same meaning. */
#define NEWS_EVENT_ADV_RESIDENTIAL  109
#define NEWS_EVENT_ADV_COMMERCIAL   110
#define NEWS_EVENT_ADV_INDUSTRIAL   111
#define NEWS_EVENT_ADV_ROADS        112
#define NEWS_EVENT_ADV_RAIL         113
#define NEWS_EVENT_ADV_POWER        114
#define NEWS_EVENT_ADV_STADIUM      115
#define NEWS_EVENT_ADV_SEAPORT      116
#define NEWS_EVENT_ADV_AIRPORT      117
#define NEWS_EVENT_ADV_FIRE_DEPT    118
#define NEWS_EVENT_ADV_POLICE_DEPT  119
#define NEWS_EVENT_ADV_BLACKOUT     120
#define NEWS_EVENT_ADV_TAX          121
#define NEWS_EVENT_ADV_ROAD_DECAY   122
#define NEWS_EVENT_ADV_FIRE_FUNDING 123
#define NEWS_EVENT_ADV_POLICE_FUNDING 124
#define NEWS_EVENT_ADV_UNEMPLOYMENT 125
#define NEWS_EVENT_ADV_PARKS        126
#define NEWS_EVENT_ADV_RIOTS        127
#define NEWS_EVENT_ADV_HOSPITAL     128
#define NEWS_EVENT_ADV_PARKS_FUNDING 129
#define NEWS_EVENT_ADV_PORT_CAPACITY 130

/* Newspaper-only: a job surplus has no ticker counterpart. */
#define NEWS_EVENT_JOBS_BOOM        131

/* Engine Global Variables */
extern uint16_t *Map[WORLD_X];
extern Byte *PopDensity[HWLDX];
extern Byte *TrfDensity[HWLDX];
extern Byte *PollutionMem[HWLDX];
extern Byte *LandValueMem[HWLDX];

typedef struct {
    int32_t centre;
    int32_t terrain;
    int32_t pollution;
    int32_t crime;
    int32_t value;
    bool zoned;
    bool access;
} LandValueFactors;
void LandValueFactorsOf(int32_t x, int32_t y, LandValueFactors *f);
bool ZoneHasRoadAccess(int32_t x, int32_t y);
bool IsGrowingZone(int32_t low);
bool BuildingAnchorAt(int32_t x, int32_t y, int32_t *ox, int32_t *oy, int32_t *size);
bool BuildingHasRoadAccess(int32_t ox, int32_t oy, int32_t size);
bool BuildingHasRailAccess(int32_t ox, int32_t oy, int32_t size);
bool CapacityBlockHasAccess(int32_t x, int32_t y);
bool CapacityBlockPowered(int32_t x, int32_t y);
bool CapacityBlockWatered(int32_t x, int32_t y);
int32_t LandValueWithNeighbors(int32_t value);
extern Byte *CrimeMem[HWLDX];
extern Byte *tem[HWLDX];
extern Byte *tem2[HWLDX];
extern Byte *TerrainMem[QWX];
extern Byte *Qtem[QWX];

extern int16_t RateOGMem[SmX][SmY];
extern int16_t HospPop, ChurchPop, StadiumPop;
extern int16_t ComRate[SmX][SmY];
extern int16_t STem[SmX][SmY];

extern int16_t SMapX, SMapY;
extern int16_t CChr, CChr9;
extern int16_t RoadTotal, RailTotal, FirePop;
extern int32_t ResPop;
extern int16_t ComPop, IndPop, TotalPop, LastTotalPop;
extern int16_t ResZPop, ComZPop, IndZPop, TotalZPop;
extern int16_t HospPop, ChurchPop, StadiumPop;
extern int16_t PolicePop, FireStPop;
extern int16_t CoalPop, NuclearPop, PortPop, APortPop;
extern int16_t NeedChurch;
extern int16_t CrimeAverage, PolluteAverage, LVAverage;
extern int16_t CrimeRamp, PolluteRamp, AvCityTax, TaxRateLastYear;
extern float EMarket;

extern char *MicropolisVersion;
extern char CityName[64];
extern char *CityFileName;
extern int16_t StartingYear;
extern int32_t CityTime;
extern int16_t GameLevel;
extern int16_t Cycle;
extern int16_t ShakeNow;

extern int16_t *ResHis;
extern int16_t *ComHis;
extern int16_t *IndHis;
extern int16_t *MoneyHis, *CrimeHis, *PollutionHis, *MiscHis;
extern uint16_t *PowerMap;

extern float roadPercent, policePercent, firePercent, healthPercent, landfillPercent, recreationPercent;
extern int32_t LandfillFund, LandfillSpend, TrashPending, UnservicedTrash;
extern int16_t LandfillPop;
extern bool LandfillAtCapacity;
extern int32_t RoadSpend, PoliceSpend, FireSpend, HealthSpend, RecSpend;
extern int32_t TaxFund, DealsIncome, RoadFund, PoliceFund, FireFund, HealthFund, RecFund;
extern int16_t RoadEffect, PoliceEffect, FireEffect, HealthEffect, RecEffect;
#define POLICE_STATION_COST 60
#define FIRE_STATION_COST   60
#define HOSPITAL_COST       80
#define PARK_COST           25
#define STADIUM_COST        150
#define LANDFILL_CENTRE_COST 20
#define GARAGE_COST          3000
#define STATION_COST         6000
#define GARAGE_ZONES         9
#define STATION_ZONES        18
#define TRAF_ROADS_PER_ZONE  10
#define PORT_COST            25
#define AIRPORT_COST         45
#define PORT_SERVICE_ZONES   128
#define AIRPORT_SERVICE_ZONES 256
void PortSystemStats(int32_t *capacity, int32_t *demand);
extern int16_t TaxFlag, ResTax, ComTax, IndTax;
extern int32_t ResTaxPop;
extern int32_t ResUncoveredPop;
extern int32_t ResUnrecPop;
extern int32_t TotalFunds;

extern int32_t CostOf[];
extern int16_t toolSize[];
extern int16_t toolOffset[];

extern int16_t flagBlink;
extern uint8_t tileSynch;
extern int16_t aniTile[];
extern uint8_t aniSynch[];
extern int32_t TilesAnimated;
extern int32_t DoAnimation;
extern int32_t DoMessages;

/* Saved in MiscHis[57], so new codes must be appended, never renumbered. */
#define SIM_SPEED_PAUSED 0
#define SIM_SPEED_SLOW 1
#define SIM_SPEED_MED 2
#define SIM_SPEED_FAST 3
#define SIM_SPEED_SUPER_FAST 4
#define SIM_SPEED_SUPER_SLOW 5
#define SIM_SPEED_MAX 5

extern int16_t SimSpeed;
extern int16_t NoDisasters;
extern int16_t autoBulldoze;
extern int16_t autoBudget;
extern int16_t autoGo;
extern int16_t UserSoundOn;

extern int16_t FloodCnt;
extern int16_t ResCap, ComCap, IndCap;
extern int16_t RValve, CValve, IValve;
extern int16_t PwrdZCnt, unPwrdZCnt, UnwateredZCnt;

extern int16_t CityScore, deltaCityScore;
extern int16_t CityClass;
extern int32_t PowerStackNum;
extern int16_t TrafficAverage;
extern int32_t absDist;

extern int16_t CCx, CCy, CCx2, CCy2;
extern int16_t TrafMaxX, TrafMaxY;
extern int32_t TrafServicedResPop;
extern int16_t CrimeMaxX, CrimeMaxY;
extern int16_t PolMaxX, PolMaxY;
extern int16_t CrashX, CrashY;
extern int32_t CityPop, deltaCityPop;
extern int16_t CityYes, CityNo;
extern int16_t ProblemTable[PROBNUM];
extern int16_t ProblemVotes[PROBNUM];
extern int16_t ProblemOrder[4];
extern int32_t CityAssValue;
extern int16_t ParkPop;

extern int16_t InitSimLoad;
extern int16_t DoInitialEval;
extern int32_t ValveFlag;
extern int32_t MustUpdateFunds;
extern int32_t MustUpdateOptions;
extern int32_t CensusChanged;
extern int32_t EvalChanged;
extern int16_t NewMap;
extern int16_t NewMapFlags[NMAPS];

extern int32_t OverRide;
extern int32_t PendingTool;

/* Message Routing State */
extern int16_t MessagePort;
extern int16_t MesNum;
extern int16_t MesX;
extern int16_t MesY;
extern int16_t HaveLastMessage;
extern char LastMessage[256];
/* Bumped whenever the ticker should show a message, even if its text repeats. */
extern uint32_t MessageSerial;
/* Tile the current ticker message refers to; (0,0) means none. */
extern int16_t MessageTargetX;
extern int16_t MessageTargetY;

extern SimSprite *GlobalSprites[OBJN];
extern SimCallbacks EngineCallbacks;

/* Explicit Function Prototypes */

/* RNG */
void SeedRand(int32_t seed);
void RandomlySeedRand(void);
int16_t Rand(int16_t range);
int16_t RandInt(void);
int16_t Rand16(void);
int16_t Rand16Signed(void);

/* Lifecycle and Simulation Loop */
void Sim_InitEngine(void);
void Sim_RegisterCallbacks(const SimCallbacks *cb);
void News_ResetSession(void);
void SimFrame(void);
void Simulate(int32_t mod16);
void SetSimSpeed(int16_t speed);
int16_t GetLastRunSpeed(void);
void SetGameLevel(int16_t level);
void SetNewCityLevel(int16_t level);
void SetMessageField(const char *str, int16_t x, int16_t y, bool is_event);
int32_t CalcRoadFund(int16_t level);
void InitWillStuff(void);
void ResetCityWorkingState(void);
void ResetMapState(void);
void ResetEditorState(void);
void AnimateTiles(void);
void ClearCensus(void);
void SimRestartCensusCycle(void);
void ResetBudgetForNewCity(void);

/* Map Generation */
void GenerateNewCity(void);
void GenerateSomeCity(int32_t seed);
void GenerateMap(int32_t seed);
void ClearMap(void);
void ClearUnnatural(void);
void MakeNakedIsland(void);
void SmoothWater(void);

/* Zones, Power, and Automata */
void DoZone(void);
void SeedServicedResPop(void);
void SeedZoneCensus(void);
int16_t CountZoneLoad(void);
void CountWaterTowerPower(void);
void CountParkWater(void);
int16_t SetZPower(void);
void DoSPZone(int16_t pwron);
void DoNilPower(void);
void DoPowerScan(void);
bool PowerGridStats(int32_t x, int32_t y, int32_t *demand, int32_t *capacity);

/* Water supply (s_water.c) */
void DoWaterScan(void);
bool WaterIsSupplied(int32_t x, int32_t y);
bool WaterHasPipe(int32_t x, int32_t y);
bool WaterCanConnect(int32_t x, int32_t y);
bool WaterPlantSupplied(int32_t x, int32_t y);
bool WaterNetworkStats(int32_t x, int32_t y, int32_t *demand, int32_t *capacity);
bool WaterTowerHasSource(int32_t x, int32_t y);
int32_t LayPipe(int32_t x, int32_t y);
int32_t LayWaterTower(int32_t x, int32_t y);
void Water_ClearPipes(void);
void Water_RemovePipe(int32_t x, int32_t y);
size_t Water_PipeBlockBytes(void);
void Water_CopyPipes(uint8_t *dst);
void Water_LoadPipes(const uint8_t *src);
bool MoveMapSim(int16_t mdir);
void PushPowerStack(void);
void SetSmoke(int32_t zone_power);
void RepairZone(int16_t zone_id, int16_t size);
void ZonePlop(int16_t base_tile);
void PlantPlop(int16_t base_tile, int32_t size);
bool RoadTest(int32_t tile);
void DoLandfill(void);
void LandfillBlockPlaced(int32_t x, int32_t y);
void LandfillResetCity(void);
void LandfillRescan(void);
#define LANDFILL_FILL_BYTES (WORLD_X * WORLD_Y * 2)
void LandfillEncodeFill(uint8_t *dst);
void LandfillDecodeFill(const uint8_t *src);
int32_t LandfillTrashPenalty(void);
void LandfillSystemStats(int32_t *capacity, int32_t *demand);
int32_t LandfillPollution(int32_t loc);

/* Neighbor cities (s_neighbor.c): one per map side, linked by Connect gates */
enum { NB_NORTH, NB_EAST, NB_SOUTH, NB_WEST, NB_SIDES };
enum { NB_LINK_ROAD, NB_LINK_RAIL, NB_LINK_POWER, NB_LINK_WATER, NB_LINK_COUNT };
enum { NB_GOODS_POWER, NB_GOODS_WATER, NB_GOODS_GARBAGE, NB_GOODS_COUNT };
void Neighbor_Reset(void);
void Neighbor_Tick(void);
int32_t Neighbor_TakeDealsFlow(void);
void Neighbor_SyncMisc(void);
void Neighbor_LoadMisc(void);
const char *Neighbor_Name(int32_t side);
bool Neighbor_IsLinked(int32_t side, int32_t link);
bool Neighbor_BuildGate(int32_t side, int32_t link, int32_t x, int32_t y);
void Neighbor_RemoveGate(int32_t side, int32_t link);
void Neighbor_PruneGates(void);
int32_t Neighbor_Units(int32_t side, int32_t goods);
int32_t Neighbor_Cap(int32_t side, int32_t goods);
int32_t Neighbor_MidPrice(int32_t side, int32_t goods);
int32_t Neighbor_BuyCost(int32_t side, int32_t goods);
int32_t Neighbor_SellPrice(int32_t side, int32_t goods);
void Neighbor_SetUnits(int32_t side, int32_t goods, int32_t units);
bool Neighbor_FeedSeed(int32_t side, int32_t goods, int32_t *x, int32_t *y);
int32_t Neighbor_FeedZones(int32_t side, int32_t goods);
bool Neighbor_GateTile(int32_t side, int32_t goods, int32_t *x, int32_t *y);
int32_t Neighbor_BuyCharge(int32_t side, int32_t goods, int32_t zones);
int32_t Neighbor_SellEarn(int32_t side, int32_t goods, int32_t zones);
void Neighbor_ClearSales(int32_t goods);
int32_t Neighbor_OfferSale(int32_t side, int32_t goods, int32_t spareZones);
int32_t Neighbor_SafeSellUnits(int32_t side, int32_t goods);
int32_t Neighbor_SoldUnits(int32_t side, int32_t goods);
bool Neighbor_SaleSettled(int32_t side, int32_t goods);
void Neighbor_BlackoutSale(int32_t side, int32_t goods);
bool Neighbor_SaleBlackedOut(int32_t side, int32_t goods);
int32_t Neighbor_GarbageFeedZones(void);
bool Neighbor_RailTile(int32_t side, int32_t *x, int32_t *y);
SimSprite *GetNeighborTrainSlot(int32_t side, int32_t index);
int32_t Neighbor_LandValueBonusPercent(void);
bool Neighbor_Connect(int32_t x, int32_t y);
void FireZone(int32_t xloc, int32_t yloc, int32_t ch);
void DoMeltdown(int32_t sx, int32_t sy);
bool PlantCaughtFire(int32_t x, int32_t y);

/* Zone Population Helpers */
int16_t RZPop(int32_t ch9);
int16_t CZPop(int32_t ch9);
int16_t IZPop(int32_t ch9);
int16_t DoFreePop(void);

/* Presentation & Sprite Subsystems */
void InitSprites(void);
void DispatchTrains(void);
void GenerateShip(void);
void GeneratePlane(int16_t x, int16_t y);
void GenerateCopter(int16_t x, int16_t y);
void MakeExplosionAt(int32_t x, int32_t y);
SimSprite *GetSprite(int32_t type);
SimSprite *GetSpriteSlot(int32_t type, int32_t index);
void MoveObjects(void);

/* Scans & Overlays */
void FireAnalysis(void);
void HealthAnalysis(void);
void SmoothServiceMap(int16_t map[SmX][SmY]);
bool HealthIsCovered(int32_t x, int32_t y);
void HospitalSystemStats(int32_t *capacity, int32_t *demand);
void RecAnalysis(void);
bool RecIsCovered(int32_t x, int32_t y);
void RecSystemStats(int32_t *capacity, int32_t *demand);
int16_t HealthCoverageLevel(int32_t x, int32_t y);
int32_t HealthAdjustedPop(int32_t pop, int32_t x, int32_t y);
void PopDenScan(void);
void PTLScan(void);

#define POLICE_ZONE_NONE 0
#define POLICE_ZONE_COVERED 1
#define POLICE_ZONE_UNCOVERED 2
void PoliceAnalysis(void);
int32_t PoliceZoneState(int32_t x, int32_t y);
bool PoliceCoversCrimeCell(int32_t hx, int32_t hy);
void PoliceSystemStats(int32_t *capacity, int32_t *demand);
void CrimeScan(void);

#define FIRE_ZONE_NONE 0
#define FIRE_ZONE_COVERED 1
#define FIRE_ZONE_UNCOVERED 2
void FireCoverageAnalysis(void);
int32_t FireZoneState(int32_t x, int32_t y);
int16_t FireBurnRate(int32_t x, int32_t y);
void FireSystemStats(int32_t *capacity, int32_t *demand);

/* Traffic and Transportation: road capacity model, see s_traf.c */
int32_t TrafRushHourCount(void);
void TrafRushHour(void);
void TrafSeedRushState(void);
enum { TRAF_LIGHT, TRAF_NORMAL, TRAF_CONGESTED, TRAF_OVERLOADED, TRAF_STATE_COUNT };
int32_t TrafState(void);
bool TrafCommuting(void);
void TrafResetCity(void);
bool TrafZoneUnserviced(int32_t x, int32_t y);
int32_t TrafProblemShare(void);
bool TrafAtCapacity(void);
bool TrafGridlocked(void);
int32_t TrafRoadPollution(void);
int32_t TrafUncongestedPollution(void);
bool IsTrafficRoadTile(uint16_t id);
bool TrafAnimTileDue(int32_t x, int32_t y);
int32_t TrafRoadDensity(int32_t x, int32_t y);
void TrafAnimBeginPass(void);
int16_t TrafRoadLoad(void);
void TrafNetworkStats(int32_t *demand, int32_t *capacity);
float TrafServiceShare(void);

/* Network Auto-Tiling & Layout Operations */
void FixZone(int32_t x, int32_t y);
int32_t LayRoad(int32_t x, int32_t y);
int32_t LayRail(int32_t x, int32_t y);
int32_t LayWire(int32_t x, int32_t y);
int32_t LayDoze(int32_t x, int32_t y);
int32_t LayPark(int32_t x, int32_t y);
bool CheckZoneClear(int32_t x, int32_t y, int16_t size);
bool AutoDozeCanClear(uint16_t low);
int32_t PlaceZone(int32_t x, int32_t y, int16_t tool_id);

/* Budget and Evaluation */
void CollectTax(void);
int16_t GetAverageTaxRate(void);
int32_t CalcTaxFund(void);
void DoBudget(void);
void ApplyBudgetCommit(void);
void UpdateFundEffects(void);
void CityEvaluation(void);
void CityEvaluationAfterLoad(void);
void SeedZoningCaps(void);
void EvalInit(void);
void GetAssValue(void);

/* Disasters and Incidents */
void DoDisasters(void);
void SetFire(void);
void MakeFire(void);
void MakeFlood(void);
void DoFlood(void);
void MakeTornado(void);
void MakeEarthquake(void);
void MakeMonster(void);
int16_t MonsterSpeed(void);
void MakeMeltdown(void);
void FireBomb(void);
void MakeExplosion(int32_t x, int32_t y);
/* kind is a ScenarioDisaster value from scenario.h. Fires regardless of the random-disasters setting. */
void TriggerDisaster(int32_t kind);

/* Typed cheat codes (fund, fart, nuke) */
void Cheat_Reset(void);
void Cheat_KeyPressed(char key);

/* Messaging & Game Outcome */
void SendMessages(void);
int32_t SendMes(int16_t msg_num);
void SendMesAt(int16_t msg_num, int16_t x, int16_t y);
void SendMesTickerAt(int16_t msg_num, int16_t x, int16_t y);
void NewsThrottled(int16_t event_id);
void doMessage(void);
void ClearMes(void);
void ResetMilestoneState(void);
void DoLoseGame(void);
void DoWinGame(void);

/* File IO & Scenarios */
/* Directory holding scenarios/ and cities/ (the executable's directory). */
void SetDataRoot(const char *root);
int32_t LoadCity(const char *filename);
int32_t SaveCity(const char *filename);
/* Recounts infrastructure and reruns the power, crime, pop-density, and fire scans after the map changes. */
void CityRescan(void);
/* Runs one full month of simulation (16 Simulate steps) even while paused. */
void SimAdvanceOneMonth(void);

/* Terrain generation levels (< 0 selects the random default; see GenerateMap) */
extern int32_t TreeLevel;
extern int32_t LakeLevel;
extern int32_t CurveLevel;
extern int32_t CreateIsland;

/* Memory Allocation */
void AllocateMapBuffers(void);
void FreeMapBuffers(void);

#endif /* SIM_H */