/* main.c: Application entry point and orchestrator for Micropolis SDL3.
 */

#include "sim.h"
#include "scenario.h"
#include "sdl3_video.h"
#include "sdl3_input.h"
#include "sdl3_audio.h"
#include "sdl3_music.h"
#include "sdl3_gui.h"
#include "sdl3_gui_dialogs.h"
#include "sdl3_help.h"
#include "sdl3_settings.h"
#include "sdl3_splash.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *cli_city_path = NULL;
static const char *cli_scenario_arg = NULL;

static void PrintUsage(FILE *out)
{
    fprintf(out,
        "Usage: micropolis [--load-city FILE] [--load-scenario FILE]\n"
        "  --load-city FILE        Start with a saved city (.cot or .cty)\n"
        "  --load-scenario FILE    Start with a scenario file (.scn)\n");
}

/* Returns 1 to start the game, 0 to exit successfully, -1 on a usage error. */
static int ParseArgs(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            PrintUsage(stdout);
            return 0;
        } else if (strcmp(arg, "--load-city") == 0 && i + 1 < argc) {
            cli_city_path = argv[++i];
        } else if (strcmp(arg, "--load-scenario") == 0 && i + 1 < argc) {
            cli_scenario_arg = argv[++i];
        } else {
            fprintf(stderr, "micropolis: unknown or incomplete option '%s'\n", arg);
            PrintUsage(stderr);
            return -1;
        }
    }
    if (cli_city_path && cli_scenario_arg) {
        fprintf(stderr, "micropolis: use either --load-city or --load-scenario, not both\n");
        return -1;
    }
    return 1;
}

/* Returns false when no startup file was requested or the requested one failed to load. */
static bool LoadStartupFile(void)
{
    if (cli_scenario_arg) {
        bool ok = Scenario_LoadFile(cli_scenario_arg) != 0;
        if (!ok) fprintf(stderr, "micropolis: could not load scenario '%s'\n", cli_scenario_arg);
        return ok;
    }
    if (cli_city_path) {
        if (!LoadCity(cli_city_path)) {
            fprintf(stderr, "micropolis: could not load city '%s'\n", cli_city_path);
            return false;
        }
        return true;
    }
    return false;
}

static void OnMessageReceived(int16_t msg_num, int16_t x, int16_t y)
{
    (void)x;
    (void)y;
    if (msg_num == -100) {
        GUI_Dialogs_ShowScenarioResult(true, "Congratulations, Mayor! You fulfilled the city charter.");
    } else if (msg_num == -200) {
        GUI_Dialogs_ShowScenarioResult(false, "You have been recalled! City charter objectives were not met.");
    }
}

static void OnAutoGoto(int16_t x, int16_t y, const char *msg)
{
    (void)msg;
    if (x > 0 || y > 0) {
        Video_CenterOnTile((int32_t)x, (int32_t)y);
    }
}

static void OnRequestBudget(void)
{
    GUI_Dialogs_OpenAnnualBudget();
}

static void OnUpdateFunds(int32_t funds)
{
    (void)funds;
    MustUpdateFunds = 1;
}

static void OnUpdateDate(int32_t year, int32_t month)
{
    (void)year;
    (void)month;
}

int main(int argc, char **argv)
{
    int parsed = ParseArgs(argc, argv);
    if (parsed <= 0) return parsed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    SetDataRoot(SDL_GetBasePath());

    if (!Video_Init(WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT)) {
        return 1;
    }

    Settings_Load();
    Video_SetLinearFilter(Settings_GetLinearFilter());
    Audio_Init();
    Input_Init();
    GUI_Init();
    Help_Init();

    SimCallbacks callbacks = {
        .update_funds = OnUpdateFunds,
        .update_date = OnUpdateDate,
        .show_message = OnMessageReceived,
        .auto_goto = OnAutoGoto,
        .invalidate_maps = NULL,
        .invalidate_editors = NULL,
        .play_sound = Audio_PlaySound,
        .make_sound = Audio_MakeSound,
        .request_budget_dialog = OnRequestBudget,
        .news_event = News_OnGameEvent,
        .news_reset = News_ResetLog
    };
    Sim_RegisterCallbacks(&callbacks);

    Sim_InitEngine();
    if (!LoadStartupFile()) GenerateMap(1337);
    SetSimSpeed(3);

    bool running = true;
    uint64_t last_tick_time = SDL_GetTicks();
    uint64_t last_anim_time = SDL_GetTicks();
    const uint64_t tick_interval_ms = 40;
    const uint64_t anim_interval_ms = 120;

    Splash_Start();

    while (running) {
        Input_ProcessEvents();
        if (GUI_Dialogs_TakeQuitConfirmed()) running = false;

        uint64_t now = SDL_GetTicks();
        Splash_Update(now);
        Input_UpdateEdgeScroll(now);
        Music_Update();

        /* Sim core tick - suspended while modal dialogs or the splash are up */
        if (!GUI_Dialogs_IsModalOpen() && !Splash_IsActive()) {
            if (now - last_tick_time >= tick_interval_ms) {
                SimFrame();
                doMessage();
                MoveObjects();
                last_tick_time = now;
            }
        } else {
            last_tick_time = now;
        }

        /* Tile animation tick */
        if (now - last_anim_time >= anim_interval_ms) {
            AnimateTiles();
            last_anim_time = now;
        }

        Video_RenderFrame();
        SDL_Delay(8);
    }

    Splash_Dismiss();
    Help_Shutdown();
    GUI_Shutdown();
    Audio_Shutdown();
    Video_Shutdown();
    FreeMapBuffers();

    return 0;
}