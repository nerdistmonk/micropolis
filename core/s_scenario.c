/* s_scenario.c: Custom scenario rules, year-boundary win/lose checks, and the scenario editor state.
 *
 * File I/O for .scn files lives in s_fileio.c.
 */

#include "sim.h"
#include "scenario.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCN_MAX_CITY_CLASS  5
#define SCN_MAX_CITY_SCORE  1000
#define SCN_YEAR_TICKS      48   /* CityTime ticks per calendar year */
#define SCN_MONTH_TICKS     (SCN_YEAR_TICKS / 12)

static ScenarioDef active_def;
static int32_t scenario_active = 0;
static int32_t scenario_resolved = 0;
static int32_t scenario_last_check = 0;
static uint32_t scenario_events_fired = 0;

static ScenarioDef editor_def;
static int32_t editor_active = 0;
static int16_t editor_resume_speed = SIM_SPEED_FAST;
static int16_t editor_resume_no_disasters = 0;

static int32_t StringTerminated(const char *s, size_t bytes)
{
    return memchr(s, '\0', bytes) != NULL;
}

static int32_t NameIsBlank(const char *s)
{
    for (const char *p = s; *p; p++) {
        if (*p != ' ' && *p != '\t') return 0;
    }
    return 1;
}

static int32_t ConditionMet(const ScenarioCondition *c)
{
    int32_t v = Scenario_MetricValue(c->metric);
    return (c->op == SCN_OP_AT_LEAST) ? (v >= c->value) : (v <= c->value);
}

/* An empty list never holds. */
static int32_t ListMet(const ScenarioCondition *list, int32_t count, int32_t all)
{
    if (count <= 0) return 0;

    for (int32_t i = 0; i < count; i++) {
        int32_t met = ConditionMet(&list[i]);
        if (all && !met) return 0;
        if (!all && met) return 1;
    }
    return all ? 1 : 0;
}

static const char *ValidateCondition(const ScenarioCondition *c)
{
    if (c->metric >= SCN_METRIC_COUNT) return "Unknown metric in a condition.";
    if (c->op >= SCN_OP_COUNT) return "Unknown operator in a condition.";
    if (c->value < 0 || c->value > SCN_MAX_VALUE) return "A condition value is out of range.";
    if (c->metric == SCN_METRIC_CITY_CLASS && c->value > SCN_MAX_CITY_CLASS) {
        return "City class must be between 0 and 5.";
    }
    if (c->metric == SCN_METRIC_CITY_SCORE && c->value > SCN_MAX_CITY_SCORE) {
        return "City score must be between 0 and 1000.";
    }
    return NULL;
}

void Scenario_SetDefaults(ScenarioDef *def)
{
    memset(def, 0, sizeof(*def));
    def->start_year = SCN_FIRST_YEAR;
    def->funds = 20000;
    def->disasters = 1;
}

const char *Scenario_Validate(const ScenarioDef *def)
{
    if (!def) return "No scenario is open.";

    if (!StringTerminated(def->name, SCN_NAME_BYTES) ||
        !StringTerminated(def->description, SCN_DESC_BYTES)) {
        return "Scenario text is not terminated.";
    }
    if (NameIsBlank(def->name)) return "Enter a scenario name.";

    if (def->start_year < SCN_FIRST_YEAR || def->start_year > SCN_LAST_YEAR) {
        return "Start year must be between 1900 and 2100.";
    }
    if (def->funds < 0 || def->funds > SCN_MAX_VALUE) {
        return "Starting funds must be between 0 and 999999999.";
    }
    if (def->deadline_year != 0 &&
        (def->deadline_year <= def->start_year || def->deadline_year > SCN_LAST_YEAR)) {
        return "Deadline must be after the start year, or 0 for none.";
    }
    if (def->disasters > 1 || def->win_all > 1 || def->lose_all > 1) {
        return "Scenario flags are invalid.";
    }

    if (def->win_count < 1) return "Add at least one win condition.";
    if (def->win_count > SCN_MAX_CONDITIONS || def->lose_count > SCN_MAX_CONDITIONS) {
        return "Too many conditions in one list.";
    }
    if (def->event_count > SCN_MAX_EVENTS) return "Too many disaster events.";

    for (int32_t i = 0; i < def->event_count; i++) {
        const ScenarioEvent *e = &def->events[i];
        if (e->type >= SCN_DISASTER_COUNT) return "Unknown disaster type in an event.";
        if (e->month < 1 || e->month > 12) return "Event month must be between 1 and 12.";
        if (e->year < def->start_year || e->year > SCN_LAST_YEAR) {
            return "Event year must be from the start year to 2100.";
        }
        if (def->deadline_year != 0 && e->year >= def->deadline_year) {
            return "Disaster events must happen before the deadline year.";
        }
    }

    for (int32_t i = 0; i < def->win_count; i++) {
        const char *err = ValidateCondition(&def->win[i]);
        if (err) return err;
    }
    for (int32_t i = 0; i < def->lose_count; i++) {
        const char *err = ValidateCondition(&def->lose[i]);
        if (err) return err;
    }

    return NULL;
}

const char *Scenario_MetricName(int32_t metric)
{
    switch (metric) {
    case SCN_METRIC_CITY_CLASS: return "City Class";
    case SCN_METRIC_CITY_SCORE: return "City Score";
    case SCN_METRIC_POPULATION: return "Population";
    case SCN_METRIC_TRAFFIC:    return "Traffic";
    case SCN_METRIC_CRIME:      return "Crime";
    case SCN_METRIC_POLLUTION:  return "Pollution";
    case SCN_METRIC_LAND_VALUE: return "Land Value";
    case SCN_METRIC_FUNDS:      return "Funds";
    default:                    return "?";
    }
}

const char *Scenario_OpName(int32_t op)
{
    switch (op) {
    case SCN_OP_AT_LEAST: return ">=";
    case SCN_OP_AT_MOST:  return "<=";
    default:              return "?";
    }
}

const char *Scenario_DisasterName(int32_t type)
{
    switch (type) {
    case SCN_DISASTER_FIRE:       return "Fire";
    case SCN_DISASTER_FLOOD:      return "Flood";
    case SCN_DISASTER_TORNADO:    return "Tornado";
    case SCN_DISASTER_EARTHQUAKE: return "Earthquake";
    case SCN_DISASTER_MONSTER:    return "Monster";
    case SCN_DISASTER_MELTDOWN:   return "Meltdown";
    case SCN_DISASTER_FIRE_BOMBS: return "Fire bombs";
    default:                      return "?";
    }
}

int32_t Scenario_MetricValue(int32_t metric)
{
    switch (metric) {
    case SCN_METRIC_CITY_CLASS: return CityClass;
    case SCN_METRIC_CITY_SCORE: return CityScore;
    case SCN_METRIC_POPULATION: return CityPop < 0 ? 0 : CityPop;
    case SCN_METRIC_TRAFFIC:    return TrafficAverage;
    case SCN_METRIC_CRIME:      return CrimeAverage;
    case SCN_METRIC_POLLUTION:  return PolluteAverage;
    case SCN_METRIC_LAND_VALUE: return LVAverage;
    case SCN_METRIC_FUNDS:      return TotalFunds;
    default:                    return 0;
    }
}

int32_t Scenario_StartTicks(int32_t year)
{
    return (year - SCN_FIRST_YEAR) * SCN_YEAR_TICKS + 2;
}

static int32_t EventTicks(const ScenarioEvent *e)
{
    return (e->year - SCN_FIRST_YEAR) * SCN_YEAR_TICKS + (e->month - 1) * SCN_MONTH_TICKS;
}

void Scenario_Activate(const ScenarioDef *def)
{
    active_def = *def;
    scenario_active = 1;
    scenario_resolved = 0;
    scenario_events_fired = 0;
    /* First check fires at the next year boundary, not immediately. */
    scenario_last_check = CityTime;
}

void Scenario_Deactivate(void)
{
    scenario_active = 0;
    scenario_resolved = 0;
}

void Scenario_CheckEvents(void)
{
    if (!scenario_active || scenario_resolved || editor_active) return;

    for (int32_t i = 0; i < active_def.event_count; i++) {
        uint32_t bit = 1u << i;
        if (scenario_events_fired & bit) continue;
        if (CityTime < EventTicks(&active_def.events[i])) continue;

        scenario_events_fired |= bit;
        TriggerDisaster(active_def.events[i].type);
    }
}

/* Called from SendMessages. Win is checked before lose, and the deadline is checked last. */
void Scenario_CheckYear(void)
{
    if (!scenario_active || scenario_resolved || editor_active) return;
    if (CityTime % SCN_YEAR_TICKS != 0 || CityTime == scenario_last_check) return;

    scenario_last_check = CityTime;
    int32_t year = SCN_FIRST_YEAR + CityTime / SCN_YEAR_TICKS;

    if (ListMet(active_def.win, active_def.win_count, active_def.win_all)) {
        scenario_resolved = 1;
        DoWinGame();
        return;
    }

    if (ListMet(active_def.lose, active_def.lose_count, active_def.lose_all)) {
        scenario_resolved = 1;
        DoLoseGame();
        return;
    }

    if (active_def.deadline_year != 0 && year >= active_def.deadline_year) {
        scenario_resolved = 1;
        DoLoseGame();
    }
}

int32_t Scenario_EditorIsActive(void)
{
    return editor_active;
}

static void NotifyEngineChanged(void)
{
    if (EngineCallbacks.update_funds) EngineCallbacks.update_funds(TotalFunds);
    if (EngineCallbacks.invalidate_maps) EngineCallbacks.invalidate_maps();
    if (EngineCallbacks.invalidate_editors) EngineCallbacks.invalidate_editors();
}

void Scenario_EditorBegin(void)
{
    if (editor_active) return;

    Scenario_Deactivate();
    Scenario_SetDefaults(&editor_def);

    editor_resume_speed = SimSpeed;
    editor_resume_no_disasters = NoDisasters;
    editor_active = 1;

    ClearMap();
    ResetCityWorkingState();
    ResetMilestoneState();
    ResetGraphHistory();

    free(CityFileName);
    CityFileName = NULL;
    snprintf(CityName, sizeof(CityName), "%s", "Untitled");

    SetGameLevel(0);
    CityTime = Scenario_StartTicks(editor_def.start_year);
    TotalFunds = editor_def.funds;
    NoDisasters = editor_def.disasters ? 0 : 1;

    ResTax = ComTax = IndTax = 7;
    roadPercent = 1.0f;
    policePercent = 1.0f;
    firePercent = 1.0f;
    healthPercent = 1.0f;
    recreationPercent = 1.0f;
    LandfillResetCity();
    RoadEffect = 32;
    PoliceEffect = 1000;
    FireEffect = 1000;
    HealthEffect = 1000;
    RecEffect = 1000;

    InitSimLoad = 1;
    DoInitialEval = 0;
    InitSprites();
    CityRescan();
    ResetBudgetForNewCity();

    SetSimSpeed(SIM_SPEED_PAUSED);
    NotifyEngineChanged();
}

void Scenario_EditorEnd(void)
{
    if (!editor_active) return;

    editor_active = 0;
    NoDisasters = editor_resume_no_disasters;
    SetSimSpeed(editor_resume_speed);
    NotifyEngineChanged();
}

ScenarioDef *Scenario_EditorDef(void)
{
    return editor_active ? &editor_def : NULL;
}

void Scenario_EditorSetStartYear(int32_t year)
{
    if (!editor_active) return;
    if (year < SCN_FIRST_YEAR || year > SCN_LAST_YEAR) return;

    editor_def.start_year = year;
    CityTime = Scenario_StartTicks(year);
    NotifyEngineChanged();
}

void Scenario_EditorSetFunds(int32_t funds)
{
    if (!editor_active) return;
    if (funds < 0 || funds > SCN_MAX_VALUE) return;

    editor_def.funds = funds;
    TotalFunds = funds;
    NotifyEngineChanged();
}

/* Regenerates terrain with the current tree/lake/river/island settings. Clears buildings and zones.
 * Funds, the scenario rules, and the paused state are kept. */
void Scenario_EditorRandomTerrain(int32_t seed)
{
    if (!editor_active) return;

    GenerateMap(seed);
    ResetCityWorkingState();
    InitSprites();
    CityRescan();
    NotifyEngineChanged();
}

void Scenario_EditorPaint(int32_t brush, int32_t tx, int32_t ty)
{
    if (!editor_active) return;
    if (tx < 0 || tx >= WORLD_X || ty < 0 || ty >= WORLD_Y) return;

    uint16_t tile;
    switch (brush) {
    case editorWaterState:  tile = RIVER;  break;
    case editorLandState:   tile = DIRT;   break;
    case editorForestState: tile = WOODS;  break;
    default: return;
    }

    if (Map[tx][ty] == tile) return;
    Map[tx][ty] = tile;
    CityRescan();
    if (EngineCallbacks.invalidate_maps) EngineCallbacks.invalidate_maps();
}

void Scenario_EditorStep(void)
{
    if (!editor_active || SimSpeed != SIM_SPEED_PAUSED) return;

    SimAdvanceOneMonth();
    NotifyEngineChanged();
}

const char *Scenario_EditorSave(char *saved_path, size_t saved_len)
{
    if (!editor_active) return "The scenario editor is not open.";

    const char *err = Scenario_Validate(&editor_def);
    if (err) return err;

    if (!Scenario_SaveToDirectory(&editor_def, saved_path, saved_len)) {
        return "Could not write the scenario file.";
    }
    return NULL;
}
