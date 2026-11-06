/* scenario.h: Custom scenario definitions, win/lose evaluation, and the scenario editor model.
 *
 * Custom scenarios are stored as .scn files (see s_fileio.c for the on-disk layout).
 * Historical snro.* scenarios still use LoadScenario() and are not handled here.
 */

#ifndef SCENARIO_H
#define SCENARIO_H

#include <stddef.h>
#include <stdint.h>

#define SCN_NAME_BYTES      32   /* includes the terminating NUL */
#define SCN_DESC_BYTES      128  /* includes the terminating NUL */
#define SCN_MAX_CONDITIONS  8    /* per win or lose list */
#define SCN_FIRST_YEAR      1900
#define SCN_LAST_YEAR       2100
#define SCN_MAX_VALUE       999999999  /* largest funds and condition value */
#define SCN_MAX_EVENTS      8    /* scheduled disasters per scenario */

typedef enum ScenarioDisaster {
    SCN_DISASTER_FIRE = 0,
    SCN_DISASTER_FLOOD,
    SCN_DISASTER_TORNADO,
    SCN_DISASTER_EARTHQUAKE,
    SCN_DISASTER_MONSTER,
    SCN_DISASTER_MELTDOWN,
    SCN_DISASTER_FIRE_BOMBS,
    SCN_DISASTER_COUNT
} ScenarioDisaster;

typedef enum ScenarioMetric {
    SCN_METRIC_CITY_CLASS = 0,
    SCN_METRIC_CITY_SCORE,
    SCN_METRIC_POPULATION,
    SCN_METRIC_TRAFFIC,
    SCN_METRIC_CRIME,
    SCN_METRIC_POLLUTION,
    SCN_METRIC_LAND_VALUE,
    SCN_METRIC_FUNDS,
    SCN_METRIC_COUNT
} ScenarioMetric;

typedef enum ScenarioOp {
    SCN_OP_AT_LEAST = 0,
    SCN_OP_AT_MOST,
    SCN_OP_COUNT
} ScenarioOp;

typedef struct ScenarioCondition {
    uint8_t metric;   /* ScenarioMetric */
    uint8_t op;       /* ScenarioOp */
    int32_t value;
} ScenarioCondition;

typedef struct ScenarioEvent {
    uint8_t type;     /* ScenarioDisaster */
    uint8_t month;    /* 1-12 */
    int32_t year;
} ScenarioEvent;

typedef struct ScenarioDef {
    char name[SCN_NAME_BYTES];
    char description[SCN_DESC_BYTES];
    int32_t start_year;
    int32_t funds;
    int32_t deadline_year;   /* 0 means no deadline */
    uint8_t disasters;       /* nonzero: random disasters enabled */
    uint8_t win_all;         /* nonzero: every win condition must hold, otherwise any */
    uint8_t lose_all;        /* nonzero: every lose condition must hold, otherwise any */
    uint8_t win_count;
    uint8_t lose_count;
    uint8_t event_count;
    ScenarioCondition win[SCN_MAX_CONDITIONS];
    ScenarioCondition lose[SCN_MAX_CONDITIONS];
    ScenarioEvent events[SCN_MAX_EVENTS];
} ScenarioDef;

/* Definition helpers */
void Scenario_SetDefaults(ScenarioDef *def);
/* Returns NULL when valid, otherwise a short message for the user. */
const char *Scenario_Validate(const ScenarioDef *def);
const char *Scenario_MetricName(int32_t metric);
const char *Scenario_OpName(int32_t op);
const char *Scenario_DisasterName(int32_t type);
int32_t Scenario_MetricValue(int32_t metric);

/* Active scenario: checked once per in-game year while a custom scenario is running. */
/* CityTime value for the first tick of a year (matches the historical scenarios' +2 offset). */
int32_t Scenario_StartTicks(int32_t year);
void Scenario_Activate(const ScenarioDef *def);
void Scenario_Deactivate(void);
void Scenario_CheckYear(void);
/* Fires each scheduled disaster once when its month is reached. Called every sim tick. */
void Scenario_CheckEvents(void);

/* Scenario editor (paused sandbox that edits a ScenarioDef and the map). */
int32_t Scenario_EditorIsActive(void);
void Scenario_EditorBegin(void);
void Scenario_EditorEnd(void);
ScenarioDef *Scenario_EditorDef(void);
void Scenario_EditorSetStartYear(int32_t year);
void Scenario_EditorSetFunds(int32_t funds);
void Scenario_EditorPaint(int32_t brush, int32_t tx, int32_t ty);
void Scenario_EditorStep(void);
void Scenario_EditorRandomTerrain(int32_t seed);
/* Returns NULL on success and fills saved_path. Otherwise returns an error message. */
const char *Scenario_EditorSave(char *saved_path, size_t saved_len);

/* Scenario file access (implemented in s_fileio.c) */
void Scenario_GetDirectory(char *buf, size_t len);
/* Saves to <scenarios dir>/<sanitized name>.scn, creating the directory if needed. */
int32_t Scenario_SaveToDirectory(const ScenarioDef *def, char *saved_path, size_t saved_len);
/* Reads and validates only the header. Used to list scenarios. */
int32_t Scenario_ReadDef(const char *path, ScenarioDef *def);
int32_t Scenario_LoadFile(const char *path);

/* Editor brush IDs. These are not palette tools and sit above the normal tool range. */
#define editorWaterState  30
#define editorLandState   31
#define editorForestState 32

#endif /* SCENARIO_H */
