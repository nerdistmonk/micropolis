/* sdl3_input.c: SDL3 event polling, mouse tools, camera controls, and text input hooks.
 */

#include "sdl3_input.h"
#include "sdl3_video.h"
#include "sdl3_gui.h"
#include "sim.h"
#include <SDL3/SDL.h>

static int16_t active_tool = residentialState;
static bool is_dragging = false;
static float drag_origin_x = 0.0f;
static float drag_origin_y = 0.0f;
static int32_t initial_cam_x = 0;
static int32_t initial_cam_y = 0;

static int32_t last_drag_tile_x = -1;
static int32_t last_drag_tile_y = -1;

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
}

int16_t Input_GetActiveTool(void)
{
  return active_tool;
}

void Input_SetActiveTool(int16_t tool)
{
  active_tool = tool;
}

void Input_ProcessEvents(bool *running)
{
  SDL_Event event;
  SDL_Renderer *renderer = Video_GetRenderer();

  while (SDL_PollEvent(&event)) {
    Video_HandleWindowEvent(&event);

    if (renderer) {
      SDL_ConvertEventToRenderCoordinates(renderer, &event);
    }

    switch (event.type) {
    case SDL_EVENT_QUIT:
      if (running) *running = false;
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

      switch (event.key.key) {
      case SDLK_ESCAPE:
        if (running) *running = false;
        break;
      case SDLK_SPACE:
        SetSimSpeed(SimSpeed == 0 ? 3 : 0);
        break;
      case SDLK_1: SetSimSpeed(1); break;
      case SDLK_2: SetSimSpeed(2); break;
      case SDLK_3: SetSimSpeed(3); break;

      /* Scenarios */
      case SDLK_4: LoadScenario(5); break; /* Tokyo 1957 (Monster) */
      case SDLK_5: LoadScenario(2); break; /* San Francisco 1906 (Earthquake) */
      case SDLK_6: LoadScenario(3); break; /* Hamburg 1944 (Firestorm) */
      case SDLK_7: LoadScenario(4); break; /* Bern 1965 (Traffic) */
      case SDLK_8: LoadScenario(6); break; /* Detroit 1972 (Crime) */
      case SDLK_9: LoadScenario(7); break; /* Boston 2010 (Meltdown) */

      case SDLK_Q: active_tool = queryState; break;
      case SDLK_R: active_tool = residentialState; break;
      case SDLK_C: active_tool = commercialState; break;
      case SDLK_I: active_tool = industrialState; break;
      case SDLK_F: active_tool = fireState; break;
      case SDLK_P: active_tool = policeState; break;
      case SDLK_W: active_tool = wireState; break;
      case SDLK_D: active_tool = dozeState; break;
      case SDLK_T: active_tool = roadState; break;

      /* Disaster Hotkeys */
      case SDLK_F1: MakeFire(); break;
      case SDLK_F2: MakeFlood(); break;
      case SDLK_F3: MakeEarthquake(); break;
      case SDLK_F4: MakeTornado(); break;
      case SDLK_F5: MakeMonster(); break;
      case SDLK_F6: MakeMeltdown(); break;

      /* Persistence Hotkeys */
      case SDLK_F7: SaveCity("quicksave.cty"); break;
      case SDLK_F8: LoadCity("quicksave.cty"); break;

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

    case SDL_EVENT_MOUSE_MOTION:
      GUI_HandleMotion(event.motion.x, event.motion.y);

      if (is_dragging) {
        float dx = event.motion.x - drag_origin_x;
        float dy = event.motion.y - drag_origin_y;
        int32_t tile_dx = (int32_t)(dx / (float)TILE_PIXEL_SIZE);
        int32_t tile_dy = (int32_t)(dy / (float)TILE_PIXEL_SIZE);
        Video_SetCamera(initial_cam_x - tile_dx, initial_cam_y - tile_dy);
      } else if ((event.motion.state & SDL_BUTTON_LMASK) && !GUI_IsModalOpen() && active_tool != queryState) {
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