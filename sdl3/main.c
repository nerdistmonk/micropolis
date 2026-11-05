/* main.c: Application entry point and orchestrator for Micropolis SDL3.
 */

#include "sim.h"
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

/* The core sends (0,0) for these notices so auto-goto does not scroll the map.
 * The notice's View button still needs a target, so take it from the sim state the way Unix does.
 */
static void NoticeViewTarget(int16_t msg_num, int16_t *x, int16_t *y)
{
    if (*x != 0 || *y != 0) return;

    switch (msg_num) {
    case -10: *x = PolMaxX;   *y = PolMaxY;   break;
    case -11: *x = CrimeMaxX; *y = CrimeMaxY; break;
    case -12: *x = TrafMaxX;  *y = TrafMaxY;  break;
    case -35: case -36: case -37: case -38: case -39: case -40:
        *x = CCx; *y = CCy;
        break;
    default:
        break;
    }
}

static void OnMessageReceived(int16_t msg_num, int16_t x, int16_t y)
{
    if (msg_num == -100) {
        GUI_Dialogs_ShowScenarioResult(true, "Congratulations, Mayor! You fulfilled the city charter.");
    } else if (msg_num == -200) {
        GUI_Dialogs_ShowScenarioResult(false, "You have been recalled! City charter objectives were not met.");
    } else if (msg_num < 0) {
        if (DoNotices) {
            NoticeViewTarget(msg_num, &x, &y);
            GUI_Dialogs_ShowNotice((int16_t)-msg_num, (int32_t)x, (int32_t)y);
        }
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
    (void)argc;
    (void)argv;

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
        .request_budget_dialog = OnRequestBudget
    };
    Sim_RegisterCallbacks(&callbacks);

    Sim_InitEngine();
    GenerateMap(1337);
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
        GUI_Dialogs_UpdateNotice(now);
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