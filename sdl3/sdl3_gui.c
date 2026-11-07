/* sdl3_gui.c: Main HUD orchestrator, toolbar icons, RCI gauge, inspector, and tooltips.
 */

#include "sdl3_gui.h"
#include "sdl3_gui_text.h"
#include "sdl3_gui_dialogs.h"
#include "sdl3_help.h"
#include "sdl3_input.h"
#include "sdl3_audio.h"
#include "sdl3_video.h"
#include "sdl3_season.h"
#include "sim.h"
#include "scenario.h"
#include "sdl3_paths.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  int16_t tool_id;
  const char *full_name;
  const char *icon_stem;
  int32_t cost;
  SDL_FRect rect;
  SDL_Texture *tex_normal;
  SDL_Texture *tex_hi;
} GuiToolButton;

static GuiToolButton tool_buttons[] = {
  { residentialState, "Residential Zone", "icres", 100, { 6.0f, 34.0f, 46.0f, 46.0f }, NULL, NULL },
  { commercialState,  "Commercial Zone",  "iccom", 100, { 56.0f, 34.0f, 46.0f, 46.0f }, NULL, NULL },
  { industrialState,  "Industrial Zone",  "icind", 100, { 6.0f, 84.0f, 46.0f, 46.0f }, NULL, NULL },
  { fireState,        "Fire Station",     "icfire", 500, { 56.0f, 84.0f, 46.0f, 46.0f }, NULL, NULL },
  { policeState,      "Police Department", "icpol", 500, { 6.0f, 134.0f, 46.0f, 46.0f }, NULL, NULL },
  { wireState,        "Power Lines",      "icwire", 5, { 56.0f, 134.0f, 46.0f, 46.0f }, NULL, NULL },
  { dozeState,        "Bulldozer",        "icdozr", 1, { 6.0f, 184.0f, 46.0f, 46.0f }, NULL, NULL },
  { roadState,        "Roadway",          "icroad", 10, { 56.0f, 184.0f, 46.0f, 46.0f }, NULL, NULL },
  { rrState,          "Railroad",         "icrail", 20, { 6.0f, 234.0f, 46.0f, 46.0f }, NULL, NULL },
  { powerState,       "Coal Power Plant", "iccoal", 3000, { 56.0f, 234.0f, 46.0f, 46.0f }, NULL, NULL },
  { nuclearState,     "Nuclear Power Plant", "icnuc", 5000, { 6.0f, 284.0f, 46.0f, 46.0f }, NULL, NULL },
  { stadiumState,     "Sports Stadium",   "icstad", 5000, { 56.0f, 284.0f, 46.0f, 46.0f }, NULL, NULL },
  { seaportState,     "Seaport",          "icseap", 3000, { 6.0f, 334.0f, 46.0f, 46.0f }, NULL, NULL },
  { airportState,     "Municipal Airport", "icairp", 10000, { 56.0f, 334.0f, 46.0f, 46.0f }, NULL, NULL },
  { parkState,        "Park",             "icpark", 20, { 6.0f, 384.0f, 46.0f, 46.0f }, NULL, NULL },
  { waterTowerState,  "Water Tower",      "icwtower", 500, { 6.0f, 434.0f, 46.0f, 46.0f }, NULL, NULL },
  { pipeState,        "Water Pipe",       "icpipe", 2, { 56.0f, 434.0f, 46.0f, 46.0f }, NULL, NULL },
  { hospitalState,    "Hospital",         "ichosp", 500, { 56.0f, 384.0f, 46.0f, 46.0f }, NULL, NULL },
  { landfillState,    "Landfill",         "icland", 100, { 6.0f, 484.0f, 46.0f, 46.0f }, NULL, NULL }
};

#define NUM_TOOL_BUTTONS (sizeof(tool_buttons) / sizeof(tool_buttons[0]))

static int hovered_tool_idx = -1;
static bool hovered_query_btn = false;
static float cursor_x = 0.0f;
static float cursor_y = 0.0f;

/* Status bar buttons */
/* Status-bar buttons are anchored to the right edge, and the query and RCI panels sit above the radar map.
   These are refreshed by UpdateLayout() on each entry point, so the defaults match a 1024x768 window. */
static SDL_FRect rect_date_btn      = { 4.0f, 2.0f, 120.0f, 24.0f };
static SDL_FRect rect_funds_btn     = { 135.0f, 2.0f, 150.0f, 24.0f };
static SDL_FRect rect_city_btn      = { 290.0f, 2.0f, 260.0f, 24.0f };
static SDL_FRect rect_overlays_btn  = { 560.0f, 2.0f, 110.0f, 24.0f };
static SDL_FRect rect_disasters_btn = { 680.0f, 2.0f,  96.0f, 24.0f };
static SDL_FRect rect_file_btn      = { 784.0f, 2.0f,  84.0f, 24.0f };
static SDL_FRect rect_speed_btn     = { 876.0f, 2.0f, 140.0f, 24.0f };

/* Ticker & Notification State */
static uint64_t ticker_start_time = 0;
static uint32_t ticker_shown_serial = 0;
static uint32_t ticker_dismissed_serial = 0;

/* Floating Ticker positioned directly beneath status bar */
static SDL_FRect rect_ticker_box     = { 280.0f, 34.0f, 464.0f, 26.0f };
static SDL_FRect rect_ticker_dismiss = { 722.0f, 37.0f,  18.0f, 20.0f };

/* Scenario editor controls, drawn as a footer strip over the bottom of the map while the editor is active */
typedef enum {
  EDITOR_BTN_WATER,
  EDITOR_BTN_LAND,
  EDITOR_BTN_FOREST,
  EDITOR_BTN_RANDOM,
  EDITOR_BTN_STEP,
  EDITOR_BTN_SETUP,
  EDITOR_BTN_SAVE,
  EDITOR_BTN_EXIT,
  EDITOR_BTN_COUNT
} EditorButtonId;

typedef struct {
  const char *label;
  int16_t brush_id;
  SDL_FRect rect;
} EditorButton;

static EditorButton editor_buttons[EDITOR_BTN_COUNT] = {
  { "Water",  editorWaterState,  { 0.0f, 0.0f, 0.0f, 0.0f } },
  { "Land",   editorLandState,   { 0.0f, 0.0f, 0.0f, 0.0f } },
  { "Forest", editorForestState, { 0.0f, 0.0f, 0.0f, 0.0f } },
  { "Random", -1,                { 0.0f, 0.0f, 0.0f, 0.0f } },
  { "Step",   -1,                { 0.0f, 0.0f, 0.0f, 0.0f } },
  { "Setup",  -1,                { 0.0f, 0.0f, 0.0f, 0.0f } },
  { "Save",   -1,                { 0.0f, 0.0f, 0.0f, 0.0f } },
  { "Exit",   -1,                { 0.0f, 0.0f, 0.0f, 0.0f } }
};
static int hovered_editor_idx = -1;

static int EditorButtonAt(float x, float y)
{
  if (!Scenario_EditorIsActive()) return -1;
  for (int i = 0; i < EDITOR_BTN_COUNT; i++) {
    SDL_FRect r = editor_buttons[i].rect;
    if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) return i;
  }
  return -1;
}

int32_t GUI_CityPopDisplay(void)
{
  return (CityPop < 0) ? 0 : CityPop;
}

void GUI_ShowStatus(const char *text)
{
  SetMessageField(text, 0, 0, true);
}


#define TICKER_DURATION_MS 12000
#define TICKER_FADE_MS     3000

/* Query Tool and RCI Gauge Rectangles sit above the Radar Map on the right edge (x and y set in UpdateLayout) */
static SDL_FRect rect_query_btn = { 0.0f, 538.0f, 120.0f, 30.0f };
static SDL_FRect rect_rci_box   = { 0.0f, 574.0f, 120.0f, 60.0f };

static void UpdateLayout(void)
{
  float cw = (float)Video_GetCanvasWidth();
  float radar_y = Video_GetMinimapRect().y;
  float right_col_x = Video_GetMinimapRect().x;

  rect_speed_btn.x = cw - 148.0f;
  rect_file_btn.x = cw - 240.0f;
  rect_disasters_btn.x = cw - 344.0f;
  rect_overlays_btn.x = cw - 464.0f;
  rect_query_btn.x = right_col_x;
  rect_query_btn.y = radar_y - 118.0f;
  rect_rci_box.x = right_col_x;
  rect_rci_box.y = radar_y - 82.0f;

  for (int i = 0; i < EDITOR_BTN_COUNT; i++) {
    editor_buttons[i].rect.x = (float)(TOOLBAR_WIDTH + 24) + (float)i * 88.0f;
    editor_buttons[i].rect.y = (float)Video_GetCanvasHeight() - 34.0f;
    editor_buttons[i].rect.w = 84.0f;
    editor_buttons[i].rect.h = 24.0f;
  }
}

static const SDL_Color col_white = { 245, 245, 250, 255 };
static const SDL_Color col_gold  = { 255, 215, 0, 255 };
static const SDL_Color col_green = { 100, 255, 120, 255 };
static const SDL_Color col_cyan  = { 90, 210, 255, 255 };

static void LoadToolButtonIcons(void)
{
  char rel[512];
  char path[512];

  for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
    GuiToolButton *btn = &tool_buttons[i];

    snprintf(rel, sizeof(rel), "images/%s.xpm", btn->icon_stem);
    if (Res_Path(path, sizeof(path), rel)) btn->tex_normal = Video_LoadTexture(path, NULL, NULL);

    snprintf(rel, sizeof(rel), "images/%shi.xpm", btn->icon_stem);
    if (Res_Path(path, sizeof(path), rel)) btn->tex_hi = Video_LoadTexture(path, NULL, NULL);
  }
}

bool GUI_Init(void)
{
  GUI_Text_Init();
  LoadToolButtonIcons();
  hovered_tool_idx = -1;
  hovered_query_btn = false;
  ticker_start_time = 0;
  ticker_shown_serial = 0;
  ticker_dismissed_serial = 0;
  return true;
}

void GUI_Shutdown(void)
{
  GUI_Dialogs_Close();

  for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
    if (tool_buttons[i].tex_normal) {
      SDL_DestroyTexture(tool_buttons[i].tex_normal);
      tool_buttons[i].tex_normal = NULL;
    }
    if (tool_buttons[i].tex_hi) {
      SDL_DestroyTexture(tool_buttons[i].tex_hi);
      tool_buttons[i].tex_hi = NULL;
    }
  }

  GUI_Text_Shutdown();
}

bool GUI_IsModalOpen(void)
{
  return GUI_Dialogs_IsModalOpen();
}

void GUI_OpenCityMgmtModal(CityMgmtTab initial_tab)
{
  GUI_Dialogs_OpenCityMgmt(initial_tab);
}

void GUI_OpenOverlaysModal(void)
{
  GUI_Dialogs_OpenOverlays();
}

void GUI_OpenFileModal(FileDialogTab initial_tab)
{
  GUI_Dialogs_OpenFile(initial_tab);
}

void GUI_OpenDisastersModal(void)
{
  GUI_Dialogs_OpenDisasters();
}

void GUI_OpenNewsModal(void)
{
  GUI_Dialogs_OpenNews();
}

void GUI_CloseModal(void)
{
  GUI_Dialogs_Close();
}

OverlayViewMode GUI_GetActiveOverlayMode(void)
{
  return GUI_Dialogs_GetActiveOverlay();
}

void GUI_SetActiveOverlayMode(OverlayViewMode mode)
{
  GUI_Dialogs_SetActiveOverlay(mode);
}

void GUI_HandleMotion(float mouse_x, float mouse_y)
{
  UpdateLayout();
  cursor_x = mouse_x;
  cursor_y = mouse_y;

  if (GUI_IsModalOpen()) {
    hovered_tool_idx = -1;
    hovered_query_btn = false;
    hovered_editor_idx = -1;
    return;
  }

  hovered_tool_idx = -1;
  hovered_query_btn = false;
  hovered_editor_idx = EditorButtonAt(mouse_x, mouse_y);
  if (hovered_editor_idx >= 0) return;

  if (mouse_x >= rect_query_btn.x && mouse_x <= (rect_query_btn.x + rect_query_btn.w) &&
      mouse_y >= rect_query_btn.y && mouse_y <= (rect_query_btn.y + rect_query_btn.h)) {
    hovered_query_btn = true;
    return;
  }

  if (mouse_x <= (float)TOOLBAR_WIDTH && mouse_y > (float)STATUS_BAR_HEIGHT) {
    for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
      SDL_FRect r = tool_buttons[i].rect;
      if (mouse_x >= r.x && mouse_x <= (r.x + r.w) &&
          mouse_y >= r.y && mouse_y <= (r.y + r.h)) {
        hovered_tool_idx = (int)i;
        break;
      }
    }
  }
}

bool GUI_HandleTextInput(const char *text)
{
  return GUI_Dialogs_HandleTextInput(text);
}

bool GUI_HandleKeyDown(SDL_Keycode key)
{
  return GUI_Dialogs_HandleKeyDown(key);
}

static bool TickerVisible(void)
{
  return MessageSerial != 0 && MessageSerial != ticker_dismissed_serial && LastMessage[0];
}

bool GUI_HandleClick(float mouse_x, float mouse_y, uint8_t button)
{
  if (button != SDL_BUTTON_LEFT) return false;
  UpdateLayout();

  if (GUI_Dialogs_HandleClick(mouse_x, mouse_y)) {
    return true;
  }

  /* Ticker: the [x] only dismisses; clicking the rest of the box goes to the incident, then dismisses */
  if (TickerVisible() &&
      mouse_x >= rect_ticker_box.x && mouse_x <= (rect_ticker_box.x + rect_ticker_box.w) &&
      mouse_y >= rect_ticker_box.y && mouse_y <= (rect_ticker_box.y + rect_ticker_box.h)) {
    bool on_dismiss = mouse_x >= rect_ticker_dismiss.x && mouse_x <= (rect_ticker_dismiss.x + rect_ticker_dismiss.w) &&
                      mouse_y >= rect_ticker_dismiss.y && mouse_y <= (rect_ticker_dismiss.y + rect_ticker_dismiss.h);
    if (!on_dismiss && (MessageTargetX != 0 || MessageTargetY != 0)) {
      Video_CenterOnTile(MessageTargetX, MessageTargetY);
    }
    ticker_dismissed_serial = MessageSerial;
    return true;
  }

  /* Radar Map Click */
  if (Video_HandleMinimapClick(mouse_x, mouse_y)) {
    return true;
  }

  /* Status Bar Buttons */
  if (mouse_y <= (float)STATUS_BAR_HEIGHT) {
    if (mouse_x >= rect_date_btn.x && mouse_x <= (rect_date_btn.x + rect_date_btn.w)) {
      GUI_OpenNewsModal();
      return true;
    }
    if (mouse_x >= rect_funds_btn.x && mouse_x <= (rect_funds_btn.x + rect_funds_btn.w)) {
      GUI_OpenCityMgmtModal(CITY_TAB_BUDGET);
      return true;
    }
    if (mouse_x >= rect_city_btn.x && mouse_x <= (rect_city_btn.x + rect_city_btn.w)) {
      GUI_OpenCityMgmtModal(CITY_TAB_CITY_INFO);
      return true;
    }
    if (mouse_x >= rect_overlays_btn.x && mouse_x <= (rect_overlays_btn.x + rect_overlays_btn.w)) {
      GUI_OpenOverlaysModal();
      return true;
    }
    if (mouse_x >= rect_disasters_btn.x && mouse_x <= (rect_disasters_btn.x + rect_disasters_btn.w)) {
      GUI_OpenDisastersModal();
      return true;
    }
    if (mouse_x >= rect_file_btn.x && mouse_x <= (rect_file_btn.x + rect_file_btn.w)) {
      GUI_OpenFileModal(FILE_TAB_SAVE_LOAD);
      return true;
    }
    if (mouse_x >= rect_speed_btn.x && mouse_x <= (rect_speed_btn.x + rect_speed_btn.w)) {
      static const int16_t speed_cycle[] = {
        SIM_SPEED_PAUSED, SIM_SPEED_SUPER_SLOW, SIM_SPEED_SLOW,
        SIM_SPEED_MED, SIM_SPEED_FAST, SIM_SPEED_SUPER_FAST
      };
      size_t cycle_len = sizeof(speed_cycle) / sizeof(speed_cycle[0]);
      size_t cur = 0;
      for (size_t i = 0; i < cycle_len; i++) {
        if (speed_cycle[i] == SimSpeed) cur = i;
      }
      SetSimSpeed(speed_cycle[(cur + 1) % cycle_len]);
      Audio_PlayRunningSound(SimSpeed);
      return true;
    }
    return true;
  }

  /* Scenario editor palette */
  int editor_idx = EditorButtonAt(mouse_x, mouse_y);
  if (editor_idx >= 0) {
    switch (editor_idx) {
    case EDITOR_BTN_WATER:
    case EDITOR_BTN_LAND:
    case EDITOR_BTN_FOREST:
      Input_SetActiveTool(editor_buttons[editor_idx].brush_id);
      break;
    case EDITOR_BTN_STEP:
      GUI_Editor_Step();
      break;
    case EDITOR_BTN_RANDOM:
      GUI_Editor_Random();
      break;
    case EDITOR_BTN_SETUP:
      GUI_Dialogs_OpenEditor();
      break;
    case EDITOR_BTN_SAVE:
      GUI_Editor_Save();
      break;
    case EDITOR_BTN_EXIT:
      GUI_Editor_Exit();
      break;
    default:
      break;
    }
    return true;
  }

  /* Query Tool Button Click */
  if (mouse_x >= rect_query_btn.x && mouse_x <= (rect_query_btn.x + rect_query_btn.w) &&
      mouse_y >= rect_query_btn.y && mouse_y <= (rect_query_btn.y + rect_query_btn.h)) {
    Input_SetActiveTool(queryState);
    return true;
  }

  /* Tool Buttons */
  if (mouse_x <= (float)TOOLBAR_WIDTH) {
    for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
      SDL_FRect r = tool_buttons[i].rect;
      if (mouse_x >= r.x && mouse_x <= (r.x + r.w) &&
          mouse_y >= r.y && mouse_y <= (r.y + r.h)) {
        Input_SetActiveTool(tool_buttons[i].tool_id);
        return true;
      }
    }
    return true;
  }

  /* Right HUD column (Inspect, RCI gauge, radar label) swallows clicks so they never reach the map */
  SDL_FRect radar = Video_GetMinimapRect();
  if (mouse_x >= radar.x && mouse_x <= (radar.x + radar.w) &&
      mouse_y >= rect_query_btn.y && mouse_y <= (radar.y + radar.h)) {
    return true;
  }

  return false;
}

static void RenderTooltip(SDL_Renderer *renderer, int tool_idx)
{
  if (tool_idx < 0 || tool_idx >= (int)NUM_TOOL_BUTTONS) return;

  GuiToolButton *btn = &tool_buttons[tool_idx];
  char text[96];
  if (btn->cost > 0) {
    snprintf(text, sizeof(text), "%s ($%d)", btn->full_name, btn->cost);
  } else {
    snprintf(text, sizeof(text), "%s (Free)", btn->full_name);
  }

  int text_w = GUI_GetTextWidth(text);
  int pad_x = 10;
  int box_w = text_w + (pad_x * 2);
  int box_h = 26;

  float tx = cursor_x + 14.0f;
  float ty = cursor_y + 10.0f;

  if (tx + (float)box_w > (float)Video_GetCanvasWidth()) {
    tx = (float)Video_GetCanvasWidth() - (float)box_w - 6.0f;
  }
  if (ty + (float)box_h > (float)Video_GetCanvasHeight()) {
    ty = cursor_y - (float)box_h - 4.0f;
  }

  SDL_FRect box = { tx, ty, (float)box_w, (float)box_h };
  SDL_SetRenderDrawColor(renderer, 15, 18, 26, 245);
  SDL_RenderFillRect(renderer, &box);

  SDL_SetRenderDrawColor(renderer, 255, 215, 0, 220);
  SDL_RenderRect(renderer, &box);

  GUI_DrawText(renderer, (int)tx + pad_x, (int)ty + 5, text, col_white);
}

static void RenderRciGauge(SDL_Renderer *renderer)
{
  SDL_SetRenderDrawColor(renderer, 16, 20, 28, 250);
  SDL_RenderFillRect(renderer, &rect_rci_box);

  SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  SDL_RenderRect(renderer, &rect_rci_box);

  GUI_DrawText(renderer, (int)rect_rci_box.x + 22, (int)rect_rci_box.y + 3, "DEMAND", col_gold);

  float zero_y = rect_rci_box.y + 32.0f;
  float max_bar_h = 12.0f;
  float label_y = zero_y + max_bar_h + 2.0f;

  SDL_SetRenderDrawColor(renderer, 90, 105, 135, 255);
  SDL_RenderLine(renderer, rect_rci_box.x + 10.0f, zero_y, rect_rci_box.x + rect_rci_box.w - 10.0f, zero_y);

  float r_norm = (float)RValve / 2000.0f;
  float c_norm = (float)CValve / 1500.0f;
  float i_norm = (float)IValve / 1500.0f;

  if (r_norm > 1.0f) r_norm = 1.0f;
  if (r_norm < -1.0f) r_norm = -1.0f;
  if (c_norm > 1.0f) c_norm = 1.0f;
  if (c_norm < -1.0f) c_norm = -1.0f;
  if (i_norm > 1.0f) i_norm = 1.0f;
  if (i_norm < -1.0f) i_norm = -1.0f;

  float bar_w = 18.0f;

  /* R Bar */
  float r_x = rect_rci_box.x + 18.0f;
  float r_h = r_norm * max_bar_h;
  SDL_FRect r_rect = { r_x, (r_h >= 0.0f) ? (zero_y - r_h) : zero_y, bar_w, (r_h >= 0.0f) ? r_h : -r_h };
  SDL_SetRenderDrawColor(renderer, 60, 220, 80, 255);
  SDL_RenderFillRect(renderer, &r_rect);
  GUI_DrawText(renderer, (int)r_x + 5, (int)label_y, "R", col_green);

  /* C Bar */
  float c_x = rect_rci_box.x + 51.0f;
  float c_h = c_norm * max_bar_h;
  SDL_FRect c_rect = { c_x, (c_h >= 0.0f) ? (zero_y - c_h) : zero_y, bar_w, (c_h >= 0.0f) ? c_h : -c_h };
  SDL_SetRenderDrawColor(renderer, 60, 160, 255, 255);
  SDL_RenderFillRect(renderer, &c_rect);
  GUI_DrawText(renderer, (int)c_x + 5, (int)label_y, "C", col_cyan);

  /* I Bar */
  float i_x = rect_rci_box.x + 84.0f;
  float i_h = i_norm * max_bar_h;
  SDL_FRect i_rect = { i_x, (i_h >= 0.0f) ? (zero_y - i_h) : zero_y, bar_w, (i_h >= 0.0f) ? i_h : -i_h };
  SDL_SetRenderDrawColor(renderer, 255, 180, 40, 255);
  SDL_RenderFillRect(renderer, &i_rect);
  GUI_DrawText(renderer, (int)i_x + 5, (int)label_y, "I", col_gold);
}

static void RenderQueryToolButton(SDL_Renderer *renderer)
{
  bool selected = (Input_GetActiveTool() == queryState);

  if (selected) {
    SDL_SetRenderDrawColor(renderer, 50, 85, 140, 255);
  } else if (hovered_query_btn) {
    SDL_SetRenderDrawColor(renderer, 42, 52, 70, 255);
  } else {
    SDL_SetRenderDrawColor(renderer, 28, 34, 46, 255);
  }
  SDL_RenderFillRect(renderer, &rect_query_btn);

  if (selected) {
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
  } else if (hovered_query_btn) {
    SDL_SetRenderDrawColor(renderer, 140, 180, 240, 255);
  } else {
    SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  }
  SDL_RenderRect(renderer, &rect_query_btn);

  GUI_DrawText(renderer, (int)rect_query_btn.x + 20, (int)rect_query_btn.y + 7, "Inspect [?]", selected ? col_gold : col_white);
}

static void RenderEditorButtons(SDL_Renderer *renderer)
{
  int16_t cur_tool = Input_GetActiveTool();

  for (int i = 0; i < EDITOR_BTN_COUNT; i++) {
    EditorButton *btn = &editor_buttons[i];
    bool selected = (btn->brush_id >= 0 && btn->brush_id == cur_tool);
    bool hovered = (hovered_editor_idx == i);

    if (selected) {
      SDL_SetRenderDrawColor(renderer, 48, 85, 140, 255);
    } else if (hovered) {
      SDL_SetRenderDrawColor(renderer, 42, 52, 70, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 30, 36, 48, 255);
    }
    SDL_RenderFillRect(renderer, &btn->rect);

    if (selected) {
      SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    } else if (hovered) {
      SDL_SetRenderDrawColor(renderer, 120, 160, 220, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 60, 72, 90, 255);
    }
    SDL_RenderRect(renderer, &btn->rect);

    GUI_DrawTextCentered(renderer, btn->rect, btn->label, selected ? col_gold : col_white);
  }
}

/* Drawn after the toolbar so it isn't covered by the tool palette. */
static void RenderSeasonTooltip(SDL_Renderer *renderer, int32_t current_month)
{
  bool date_hovered = cursor_x >= rect_date_btn.x && cursor_x <= rect_date_btn.x + rect_date_btn.w &&
                      cursor_y >= rect_date_btn.y && cursor_y <= rect_date_btn.y + rect_date_btn.h;
  if (!date_hovered || GUI_IsModalOpen()) return;

  const char *season = Season_Name(Season_OfMonth(current_month));
  float box_x = rect_date_btn.x;
  float box_y = rect_date_btn.y + rect_date_btn.h + 4.0f;
  SDL_FRect box = { box_x, box_y, (float)(GUI_GetTextWidth(season) + 20), 26.0f };
  SDL_SetRenderDrawColor(renderer, 15, 18, 26, 245);
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 255, 215, 0, 220);
  SDL_RenderRect(renderer, &box);
  GUI_DrawText(renderer, (int)box_x + 10, (int)box_y + 5, season, col_white);
}

void GUI_Render(SDL_Renderer *renderer)
{
  if (!renderer) return;
  UpdateLayout();

  /* 1. Status Bar */
  SDL_FRect status_rect = { 0.0f, 0.0f, (float)Video_GetCanvasWidth(), (float)STATUS_BAR_HEIGHT };
  SDL_SetRenderDrawColor(renderer, 24, 28, 36, 255);
  SDL_RenderFillRect(renderer, &status_rect);

  SDL_SetRenderDrawColor(renderer, 60, 72, 92, 255);
  SDL_RenderLine(renderer, 0.0f, (float)STATUS_BAR_HEIGHT, (float)Video_GetCanvasWidth(), (float)STATUS_BAR_HEIGHT);

  static const char *months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  int32_t current_year = StartingYear + (CityTime / 48);
  int32_t current_month = (CityTime % 48) / 4;
  if (current_month < 0) current_month = 0;
  if (current_month > 11) current_month = 11;

  /* Date Button (opens the newspaper headlines) */
  char date_buf[64];
  snprintf(date_buf, sizeof(date_buf), "%s %d", months[current_month], current_year);
  SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
  SDL_RenderFillRect(renderer, &rect_date_btn);
  SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  SDL_RenderRect(renderer, &rect_date_btn);
  GUI_DrawText(renderer, (int)rect_date_btn.x + 8, (int)rect_date_btn.y + 4, date_buf, col_white);

  if (News_HasUnread()) {
    SDL_FRect badge = { rect_date_btn.x + rect_date_btn.w - 20.0f, rect_date_btn.y + 5.0f, 14.0f, 14.0f };
    SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
    SDL_RenderFillRect(renderer, &badge);
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    SDL_RenderRect(renderer, &badge);
    SDL_Color white = { 245, 245, 250, 255 };
    GUI_DrawTextCentered(renderer, badge, "!", white);
  }

  /* Funds */
  char funds_buf[64];
  snprintf(funds_buf, sizeof(funds_buf), "Funds: $%d", TotalFunds);
  SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
  SDL_RenderFillRect(renderer, &rect_funds_btn);
  SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  SDL_RenderRect(renderer, &rect_funds_btn);
  GUI_DrawText(renderer, (int)rect_funds_btn.x + 8, (int)rect_funds_btn.y + 4, funds_buf, col_gold);

  /* City Name & Pop */
  char city_buf[96];
  snprintf(city_buf, sizeof(city_buf), "%s | Pop: %d", CityName, GUI_CityPopDisplay());
  SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
  SDL_RenderFillRect(renderer, &rect_city_btn);
  SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  SDL_RenderRect(renderer, &rect_city_btn);
  GUI_DrawText(renderer, (int)rect_city_btn.x + 8, (int)rect_city_btn.y + 4, city_buf, col_cyan);

  /* Overlays Button */
  SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
  SDL_RenderFillRect(renderer, &rect_overlays_btn);
  SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  SDL_RenderRect(renderer, &rect_overlays_btn);
  GUI_DrawText(renderer, (int)rect_overlays_btn.x + 14, (int)rect_overlays_btn.y + 4, "Overlays", col_white);

  /* Disasters Button */
  SDL_SetRenderDrawColor(renderer, 48, 30, 36, 255);
  SDL_RenderFillRect(renderer, &rect_disasters_btn);
  SDL_SetRenderDrawColor(renderer, 140, 60, 70, 255);
  SDL_RenderRect(renderer, &rect_disasters_btn);
  GUI_DrawText(renderer, (int)rect_disasters_btn.x + 16, (int)rect_disasters_btn.y + 4, "Disasters", col_gold);

  /* File Button */
  SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
  SDL_RenderFillRect(renderer, &rect_file_btn);
  SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  SDL_RenderRect(renderer, &rect_file_btn);
  GUI_DrawText(renderer, (int)rect_file_btn.x + 26, (int)rect_file_btn.y + 4, "File", col_white);

  /* Speed indicator */
  const char *spd_label = "Speed: [Paused]";
  if (SimSpeed == SIM_SPEED_SUPER_SLOW) spd_label = "Speed: [S.Slow]";
  else if (SimSpeed == SIM_SPEED_SLOW) spd_label = "Speed: [Slow]";
  else if (SimSpeed == SIM_SPEED_MED) spd_label = "Speed: [Med]";
  else if (SimSpeed == SIM_SPEED_FAST) spd_label = "Speed: [Fast]";
  else if (SimSpeed == SIM_SPEED_SUPER_FAST) spd_label = "Speed: [S.Fast]";

  SDL_SetRenderDrawColor(renderer, 36, 44, 60, 255);
  SDL_RenderFillRect(renderer, &rect_speed_btn);
  SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  SDL_RenderRect(renderer, &rect_speed_btn);
  GUI_DrawText(renderer, (int)rect_speed_btn.x + 10, (int)rect_speed_btn.y + 4, spd_label, (SimSpeed == 0) ? col_gold : col_green);

  /* 2. Floating Dismissible Notification Ticker (Relocated Below Status Bar) */
  if (TickerVisible()) {
    uint64_t now = SDL_GetTicks();

    if (MessageSerial != ticker_shown_serial) {
      ticker_shown_serial = MessageSerial;
      ticker_start_time = now;
    }

    uint64_t elapsed = now - ticker_start_time;
    if (elapsed >= TICKER_DURATION_MS) {
      ticker_dismissed_serial = MessageSerial;
    } else {
      uint8_t alpha = 255;
      if (elapsed > (TICKER_DURATION_MS - TICKER_FADE_MS)) {
        uint64_t fade_elapsed = elapsed - (TICKER_DURATION_MS - TICKER_FADE_MS);
        alpha = (uint8_t)(255 - ((fade_elapsed * 255) / TICKER_FADE_MS));
      }

      /* Dynamically fit ticker box width to message length */
      int text_w = GUI_GetTextWidth(LastMessage);
      float box_w = (float)(text_w + 50);
      if (box_w < 260.0f) box_w = 260.0f;
      if (box_w > 640.0f) box_w = 640.0f;
      rect_ticker_box.w = box_w;
      rect_ticker_box.x = ((float)Video_GetCanvasWidth() - box_w) / 2.0f;
      rect_ticker_dismiss.x = rect_ticker_box.x + box_w - 24.0f;

      /* Box background & border */
      SDL_SetRenderDrawColor(renderer, 28, 16, 20, (uint8_t)((alpha * 240) / 255));
      SDL_RenderFillRect(renderer, &rect_ticker_box);

      SDL_SetRenderDrawColor(renderer, 240, 80, 80, alpha);
      SDL_RenderRect(renderer, &rect_ticker_box);

      SDL_Color tick_col = { 255, 230, 130, alpha };
      GUI_DrawText(renderer, (int)rect_ticker_box.x + 10, (int)rect_ticker_box.y + 5, LastMessage, tick_col);

      /* Dismiss Button [x] */
      SDL_SetRenderDrawColor(renderer, 100, 30, 35, alpha);
      SDL_RenderFillRect(renderer, &rect_ticker_dismiss);
      SDL_Color x_col = { 240, 240, 240, alpha };
      GUI_DrawText(renderer, (int)rect_ticker_dismiss.x + 5, (int)rect_ticker_dismiss.y + 2, "x", x_col);
    }
  }

  /* 3. Tool Palette sidebar */
  SDL_FRect tool_bg = { 0.0f, (float)STATUS_BAR_HEIGHT, (float)TOOLBAR_WIDTH, (float)(Video_GetCanvasHeight() - STATUS_BAR_HEIGHT) };
  SDL_SetRenderDrawColor(renderer, 20, 24, 30, 255);
  SDL_RenderFillRect(renderer, &tool_bg);

  SDL_SetRenderDrawColor(renderer, 60, 72, 92, 255);
  SDL_RenderLine(renderer, (float)TOOLBAR_WIDTH, (float)STATUS_BAR_HEIGHT, (float)TOOLBAR_WIDTH, (float)Video_GetCanvasHeight());

  /* Tool Buttons */
  int16_t cur_tool = Input_GetActiveTool();

  for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
    GuiToolButton *btn = &tool_buttons[i];
    bool selected = (btn->tool_id == cur_tool);
    bool hovered = (hovered_tool_idx == (int)i);

    if (selected) {
      SDL_SetRenderDrawColor(renderer, 48, 85, 140, 255);
    } else if (hovered) {
      SDL_SetRenderDrawColor(renderer, 42, 52, 70, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 30, 36, 48, 255);
    }
    SDL_RenderFillRect(renderer, &btn->rect);

    if (selected) {
      SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    } else if (hovered) {
      SDL_SetRenderDrawColor(renderer, 120, 160, 220, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 60, 72, 90, 255);
    }
    SDL_RenderRect(renderer, &btn->rect);

    SDL_Texture *icon = (selected || hovered) ? (btn->tex_hi ? btn->tex_hi : btn->tex_normal) : btn->tex_normal;
    if (icon) {
      SDL_FRect icon_dst = { btn->rect.x + 7.0f, btn->rect.y + 7.0f, 32.0f, 32.0f };
      SDL_RenderTexture(renderer, icon, NULL, &icon_dst);
    }
  }

  /* Scenario editor palette (only while the editor is active) */
  if (Scenario_EditorIsActive()) {
    RenderEditorButtons(renderer);
  }

  /* Query Tool Button (Above RCI) */
  RenderQueryToolButton(renderer);

  /* RCI Demand Gauge (Above Radar Minimap) */
  RenderRciGauge(renderer);

  /* 4. Modal Dialogs */
  GUI_Dialogs_Render(renderer);

  /* 5. Floating Tooltip */
  if (hovered_tool_idx >= 0 && !GUI_IsModalOpen()) {
    RenderTooltip(renderer, hovered_tool_idx);
  }
  RenderSeasonTooltip(renderer, current_month);

  /* 6. Context help overlay (F1) */
  Help_Render(renderer);
}

static bool RectContainsPoint(SDL_FRect r, float x, float y)
{
  return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

HelpTopic GUI_HelpTopicAt(float mouse_x, float mouse_y)
{
  UpdateLayout();
  if (mouse_y <= (float)STATUS_BAR_HEIGHT) {
    if (RectContainsPoint(rect_funds_btn, mouse_x, mouse_y)) return HELP_FUNDS;
    if (RectContainsPoint(rect_city_btn, mouse_x, mouse_y)) return HELP_CITY;
    if (RectContainsPoint(rect_overlays_btn, mouse_x, mouse_y)) return HELP_OVERLAYS;
    if (RectContainsPoint(rect_disasters_btn, mouse_x, mouse_y)) return HELP_DISASTERS;
    if (RectContainsPoint(rect_file_btn, mouse_x, mouse_y)) return HELP_FILE;
    if (RectContainsPoint(rect_speed_btn, mouse_x, mouse_y)) return HELP_SPEED;
    return HELP_NONE;
  }

  if (TickerVisible() && RectContainsPoint(rect_ticker_box, mouse_x, mouse_y)) return HELP_TICKER;
  if (EditorButtonAt(mouse_x, mouse_y) >= 0) return HELP_NONE;
  if (RectContainsPoint(rect_query_btn, mouse_x, mouse_y)) return HELP_QUERY;
  if (RectContainsPoint(rect_rci_box, mouse_x, mouse_y)) return HELP_RCI;
  if (RectContainsPoint(Video_GetMinimapRect(), mouse_x, mouse_y)) return HELP_MINIMAP;

  if (mouse_x <= (float)TOOLBAR_WIDTH) {
    for (size_t i = 0; i < NUM_TOOL_BUTTONS; i++) {
      if (RectContainsPoint(tool_buttons[i].rect, mouse_x, mouse_y)) {
        return (HelpTopic)(HELP_TOOL_RESIDENTIAL + (int)i);
      }
    }
    return HELP_NONE;
  }

  return HELP_MAP;
}
