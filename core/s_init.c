/* s_init.c: Engine initialization and layer reset routines.
 *
 * Originally designed and implemented by Will Wright.
 * Unix version by Don Hopkins.
 *
 * Modernized to C11 / C17 for headless execution and SDL3 integration.
 */

#include "sim.h"
#include "scenario.h"

/* Forward Declarations of Internal Functions */
static void ResetLastKeys(void);
static void DoNewGame(void);

void InitWillStuff(void)
{
    SeedRand(1);

    RoadEffect   = 32;
    PoliceEffect = 1000;
    FireEffect   = 1000;
    RoadFund      = 0;
    PoliceFund    = 0;
    FireFund      = 0;

    ResetCityWorkingState();

    ResetLastKeys();
    DoNewGame();
}

void ResetCityWorkingState(void)
{
    Scenario_Deactivate();

    CityScore    = 500;
    CityPop      = -1;
    OverRide     = 0;
    PendingTool  = -1;

    MesNum        = 0;
    MessagePort   = 0;
    ValveFlag     = 1;
    DisasterEvent = 0;
    TaxFlag       = 0;

    for (int16_t x = 0; x < HWLDX; x++) {
        for (int16_t y = 0; y < HWLDY; y++) {
            PopDensity[x][y]   = 0;
            TrfDensity[x][y]   = 0;
            PollutionMem[x][y] = 0;
            LandValueMem[x][y] = 0;
            CrimeMem[x][y]     = 0;
        }
    }

    for (int16_t x = 0; x < QWX; x++) {
        for (int16_t y = 0; y < QWY; y++) {
            TerrainMem[x][y] = 0;
        }
    }

    for (int16_t x = 0; x < SmX; x++) {
        for (int16_t y = 0; y < SmY; y++) {
            RateOGMem[x][y]       = 0;
            FireRate[x][y]        = 0;
            ComRate[x][y]         = 0;
            PoliceMap[x][y]       = 0;
            PoliceMapEffect[x][y] = 0;
        }
    }
}

static void ResetLastKeys(void)
{
    /* Stubbed input key history reset */
}

static void DoNewGame(void)
{
    CityTime = 0;
    StartingYear = 1900;
}

void ResetMapState(void)
{
    if (EngineCallbacks.invalidate_maps) {
        EngineCallbacks.invalidate_maps();
    }
}

void ResetEditorState(void)
{
    if (EngineCallbacks.invalidate_editors) {
        EngineCallbacks.invalidate_editors();
    }
}