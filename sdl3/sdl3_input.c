/* sdl3_input.c: SDL3 event polling, mouse tools, camera controls, and text input hooks.
 */

#include "sdl3_input.h"
#include "sdl3_video.h"
#include "sdl3_gui.h"
#include "sdl3_gui_dialogs.h"
#include "sdl3_help.h"
#include "sdl3_settings.h"
#include "sdl3_splash.h"
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

static void ApplyTool(int32_t tx, int32_t ty)
{
    if (!TestBounds(tx, ty)) return;

    if (active_tool == queryState) {
        GUI_Dialogs_ShowQueryInspector(tx, ty);
        return;
    }

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
        PlaceZone(tx, ty, active_tool);
        break;

    case wireState:
        LayWire(tx, ty);
        break;
    case dozeState:
        LayDoze(tx, ty);
        break;
    case parkState:
        LayPark(tx, ty);
        break;
    case roadState:
        LayRoad(tx, ty);
        break;
    case rrState:
        LayRail(tx, ty);
        break;
    default:
        break;
    }
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
                break;
            case SDLK_1: SetSimSpeed(1); break;
            case SDLK_2: SetSimSpeed(2); break;
            case SDLK_3: SetSimSpeed(3); break;

            /* Scenarios */
            case SDLK_4: GUI_LoadScenarioWithStatus(5); break; /* Tokyo 1957 (Monster) */
            case SDLK_5: GUI_LoadScenarioWithStatus(2); break; /* San Francisco 1906 (Earthquake) */
            case SDLK_6: GUI_LoadScenarioWithStatus(3); break; /* Hamburg 1944 (Firestorm) */
            case SDLK_7: GUI_LoadScenarioWithStatus(4); break; /* Bern 1965 (Traffic) */
            case SDLK_8: GUI_LoadScenarioWithStatus(6); break; /* Detroit 1972 (Crime) */
            case SDLK_9: GUI_LoadScenarioWithStatus(7); break; /* Boston 2010 (Meltdown) */

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
                GUI_ShowStatus(SaveCity("quicksave.cty") ? "Quicksaved to quicksave.cty" : "Quicksave failed");
                break;
            case SDLK_F8:
                GUI_ShowStatus(LoadCity("quicksave.cty") ? "Quickloaded quicksave.cty" : "Quickload failed: quicksave.cty missing or unreadable");
                break;

            /* Scenario Evaluation Test Hotkeys */
            case SDLK_F9:
                ScoreWait = 1;
                if (!ScoreType) ScoreType = (ScenarioID ? ScenarioID : 1);
                SDL_Log("[DEBUG] Force-triggering scenario score evaluation on next frame.");
                break;

            case SDLK_F10:
                if (DisasterEvent && DisasterWait > 2) {
                    DisasterWait = 2;
                    SDL_Log("[DEBUG] Fast-forwarded disaster wait timer.");
                }
                break;

            case SDLK_F11:
                if (EngineCallbacks.show_message) {
                    EngineCallbacks.show_message(-100, 0, 0);
                }
                break;

            case SDLK_F12:
                if (EngineCallbacks.show_message) {
                    EngineCallbacks.show_message(-200, 0, 0);
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
                    ApplyTool(tx, ty);
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
                        ApplyTool(tx, ty);
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