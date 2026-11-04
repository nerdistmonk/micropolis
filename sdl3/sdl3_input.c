/* sdl3_input.c: SDL3 event polling, mouse tools, camera controls, and text input hooks.
 */

#include "sdl3_input.h"
#include "sdl3_video.h"
#include "sdl3_gui.h"
#include "sdl3_gui_dialogs.h"
#include "sdl3_help.h"
#include "sdl3_settings.h"
#include "sdl3_splash.h"
#include "sdl3_audio.h"
#include "sim.h"
#include <SDL3/SDL.h>

#define EDGE_SCROLL_ZONE_PX 16.0f
#define EDGE_SCROLL_INTERVAL_MS 10

static int16_t active_tool = residentialState;
static bool is_dragging = false;
static float drag_origin_x = 0.0f;
static float drag_origin_y = 0.0f;
static int32_t initial_cam_x = 0;
static int32_t initial_cam_y = 0;

static int32_t last_drag_tile_x = -1;
static int32_t last_drag_tile_y = -1;

static float pointer_x = 0.0f;
static float pointer_y = 0.0f;
static bool pointer_known = false;
static uint64_t last_edge_scroll_ms = 0;

/* Success tones per tool, matching Unix UIDidTool* in res/micropolis.tcl */
static void PlayToolTone(int16_t tool)
{
    switch (tool) {
    case residentialState: Audio_MakeSound("edit", "o -speed 140"); break;
    case commercialState:  Audio_MakeSound("edit", "a -speed 140"); break;
    case industrialState:  Audio_MakeSound("edit", "e -speed 140"); break;
    case fireState:        Audio_MakeSound("edit", "o -speed 130"); break;
    case policeState:      Audio_MakeSound("edit", "e -speed 130"); break;
    case powerState:       Audio_MakeSound("edit", "o -speed 75"); break;
    case nuclearState:     Audio_MakeSound("edit", "e -speed 75"); break;
    case stadiumState:     Audio_MakeSound("edit", "o -speed 90"); break;
    case seaportState:     Audio_MakeSound("edit", "e -speed 90"); break;
    case airportState:     Audio_MakeSound("edit", "a -speed 50"); break;
    case parkState:        Audio_MakeSound("edit", "a -speed 130"); break;
    case roadState:        Audio_MakeSound("edit", "e -speed 100"); break;
    case rrState:          Audio_MakeSound("edit", "o -speed 100"); break;
    case wireState:        Audio_MakeSound("edit", "o -speed 120"); break;
    default: break;
    }
}

/* Mirrors Unix ToolDown: -2 is insufficient funds, anything else is a refusal */
static void ReportToolFailure(int32_t result)
{
    ClearMes();
    if (result == -2) {
        SendMes(33);
        Audio_MakeSound("edit", "Sorry");
    } else {
        SendMes(34);
        Audio_MakeSound("edit", "UhUh");
    }
}

static void ApplyTool(int32_t tx, int32_t ty, bool first)
{
    if (!TestBounds(tx, ty)) return;

    if (active_tool == queryState) {
        GUI_Dialogs_ShowQueryInspector(tx, ty);
        Audio_MakeSound("edit", "e -speed 200");
        return;
    }

    int32_t result;
    switch (active_tool) {
    case residentialState:
    case commercialState:
    case industrialState:
    case fireState:
    case policeState:
    case powerState:
    case stadiumState:
    case seaportState:
    case nuclearState:
    case airportState:
        result = PlaceZone(tx, ty, active_tool);
        break;

    case wireState:
        result = LayWire(tx, ty);
        break;
    case dozeState:
        result = LayDoze(tx, ty);
        break;
    case parkState:
        result = LayPark(tx, ty);
        break;
    case roadState:
        result = LayRoad(tx, ty);
        break;
    case rrState:
        result = LayRail(tx, ty);
        break;
    default:
        return;
    }

    if (result == 1) {
        /* Bulldozer clips are played by the core */
        if (active_tool != dozeState) PlayToolTone(active_tool);
        return;
    }

    /* Failure feedback only on the initial click, not while dragging */
    if (!first) return;

    /* Bulldozing empty ground is silent */
    if (active_tool == dozeState && result == 0) return;

    ReportToolFailure(result);
}

void Input_Init(void)
{
    active_tool = residentialState;
    is_dragging = false;
    last_drag_tile_x = -1;
    last_drag_tile_y = -1;
    pointer_known = false;
    last_edge_scroll_ms = 0;
}

int16_t Input_GetActiveTool(void)
{
    return active_tool;
}

void Input_SetActiveTool(int16_t tool)
{
    active_tool = tool;
}

void Input_UpdateEdgeScroll(uint64_t now)
{
    if (!Settings_GetEdgeScroll() || !pointer_known || is_dragging ||
        GUI_IsModalOpen() || Splash_IsActive() || Help_IsModeActive()) {
        last_edge_scroll_ms = now;
        return;
    }
    if (now - last_edge_scroll_ms < EDGE_SCROLL_INTERVAL_MS) return;
    last_edge_scroll_ms = now;

    if (GUI_HelpTopicAt(pointer_x, pointer_y) != HELP_MAP) return;

    int32_t dx = 0;
    int32_t dy = 0;
    if (pointer_x < (float)TOOLBAR_WIDTH + EDGE_SCROLL_ZONE_PX) dx = -1;
    else if (pointer_x >= (float)WINDOW_DEFAULT_WIDTH - EDGE_SCROLL_ZONE_PX) dx = 1;
    if (pointer_y < (float)STATUS_BAR_HEIGHT + EDGE_SCROLL_ZONE_PX) dy = -1;
    else if (pointer_y >= (float)WINDOW_DEFAULT_HEIGHT - EDGE_SCROLL_ZONE_PX) dy = 1;

    if (dx || dy) Video_PanCamera(dx, dy);
}

void Input_ProcessEvents(void)
{
    SDL_Event event;
    SDL_Renderer *renderer = Video_GetRenderer();

    while (SDL_PollEvent(&event)) {
        Video_HandleWindowEvent(&event);

        if (renderer) {
            SDL_ConvertEventToRenderCoordinates(renderer, &event);
        }

        if (Splash_IsActive() &&
            (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)) {
            Splash_Dismiss();
            continue;
        }

        switch (event.type) {
        case SDL_EVENT_QUIT:
            GUI_Dialogs_OpenQuitConfirm();
            break;

        case SDL_EVENT_TEXT_INPUT:
            if (GUI_IsModalOpen()) {
                GUI_HandleTextInput(event.text.text);
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            if (GUI_IsModalOpen()) {
                GUI_HandleKeyDown(event.key.key);
                break;
            }

            /* Escape dismisses an open notice before it can quit */
            if (Help_HandleKey(event.key.key)) break;
            if (GUI_Dialogs_HandleNoticeKey(event.key.key)) break;

            switch (event.key.key) {
            case SDLK_ESCAPE:
                GUI_Dialogs_OpenQuitConfirm();
                break;
            case SDLK_SPACE:
                SetSimSpeed(SimSpeed == 0 ? GetLastRunSpeed() : 0);
                Audio_PlayRunningSound(SimSpeed);
                break;
            case SDLK_1: SetSimSpeed(1); Audio_PlayRunningSound(SimSpeed); break;
            case SDLK_2: SetSimSpeed(2); Audio_PlayRunningSound(SimSpeed); break;
            case SDLK_3: SetSimSpeed(3); Audio_PlayRunningSound(SimSpeed); break;

            case SDLK_Q: active_tool = queryState; break;
            case SDLK_R: active_tool = residentialState; break;
            case SDLK_C: active_tool = commercialState; break;
            case SDLK_I: active_tool = industrialState; break;
            case SDLK_F: active_tool = fireState; break;
            case SDLK_P: active_tool = policeState; break;
            case SDLK_W: active_tool = wireState; break;
            case SDLK_D: active_tool = dozeState; break;
            case SDLK_T: active_tool = roadState; break;

            /* Persistence Hotkeys */
            case SDLK_F7:
                if (SaveCity("quicksave.cty")) {
                    GUI_ShowStatus("Quicksaved to quicksave.cty");
                } else {
                    GUI_ShowStatus("Quicksave failed");
                    Audio_MakeSound("warning", "Sorry -speed 85");
                }
                break;
            case SDLK_F8:
                if (LoadCity("quicksave.cty")) {
                    GUI_ShowStatus("Quickloaded quicksave.cty");
                } else {
                    GUI_ShowStatus("Quickload failed: quicksave.cty missing or unreadable");
                    Audio_MakeSound("warning", "Sorry -speed 85");
                }
                break;

            case SDLK_LEFT:
            case SDLK_A:
                Video_PanCamera(-2, 0);
                break;
            case SDLK_RIGHT:
            case SDLK_E:
                Video_PanCamera(2, 0);
                break;
            case SDLK_UP:
            case SDLK_COMMA:
                Video_PanCamera(0, -2);
                break;
            case SDLK_DOWN:
            case SDLK_O:
                Video_PanCamera(0, 2);
                break;
            default:
                break;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (Help_IsModeActive() && !GUI_IsModalOpen() && event.button.button == SDL_BUTTON_LEFT) {
                /* Notice buttons keep working in help mode */
                if (!GUI_Dialogs_HandleClick(event.button.x, event.button.y)) {
                    Help_HandleClick(event.button.x, event.button.y,
                                     GUI_HelpTopicAt(event.button.x, event.button.y));
                }
                break;
            }

            if (GUI_HandleClick(event.button.x, event.button.y, event.button.button)) {
                break;
            }

            if (GUI_IsModalOpen()) {
                break;
            }

            if (event.button.button == SDL_BUTTON_LEFT) {
                int32_t tx, ty;
                if (Video_ScreenToTile(event.button.x, event.button.y, &tx, &ty)) {
                    ApplyTool(tx, ty, true);
                    last_drag_tile_x = tx;
                    last_drag_tile_y = ty;
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                is_dragging = true;
                drag_origin_x = event.button.x;
                drag_origin_y = event.button.y;
                Video_GetCamera(&initial_cam_x, &initial_cam_y);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_RIGHT) {
                is_dragging = false;
            }
            last_drag_tile_x = -1;
            last_drag_tile_y = -1;
            break;

        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            pointer_known = false;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            GUI_HandleMotion(event.motion.x, event.motion.y);
            pointer_x = event.motion.x;
            pointer_y = event.motion.y;
            pointer_known = true;

            if (is_dragging) {
                float dx = event.motion.x - drag_origin_x;
                float dy = event.motion.y - drag_origin_y;
                int32_t tile_dx = (int32_t)(dx / (float)TILE_PIXEL_SIZE);
                int32_t tile_dy = (int32_t)(dy / (float)TILE_PIXEL_SIZE);
                Video_SetCamera(initial_cam_x - tile_dx, initial_cam_y - tile_dy);
            } else if ((event.motion.state & SDL_BUTTON_LMASK) && !GUI_IsModalOpen() &&
                       !Help_IsModeActive() && active_tool != queryState) {
                int32_t tx, ty;
                if (Video_ScreenToTile(event.motion.x, event.motion.y, &tx, &ty)) {
                    if (tx != last_drag_tile_x || ty != last_drag_tile_y) {
                        ApplyTool(tx, ty, false);
                        last_drag_tile_x = tx;
                        last_drag_tile_y = ty;
                    }
                }
            }
            break;

        default:
            break;
        }
    }
}