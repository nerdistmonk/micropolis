/* sdl3_video.c: SDL3 video, windowing, tile rendering, minimap radar, overlays, and sprite blitting.
 */

#include "sdl3_video.h"
#include "sdl3_tiles.h"
#include "sdl3_gui.h"
#include "sdl3_splash.h"
#include "sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *video_window = NULL;
static SDL_Renderer *video_renderer = NULL;
static SDL_Texture *stream_texture = NULL;
static SDL_Texture *canvas_texture = NULL;

static int32_t cam_x = 0;
static int32_t cam_y = 0;
static int32_t win_w = WINDOW_DEFAULT_WIDTH;
static int32_t win_h = WINDOW_DEFAULT_HEIGHT;

static uint32_t *frame_buffer = NULL;

/* Minimap streaming texture (120 x 100) and pixel buffer */
static SDL_Texture *minimap_texture = NULL;
static uint32_t *minimap_pixels = NULL;

/* Minimap layout: 1.0x scale (120x100) docked at the bottom of the left HUD */
#define MINIMAP_SCALE 1.0f
#define MINIMAP_W 120.0f
#define MINIMAP_H 100.0f
#define MINIMAP_X 6.0f
#define MINIMAP_Y (WINDOW_DEFAULT_HEIGHT - MINIMAP_H - 12.0f) /* 656.0f */

#define MAX_SPRITE_FRAMES 16
static SDL_Texture *sprite_textures[OBJN][MAX_SPRITE_FRAMES];
static int sprite_w[OBJN][MAX_SPRITE_FRAMES];
static int sprite_h[OBJN][MAX_SPRITE_FRAMES];

static uint32_t ParseHexColor(const char *hex)
{
  if (!hex) return 0xFF000000;
  while (*hex == ' ' || *hex == '\t') hex++;

  if (hex[0] != '#') {
    if (SDL_strcasecmp(hex, "black") == 0) return 0xFF000000;
    if (SDL_strcasecmp(hex, "white") == 0) return 0xFFFFFFFF;
    if (SDL_strcasecmp(hex, "red") == 0) return 0xFF0000FF;
    if (SDL_strcasecmp(hex, "green") == 0) return 0xFF008000;
    if (SDL_strcasecmp(hex, "blue") == 0) return 0xFFFF0000;
    return 0xFF000000;
  }
  hex++;

  size_t len = strlen(hex);
  uint32_t r = 0, g = 0, b = 0;

  if (len >= 6 && len < 12) {
    unsigned int ri = 0, gi = 0, bi = 0;
    if (sscanf(hex, "%02x%02x%02x", &ri, &gi, &bi) == 3) {
      r = ri; g = gi; b = bi;
    }
  } else if (len >= 12) {
    unsigned int ri = 0, gi = 0, bi = 0;
    if (sscanf(hex, "%04x%04x%04x", &ri, &gi, &bi) == 3) {
      r = ri >> 8; g = gi >> 8; b = bi >> 8;
    }
  }
  return (0xFFu << 24) | (b << 16) | (g << 8) | r;
}

typedef struct {
  char symbol[16];
  uint32_t rgba;
} XpmCol;

static int CompareXpmCol(const void *a, const void *b)
{
  return strcmp(((const XpmCol *)a)->symbol, ((const XpmCol *)b)->symbol);
}

/* Reads one complete line of any length; the caller frees the result. */
static char *ReadXpmLine(FILE *f)
{
  size_t cap = 512, len = 0;
  char *buf = (char *)malloc(cap);
  if (!buf) return NULL;
  while (fgets(buf + len, (int)(cap - len), f)) {
    len += strlen(buf + len);
    if (len > 0 && buf[len - 1] == '\n') return buf;
    if (len + 1 >= cap) {
      size_t ncap = cap * 2;
      char *nb = (char *)realloc(buf, ncap);
      if (!nb) {
        free(buf);
        return NULL;
      }
      buf = nb;
      cap = ncap;
    }
  }
  if (len == 0) {
    free(buf);
    return NULL;
  }
  return buf;
}

static SDL_Texture *LoadXpmFile(SDL_Renderer *renderer, const char *path, int *out_w, int *out_h)
{
  FILE *f = fopen(path, "r");
  if (!f) return NULL;

  char *line = NULL;
  int width = 0, height = 0, num_colors = 0, cpp = 0;
  while ((line = ReadXpmLine(f)) != NULL) {
    char *p = strchr(line, '"');
    int matched = p && sscanf(p + 1, "%d %d %d %d", &width, &height, &num_colors, &cpp) == 4;
    free(line);
    line = NULL;
    if (matched) break;
  }

  if (width <= 0 || height <= 0 || num_colors <= 0 || cpp <= 0 || cpp >= (int)sizeof(((XpmCol *)0)->symbol)) {
    fclose(f);
    return NULL;
  }

  XpmCol *cmap = (XpmCol *)calloc((size_t)num_colors, sizeof(XpmCol));
  if (!cmap) {
    fclose(f);
    return NULL;
  }

  int color_idx = 0;
  while (color_idx < num_colors && (line = ReadXpmLine(f)) != NULL) {
    char *start = strchr(line, '"');
    if (!start) {
      free(line);
      continue;
    }
    start++;
    for (int k = 0; k < cpp; k++) cmap[color_idx].symbol[k] = start[k];
    cmap[color_idx].symbol[cpp] = '\0';

    char *c_pos = strstr(start + cpp, "c ");
    if (!c_pos) c_pos = strstr(start + cpp, "c\t");
    if (c_pos) {
      c_pos += 2;
      while (*c_pos == ' ' || *c_pos == '\t') c_pos++;
      char col_str[64] = {0};
      int ci = 0;
      while (c_pos[ci] && c_pos[ci] != '"' && c_pos[ci] != ' ' && c_pos[ci] != '\t' && ci < 63) {
        col_str[ci] = c_pos[ci];
        ci++;
      }
      col_str[ci] = '\0';
      if (SDL_strncasecmp(col_str, "None", 4) == 0) {
        cmap[color_idx].rgba = 0x00000000;
      } else {
        cmap[color_idx].rgba = ParseHexColor(col_str);
      }
    }
    free(line);
    color_idx++;
  }
  qsort(cmap, (size_t)num_colors, sizeof(XpmCol), CompareXpmCol);

  uint32_t *pixels = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
  if (!pixels) {
    free(cmap);
    fclose(f);
    return NULL;
  }

  int row = 0;
  while (row < height && (line = ReadXpmLine(f)) != NULL) {
    char *start = strchr(line, '"');
    if (!start) {
      free(line);
      continue;
    }
    start++;
    if (strlen(start) < (size_t)width * (size_t)cpp) {
      free(line);
      break;
    }
    for (int col = 0; col < width; col++) {
      XpmCol key;
      memcpy(key.symbol, start + col * cpp, (size_t)cpp);
      key.symbol[cpp] = '\0';
      XpmCol *hit = (XpmCol *)bsearch(&key, cmap, (size_t)num_colors, sizeof(XpmCol), CompareXpmCol);
      pixels[row * width + col] = hit ? hit->rgba : 0x00000000;
    }
    free(line);
    row++;
  }

  fclose(f);
  free(cmap);

  SDL_Surface *surf = SDL_CreateSurfaceFrom(
    width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * (int)sizeof(uint32_t)
  );
  if (!surf) {
    free(pixels);
    return NULL;
  }

  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
  SDL_DestroySurface(surf);
  free(pixels);

  if (tex) {
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
  }
  return tex;
}

SDL_Texture *Video_LoadTexture(const char *path, int *out_w, int *out_h)
{
  return LoadXpmFile(video_renderer, path, out_w, out_h);
}

static void LoadSpriteTextures(SDL_Renderer *renderer)
{
  const char *prefixes[] = { "", "images/", "../images/", "../../images/" };

  int frame_counts[OBJN] = {
    [TRA] = 5,
    [COP] = 8,
    [AIR] = 11,
    [SHI] = 8,
    [GOD] = 16,
    [TOR] = 3,
    [EXP] = 6,
    [BUS] = 4
  };

  for (int type = 1; type < OBJN; type++) {
    for (int frame = 0; frame < frame_counts[type]; frame++) {
      char path[512];
      for (size_t p = 0; p < sizeof(prefixes) / sizeof(prefixes[0]); p++) {
        snprintf(path, sizeof(path), "%sobj%d-%d.xpm", prefixes[p], type, frame);
        sprite_textures[type][frame] = LoadXpmFile(
          renderer, path, &sprite_w[type][frame], &sprite_h[type][frame]
        );
        if (sprite_textures[type][frame]) break;
      }
    }
  }
}

static void FreeSpriteTextures(void)
{
  for (int t = 0; t < OBJN; t++) {
    for (int f = 0; f < MAX_SPRITE_FRAMES; f++) {
      if (sprite_textures[t][f]) {
        SDL_DestroyTexture(sprite_textures[t][f]);
        sprite_textures[t][f] = NULL;
      }
    }
  }
}

/* Shrink the requested window (keeping its aspect ratio) so it fits on the primary display. */
static void FitWindowToDisplay(int32_t *w, int32_t *h)
{
  SDL_Rect usable;
  SDL_DisplayID display = SDL_GetPrimaryDisplay();
  if (display == 0 || !SDL_GetDisplayUsableBounds(display, &usable)) return;
  if (usable.w <= 0 || usable.h <= 0) return;

  float max_w = (float)usable.w * 0.95f;
  float max_h = (float)usable.h * 0.95f;
  float scale = 1.0f;
  if ((float)*w > max_w) scale = max_w / (float)*w;
  if ((float)*h * scale > max_h) scale = max_h / (float)*h;
  if (scale < 1.0f) {
    *w = (int32_t)((float)*w * scale);
    *h = (int32_t)((float)*h * scale);
  }
}

bool Video_Init(int width, int height)
{
  win_w = width;
  win_h = height;

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    SDL_Log("Failed to initialize SDL3 Video: %s", SDL_GetError());
    return false;
  }

  FitWindowToDisplay(&win_w, &win_h);

  video_window = SDL_CreateWindow("Micropolis SDL3", win_w, win_h, SDL_WINDOW_RESIZABLE);
  if (!video_window) return false;

  video_renderer = SDL_CreateRenderer(video_window, NULL);
  if (!video_renderer) return false;

  SDL_SetRenderDrawBlendMode(video_renderer, SDL_BLENDMODE_BLEND);

  /* All drawing targets a 1024x768 canvas, which is scaled once to the window. This keeps
     1px borders on whole canvas pixels instead of fractional physical pixels. */
  canvas_texture = SDL_CreateTexture(
    video_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
    WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT
  );
  if (!canvas_texture) return false;
  SDL_SetTextureBlendMode(canvas_texture, SDL_BLENDMODE_NONE);
  SDL_SetTextureScaleMode(canvas_texture, SDL_SCALEMODE_LINEAR);

  stream_texture = SDL_CreateTexture(
    video_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
    WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT
  );
  if (!stream_texture) return false;

  frame_buffer = (uint32_t *)calloc((size_t)(WINDOW_DEFAULT_WIDTH * WINDOW_DEFAULT_HEIGHT), sizeof(uint32_t));
  if (!frame_buffer) return false;

  minimap_texture = SDL_CreateTexture(
    video_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
    WORLD_X, WORLD_Y
  );
  minimap_pixels = (uint32_t *)calloc((size_t)(WORLD_X * WORLD_Y), sizeof(uint32_t));

  Tiles_Init();
  LoadSpriteTextures(video_renderer);
  return true;
}

void Video_Shutdown(void)
{
  FreeSpriteTextures();
  Tiles_Shutdown();

  if (minimap_pixels) {
    free(minimap_pixels);
    minimap_pixels = NULL;
  }
  if (minimap_texture) {
    SDL_DestroyTexture(minimap_texture);
    minimap_texture = NULL;
  }
  if (frame_buffer) {
    free(frame_buffer);
    frame_buffer = NULL;
  }
  if (stream_texture) {
    SDL_DestroyTexture(stream_texture);
    stream_texture = NULL;
  }
  if (canvas_texture) {
    SDL_DestroyTexture(canvas_texture);
    canvas_texture = NULL;
  }
  if (video_renderer) {
    SDL_DestroyRenderer(video_renderer);
    video_renderer = NULL;
  }
  if (video_window) {
    SDL_DestroyWindow(video_window);
    video_window = NULL;
  }
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Video_HandleWindowEvent(const SDL_Event *event)
{
  if (!event) return;
  if (event->type == SDL_EVENT_WINDOW_RESIZED || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
    win_w = event->window.data1;
    win_h = event->window.data2;
  }
}

void Video_SetCamera(int32_t tile_x, int32_t tile_y)
{
  int32_t max_x = WORLD_X - ((WINDOW_DEFAULT_WIDTH - MAP_ORIGIN_X) / TILE_PIXEL_SIZE);
  int32_t max_y = WORLD_Y - ((WINDOW_DEFAULT_HEIGHT - MAP_ORIGIN_Y) / TILE_PIXEL_SIZE);

  if (max_x < 0) max_x = 0;
  if (max_y < 0) max_y = 0;

  if (tile_x < 0) tile_x = 0;
  if (tile_x > max_x) tile_x = max_x;

  if (tile_y < 0) tile_y = 0;
  if (tile_y > max_y) tile_y = max_y;

  cam_x = tile_x;
  cam_y = tile_y;
}

void Video_GetCamera(int32_t *tile_x, int32_t *tile_y)
{
  if (tile_x) *tile_x = cam_x;
  if (tile_y) *tile_y = cam_y;
}

void Video_PanCamera(int32_t delta_x, int32_t delta_y)
{
  Video_SetCamera(cam_x + delta_x, cam_y + delta_y);
}

void Video_CenterOnTile(int32_t tile_x, int32_t tile_y)
{
  int32_t view_w_tiles = (WINDOW_DEFAULT_WIDTH - MAP_ORIGIN_X) / TILE_PIXEL_SIZE;
  int32_t view_h_tiles = (WINDOW_DEFAULT_HEIGHT - MAP_ORIGIN_Y) / TILE_PIXEL_SIZE;

  Video_SetCamera(tile_x - (view_w_tiles / 2), tile_y - (view_h_tiles / 2));
}

bool Video_ScreenToTile(float screen_x, float screen_y, int32_t *tile_x, int32_t *tile_y)
{
  if (screen_x < (float)TOOLBAR_WIDTH || screen_y < (float)STATUS_BAR_HEIGHT) {
    return false;
  }

  int32_t tx = cam_x + (((int32_t)screen_x - MAP_ORIGIN_X) / TILE_PIXEL_SIZE);
  int32_t ty = cam_y + (((int32_t)screen_y - MAP_ORIGIN_Y) / TILE_PIXEL_SIZE);

  if (!TestBounds(tx, ty)) return false;

  if (tile_x) *tile_x = tx;
  if (tile_y) *tile_y = ty;
  return true;
}

SDL_FRect Video_GetMinimapRect(void)
{
  SDL_FRect r = { MINIMAP_X, MINIMAP_Y, MINIMAP_W, MINIMAP_H };
  return r;
}

bool Video_HandleMinimapClick(float screen_x, float screen_y)
{
  SDL_FRect mr = Video_GetMinimapRect();
  if (screen_x < mr.x || screen_x > (mr.x + mr.w) ||
      screen_y < mr.y || screen_y > (mr.y + mr.h)) {
    return false;
  }

  int32_t target_tx = (int32_t)((screen_x - mr.x) / MINIMAP_SCALE);
  int32_t target_ty = (int32_t)((screen_y - mr.y) / MINIMAP_SCALE);

  Video_CenterOnTile(target_tx, target_ty);
  return true;
}

static void DrawSpriteSlot(SDL_Renderer *renderer, const SimSprite *s, int32_t cam_pixel_x, int32_t cam_pixel_y)
{
  float sx = (float)(s->x + s->x_offset - cam_pixel_x + MAP_ORIGIN_X);
  float sy = (float)(s->y + s->y_offset - cam_pixel_y + MAP_ORIGIN_Y);

  if (sx + 48.0f < (float)MAP_ORIGIN_X || sx > (float)WINDOW_DEFAULT_WIDTH ||
      sy + 48.0f < (float)MAP_ORIGIN_Y || sy > (float)WINDOW_DEFAULT_HEIGHT) {
    return;
  }

  int frame_idx = s->frame - 1;
  if (frame_idx < 0) frame_idx = 0;

  if (sprite_textures[s->type][frame_idx]) {
    SDL_FRect dst = {
      sx, sy,
      (float)sprite_w[s->type][frame_idx],
      (float)sprite_h[s->type][frame_idx]
    };
    SDL_RenderTexture(renderer, sprite_textures[s->type][frame_idx], NULL, &dst);
  }
}

static void RenderSprites(SDL_Renderer *renderer)
{
  int32_t cam_pixel_x = cam_x * TILE_PIXEL_SIZE;
  int32_t cam_pixel_y = cam_y * TILE_PIXEL_SIZE;

  for (int32_t i = 1; i < OBJN; i++) {
    SimSprite *s = GetSprite(i);
    if (!s || s->frame == 0) continue;
    DrawSpriteSlot(renderer, s, cam_pixel_x, cam_pixel_y);
  }

  for (int32_t i = 0; i < EXP_SLOTS; i++) {
    SimSprite *s = GetSpriteSlot(EXP, i);
    if (!s || s->frame == 0) continue;
    DrawSpriteSlot(renderer, s, cam_pixel_x, cam_pixel_y);
  }
}

static bool IsTileFilterOverlay(OverlayViewMode mode)
{
  return mode == OVERLAY_VIEW_RES_ZONE || mode == OVERLAY_VIEW_COM_ZONE ||
         mode == OVERLAY_VIEW_IND_ZONE || mode == OVERLAY_VIEW_TRANSPORT;
}

/* Tile ranges match the Unix g_smmaps.c drawRes/drawCom/drawInd/drawLilTransMap; hidden tiles render as DIRT. */
static bool FilterViewHidesTile(OverlayViewMode mode, uint16_t raw_tile)
{
  uint16_t tile = raw_tile & LOMASK;
  if (tile >= TILE_COUNT) tile -= TILE_COUNT;

  switch (mode) {
  case OVERLAY_VIEW_RES_ZONE:
    return tile > 422;
  case OVERLAY_VIEW_COM_ZONE:
    return (tile > 609) || (tile >= 232 && tile < 423);
  case OVERLAY_VIEW_IND_ZONE:
    return (tile >= 240 && tile <= 611) || (tile >= 693 && tile <= 851) ||
           (tile >= 860 && tile <= 883) || (tile >= 932);
  case OVERLAY_VIEW_TRANSPORT:
    return (tile >= 240) || (tile >= 207 && tile <= 220) || (tile == 223);
  default:
    return false;
  }
}

static uint32_t GetOverlayColorRamp(OverlayViewMode mode, int32_t tx, int32_t ty)
{
  int32_t hx = tx >> 1;
  int32_t hy = ty >> 1;

  switch (mode) {
  case OVERLAY_VIEW_POWER: {
    uint16_t tile = Map[tx][ty];
    QUAD pw = POWERWORD(tx, ty);
    bool has_power = (pw < PWRMAPSIZE) && (PowerMap[pw] & (uint16_t)(1u << (tx & 15)));

    if (has_power) {
      return 0x9000E5FF;
    } else if ((tile & ZONEBIT) && !(tile & PWRBIT)) {
      return 0x852020D0;
    }
    break;
  }

  case OVERLAY_VIEW_TRAFFIC: {
    if (hx < HWLDX && hy < HWLDY) {
      uint8_t t = TrfDensity[hx][hy];
      if (t > 15) {
        uint8_t r = (uint8_t)((t < 128) ? (t * 2) : 255);
        uint8_t g = (uint8_t)((t < 128) ? 240 : (255 - ((t - 128) * 2)));
        uint8_t b = 20;
        uint8_t a = (uint8_t)(120 + (t / 2));
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
      }
    }
    break;
  }

  case OVERLAY_VIEW_POLLUTION: {
    if (hx < HWLDX && hy < HWLDY) {
      uint8_t p = PollutionMem[hx][hy];
      if (p > 10) {
        uint8_t r = (uint8_t)(80 + (p / 2));
        uint8_t g = (uint8_t)((p > 140) ? 20 : (160 - p));
        uint8_t b = (uint8_t)(20 + (p / 2));
        uint8_t a = (uint8_t)(110 + (p / 2));
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
      }
    }
    break;
  }

  case OVERLAY_VIEW_CRIME: {
    if (hx < HWLDX && hy < HWLDY) {
      uint8_t c = CrimeMem[hx][hy];
      if (c > 15) {
        uint8_t r = (uint8_t)(160 + (c / 3));
        uint8_t g = (uint8_t)(30);
        uint8_t b = (uint8_t)(40 + (c / 4));
        uint8_t a = (uint8_t)(120 + (c / 2));
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
      }
    }
    break;
  }

  case OVERLAY_VIEW_LAND_VALUE: {
    if (hx < HWLDX && hy < HWLDY) {
      uint8_t lv = LandValueMem[hx][hy];
      if (lv > 10) {
        uint8_t r = (uint8_t)((lv > 128) ? 255 : (lv * 2));
        uint8_t g = (uint8_t)(180 + (lv / 4));
        uint8_t b = 30;
        uint8_t a = (uint8_t)(100 + (lv / 2));
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
      }
    }
    break;
  }

  case OVERLAY_VIEW_POP_DENSITY: {
    if (hx < HWLDX && hy < HWLDY) {
      uint8_t pd = PopDensity[hx][hy];
      if (pd > 10) {
        uint8_t r = (uint8_t)(40 + (pd / 2));
        uint8_t g = (uint8_t)(60 + (pd / 3));
        uint8_t b = (uint8_t)(180 + (pd / 4));
        uint8_t a = (uint8_t)(110 + (pd / 2));
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
      }
    }
    break;
  }

  case OVERLAY_VIEW_RATE_OF_GROWTH: {
    int32_t bx = tx >> 3;
    int32_t by = ty >> 3;
    if (bx < SmX && by < SmY) {
      int32_t v = RateOGMem[bx][by];
      if (v > 10 || v < -10) {
        int32_t mag = (v < 0) ? -v : v;
        if (mag > 200) mag = 200;
        uint8_t a = (uint8_t)(90 + mag / 2);
        uint8_t r = (uint8_t)((v < 0) ? 230 : 40);
        uint8_t g = (uint8_t)((v < 0) ? 40 : 220);
        uint8_t b = (uint8_t)((v < 0) ? 40 : 80);
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
      }
    }
    break;
  }

  case OVERLAY_VIEW_FIRE_COVERAGE: {
    int32_t bx = tx >> 3;
    int32_t by = ty >> 3;
    if (bx < SmX && by < SmY) {
      int32_t v = FireStMap[bx][by];
      if (v > 0) {
        if (v > 255) v = 255;
        uint8_t a = (uint8_t)(90 + v / 2);
        return ((uint32_t)a << 24) | (20u << 16) | (130u << 8) | 255u;
      }
    }
    break;
  }

  case OVERLAY_VIEW_POLICE_COVERAGE: {
    int32_t bx = tx >> 3;
    int32_t by = ty >> 3;
    if (bx < SmX && by < SmY) {
      int32_t v = PoliceMap[bx][by];
      if (v > 0) {
        if (v > 255) v = 255;
        uint8_t a = (uint8_t)(90 + v / 2);
        return ((uint32_t)a << 24) | (255u << 16) | (120u << 8) | 40u;
      }
    }
    break;
  }

  case OVERLAY_VIEW_NORMAL:
  default:
    break;
  }
  return 0x00000000;
}

static uint32_t BlendOverlayOnPixel(uint32_t overlay_color, uint32_t base_pixel)
{
  if (overlay_color == 0) return base_pixel;

  uint32_t a = (overlay_color >> 24) & 0xFF;
  uint32_t inv_a = 255 - a;

  uint32_t br = base_pixel & 0xFF;
  uint32_t bg = (base_pixel >> 8) & 0xFF;
  uint32_t bb = (base_pixel >> 16) & 0xFF;

  uint32_t or_ = overlay_color & 0xFF;
  uint32_t og = (overlay_color >> 8) & 0xFF;
  uint32_t ob = (overlay_color >> 16) & 0xFF;

  uint32_t final_r = (or_ * a + br * inv_a) / 255;
  uint32_t final_g = (og * a + bg * inv_a) / 255;
  uint32_t final_b = (ob * a + bb * inv_a) / 255;

  return (0xFFu << 24) | (final_b << 16) | (final_g << 8) | final_r;
}

static uint32_t GetTileRadarColor(uint16_t tile, OverlayViewMode mode, int32_t tx, int32_t ty)
{
  uint32_t base_col;
  uint16_t low = tile & LOMASK;

  /* Base geography */
  if (low == RIVER || low == CHANNEL || low == REDGE || (low >= FIRSTRIVEDGE && low <= LASTRIVEDGE)) {
    base_col = 0xFF9E4A28; /* River Blue */
  } else if (low >= TREEBASE && low <= LASTTREE) {
    base_col = 0xFF226E2A; /* Forest Green */
  } else if ((low >= ROADBASE && low <= LASTROAD) || low == HBRIDGE || low == VBRIDGE || low == BRWH || low == BRWV) {
    base_col = 0xFF585858; /* Road Gray */
  } else if (low >= RAILBASE && low <= LASTRAIL) {
    base_col = 0xFF2A4268; /* Rail Brown */
  } else if (low >= POWERBASE && low <= LASTPOWER) {
    base_col = 0xFFB0B0B0; /* Wire Silver */
  } else if ((low >= FIRE && low <= LASTFIRE) || (low >= TINYEXP && low <= LASTTINYEXP)) {
    base_col = 0xFF2020E8; /* Fire Red */
  } else if (low >= FLOOD && low <= LASTFLOOD) {
    base_col = 0xFFD06840; /* Flood Aqua */
  } else if (low == RADTILE) {
    base_col = 0xFF20E040; /* Toxic Green */
  } else if (low >= RESBASE && low < COMBASE) {
    base_col = 0xFF28B8E8; /* Residential Yellow */
  } else if (low >= COMBASE && low < INDBASE) {
    base_col = 0xFFC04030; /* Commercial Blue */
  } else if (low >= INDBASE && low < PORTBASE) {
    base_col = 0xFF2080D8; /* Industrial Orange */
  } else if (low >= PORTBASE && low <= LASTZONE) {
    base_col = 0xFF4040A0; /* Civic Red */
  } else if (low >= RUBBLE && low <= LASTRUBBLE) {
    base_col = 0xFF485868;
  } else {
    base_col = 0xFF6B8A9E; /* Natural dirt */
  }

  if (mode == OVERLAY_VIEW_NORMAL) {
    return base_col;
  }

  uint32_t overlay_col = GetOverlayColorRamp(mode, tx, ty);
  return BlendOverlayOnPixel(overlay_col, base_col);
}

static void UpdateAndRenderMinimap(SDL_Renderer *renderer, OverlayViewMode mode)
{
  if (!minimap_pixels || !minimap_texture) return;

  for (int32_t y = 0; y < WORLD_Y; y++) {
    for (int32_t x = 0; x < WORLD_X; x++) {
      uint16_t tile = Map[x][y];
      minimap_pixels[y * WORLD_X + x] = GetTileRadarColor(tile, mode, x, y);
    }
  }

  SDL_UpdateTexture(minimap_texture, NULL, minimap_pixels, WORLD_X * (int)sizeof(uint32_t));

  /* Minimap frame and background */
  SDL_FRect bg = { MINIMAP_X - 2.0f, MINIMAP_Y - 18.0f, MINIMAP_W + 4.0f, MINIMAP_H + 20.0f };
  SDL_SetRenderDrawColor(renderer, 18, 22, 30, 245);
  SDL_RenderFillRect(renderer, &bg);

  SDL_SetRenderDrawColor(renderer, 70, 85, 115, 255);
  SDL_RenderRect(renderer, &bg);

  SDL_Color col_gold = { 255, 215, 0, 255 };
  GUI_DrawText(renderer, (int)MINIMAP_X, (int)MINIMAP_Y - 16, "RADAR MAP", col_gold);

  SDL_FRect dst = { MINIMAP_X, MINIMAP_Y, MINIMAP_W, MINIMAP_H };
  SDL_RenderTexture(renderer, minimap_texture, NULL, &dst);

  SDL_SetRenderDrawColor(renderer, 80, 95, 125, 255);
  SDL_RenderRect(renderer, &dst);

  /* Render camera viewport rectangle on radar */
  float view_tiles_w = (float)(WINDOW_DEFAULT_WIDTH - MAP_ORIGIN_X) / (float)TILE_PIXEL_SIZE;
  float view_tiles_h = (float)(WINDOW_DEFAULT_HEIGHT - MAP_ORIGIN_Y) / (float)TILE_PIXEL_SIZE;

  float cam_box_x = MINIMAP_X + ((float)cam_x * MINIMAP_SCALE);
  float cam_box_y = MINIMAP_Y + ((float)cam_y * MINIMAP_SCALE);
  float cam_box_w = view_tiles_w * MINIMAP_SCALE;
  float cam_box_h = view_tiles_h * MINIMAP_SCALE;

  SDL_FRect cam_shadow = { cam_box_x - 1.0f, cam_box_y - 1.0f, cam_box_w + 2.0f, cam_box_h + 2.0f };
  SDL_SetRenderDrawColor(renderer, 10, 10, 15, 220);
  SDL_RenderRect(renderer, &cam_shadow);

  SDL_FRect cam_rect = { cam_box_x, cam_box_y, cam_box_w, cam_box_h };
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 240);
  SDL_RenderRect(renderer, &cam_rect);
}

/* Unix w_tool.c toolSize/toolOffset/toolColors, indexed by tool state */
typedef struct {
    int32_t size;
    int32_t offset;
    uint8_t fg;
    uint8_t bg;
} ToolCursorStyle;

static const ToolCursorStyle tool_cursor_style[] = {
    [residentialState] = { 3, 1, COLOR_LIGHTGREEN, COLOR_LIGHTGREEN },
    [commercialState]  = { 3, 1, COLOR_LIGHTBLUE, COLOR_LIGHTBLUE },
    [industrialState]  = { 3, 1, COLOR_YELLOW, COLOR_YELLOW },
    [fireState]        = { 3, 1, COLOR_LIGHTGREEN, COLOR_RED },
    [queryState]       = { 1, 0, COLOR_ORANGE, COLOR_ORANGE },
    [policeState]      = { 3, 1, COLOR_LIGHTGREEN, COLOR_LIGHTBLUE },
    [wireState]        = { 1, 0, COLOR_DARKGRAY, COLOR_YELLOW },
    [dozeState]        = { 1, 0, COLOR_LIGHTBROWN, COLOR_LIGHTBROWN },
    [rrState]          = { 1, 0, COLOR_DARKGRAY, COLOR_OLIVE },
    [roadState]        = { 1, 0, COLOR_DARKGRAY, COLOR_WHITE },
    [stadiumState]     = { 4, 1, COLOR_LIGHTGRAY, COLOR_LIGHTGREEN },
    [parkState]        = { 1, 0, COLOR_LIGHTBROWN, COLOR_LIGHTGREEN },
    [seaportState]     = { 4, 1, COLOR_LIGHTGRAY, COLOR_LIGHTBLUE },
    [powerState]       = { 4, 1, COLOR_LIGHTGRAY, COLOR_YELLOW },
    [nuclearState]     = { 4, 1, COLOR_LIGHTGRAY, COLOR_YELLOW },
    [airportState]     = { 6, 1, COLOR_LIGHTGRAY, COLOR_LIGHTBROWN },
    [networkState]     = { 1, 0, COLOR_LIGHTGRAY, COLOR_RED },
};

/* Unix w_x.c palette */
static const SDL_Color cursor_palette[] = {
    [COLOR_WHITE]      = { 255, 255, 255, 255 },
    [COLOR_YELLOW]     = { 255, 255, 0, 255 },
    [COLOR_ORANGE]     = { 255, 127, 0, 255 },
    [COLOR_RED]        = { 255, 0, 0, 255 },
    [COLOR_DARKRED]    = { 191, 0, 0, 255 },
    [COLOR_DARKBLUE]   = { 0, 0, 230, 255 },
    [COLOR_LIGHTBLUE]  = { 102, 102, 230, 255 },
    [COLOR_BROWN]      = { 204, 76, 76, 255 },
    [COLOR_LIGHTGREEN] = { 0, 230, 0, 255 },
    [COLOR_DARKGREEN]  = { 0, 127, 0, 255 },
    [COLOR_OLIVE]      = { 153, 127, 76, 255 },
    [COLOR_LIGHTBROWN] = { 204, 127, 102, 255 },
    [COLOR_LIGHTGRAY]  = { 191, 191, 191, 255 },
    [COLOR_MEDIUMGRAY] = { 127, 127, 127, 255 },
    [COLOR_DARKGRAY]   = { 63, 63, 63, 255 },
    [COLOR_BLACK]      = { 0, 0, 0, 255 },
};

static MapCursor map_cursor;

void Video_SetMapCursor(const MapCursor *cursor)
{
    if (cursor) map_cursor = *cursor;
}

static void CursorFill(SDL_Renderer *renderer, float x, float y, float w, float h, uint8_t color)
{
    SDL_SetRenderDrawColor(renderer, cursor_palette[color].r, cursor_palette[color].g,
                           cursor_palette[color].b, 255);
    SDL_FRect r = { x, y, w, h };
    SDL_RenderFillRect(renderer, &r);
}

static void CursorHLine(SDL_Renderer *renderer, int32_t x1, int32_t x2, int32_t y, uint8_t color)
{
    int32_t lo = x1 < x2 ? x1 : x2;
    int32_t hi = x1 < x2 ? x2 : x1;
    CursorFill(renderer, (float)lo, (float)y, (float)(hi - lo + 1), 1.0f, color);
}

static void CursorVLine(SDL_Renderer *renderer, int32_t x, int32_t y1, int32_t y2, uint8_t color)
{
    int32_t lo = y1 < y2 ? y1 : y2;
    int32_t hi = y1 < y2 ? y2 : y1;
    CursorFill(renderer, (float)x, (float)lo, 1.0f, (float)(hi - lo + 1), color);
}

static void CursorRectOutline(SDL_Renderer *renderer, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color)
{
    CursorHLine(renderer, x, x + w, y, color);
    CursorHLine(renderer, x, x + w, y + h, color);
    CursorVLine(renderer, x, y, y + h, color);
    CursorVLine(renderer, x + w, y, y + h, color);
}

/* Two-pixel stroke; dashed with 4 on / 4 off in fg over bg, as Unix CursorDashes */
static void CursorStrokeH(SDL_Renderer *renderer, int32_t x1, int32_t x2, int32_t y,
                          uint8_t fg, uint8_t bg)
{
    int32_t lo = x1 < x2 ? x1 : x2;
    int32_t hi = x1 < x2 ? x2 : x1;
    for (int32_t row = y; row <= y + 1; row++) {
        if (fg == bg) {
            CursorHLine(renderer, lo, hi, row, fg);
            continue;
        }
        CursorHLine(renderer, lo, hi, row, bg);
        for (int32_t x = lo; x <= hi; x += 8) {
            int32_t end = (x + 3 < hi) ? x + 3 : hi;
            CursorHLine(renderer, x, end, row, fg);
        }
    }
}

static void CursorStrokeV(SDL_Renderer *renderer, int32_t x, int32_t y1, int32_t y2,
                          uint8_t fg, uint8_t bg)
{
    int32_t lo = y1 < y2 ? y1 : y2;
    int32_t hi = y1 < y2 ? y2 : y1;
    for (int32_t col = x; col <= x + 1; col++) {
        if (fg == bg) {
            CursorVLine(renderer, col, lo, hi, fg);
            continue;
        }
        CursorVLine(renderer, col, lo, hi, bg);
        for (int32_t y = lo; y <= hi; y += 8) {
            int32_t end = (y + 3 < hi) ? y + 3 : hi;
            CursorVLine(renderer, col, y, end, fg);
        }
    }
}

/* Mirrors the default branch of Unix DrawCursor: a tool-sized box snapped to the pointer tile */
static void RenderToolOutline(SDL_Renderer *renderer, const MapCursor *cursor)
{
    if (cursor->tool < firstState || cursor->tool > lastState) return;

    const ToolCursorStyle *style = &tool_cursor_style[cursor->tool];
    int32_t size = style->size * TILE_PIXEL_SIZE;
    int32_t x = (cursor->tile_x - style->offset - cam_x) * TILE_PIXEL_SIZE + MAP_ORIGIN_X;
    int32_t y = (cursor->tile_y - style->offset - cam_y) * TILE_PIXEL_SIZE + MAP_ORIGIN_Y;

    uint8_t dark = COLOR_BLACK;
    uint8_t light = COLOR_WHITE;
    if (cursor->tool_pressed) {
        uint8_t swap = dark;
        dark = light;
        light = swap;
    }

    CursorRectOutline(renderer, x - 1, y - 1, size + 4, size + 4, dark);
    CursorHLine(renderer, x - 3, x - 1, y + size + 3, dark);
    CursorVLine(renderer, x + size + 3, y - 3, y - 1, dark);

    CursorRectOutline(renderer, x - 4, y - 4, size + 4, size + 4, light);
    CursorVLine(renderer, x - 4, y + size + 1, y + size + 3, light);
    CursorHLine(renderer, x + size + 1, x + size + 3, y - 4, light);

    CursorStrokeV(renderer, x - 2, y - 1, y + size + 3, style->fg, style->bg);
    CursorStrokeH(renderer, x - 1, x + size + 3, y + size + 2, style->fg, style->bg);
    CursorStrokeV(renderer, x + size + 2, y + size + 1, y - 3, style->fg, style->bg);
    CursorStrokeH(renderer, x + size + 1, x - 3, y - 2, style->fg, style->bg);
}

/* Mirrors the pan branch of Unix DrawCursor: a black-edged white cross at the pan anchor */
static void RenderPanCrosshair(SDL_Renderer *renderer, const MapCursor *cursor)
{
    float x = (float)(cursor->pan_px - cam_x * TILE_PIXEL_SIZE + MAP_ORIGIN_X);
    float y = (float)(cursor->pan_py - cam_y * TILE_PIXEL_SIZE + MAP_ORIGIN_Y);

    SDL_SetRenderDrawColor(renderer, cursor_palette[COLOR_BLACK].r, cursor_palette[COLOR_BLACK].g,
                           cursor_palette[COLOR_BLACK].b, 255);
    for (int32_t d = -1; d <= 1; d++) {
        SDL_RenderLine(renderer, x - 6 + d, y - 6, x + 6 + d, y + 6);
        SDL_RenderLine(renderer, x - 6 + d, y + 6, x + 6 + d, y - 6);
        SDL_RenderLine(renderer, x - 8, y + d, x + 8, y + d);
        SDL_RenderLine(renderer, x + d, y - 8, x + d, y + 8);
    }

    SDL_SetRenderDrawColor(renderer, cursor_palette[COLOR_WHITE].r, cursor_palette[COLOR_WHITE].g,
                           cursor_palette[COLOR_WHITE].b, 255);
    SDL_RenderLine(renderer, x - 6, y - 6, x + 6, y + 6);
    SDL_RenderLine(renderer, x - 6, y + 6, x + 6, y - 6);
    SDL_RenderLine(renderer, x - 8, y, x + 8, y);
    SDL_RenderLine(renderer, x, y - 8, x, y + 8);
}

static void RenderMapCursor(SDL_Renderer *renderer)
{
    SDL_Color saved = { 0, 0, 0, 255 };
    SDL_GetRenderDrawColor(renderer, &saved.r, &saved.g, &saved.b, &saved.a);
    SDL_BlendMode saved_blend;
    SDL_GetRenderDrawBlendMode(renderer, &saved_blend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    if (map_cursor.pan_visible) {
        RenderPanCrosshair(renderer, &map_cursor);
    } else if (map_cursor.tool_visible) {
        RenderToolOutline(renderer, &map_cursor);
    }

    SDL_SetRenderDrawBlendMode(renderer, saved_blend);
    SDL_SetRenderDrawColor(renderer, saved.r, saved.g, saved.b, saved.a);
}

/* Unix StopEarthquake timer: the shake ends 3000 ms after the most recent quake */
#define QUAKE_SHAKE_MS 3000

static int16_t shake_seen_quakes = 0;
static uint64_t shake_deadline_ms = 0;

/* Largest 1024x768-aspect rect that fits (out_w, out_h), centred. */
static void ComputeLetterbox(float out_w, float out_h, SDL_FRect *dst, float *scale)
{
  float s = out_w / (float)WINDOW_DEFAULT_WIDTH;
  float s_h = out_h / (float)WINDOW_DEFAULT_HEIGHT;
  if (s_h < s) s = s_h;
  if (s < 0.0f) s = 0.0f;

  dst->w = (float)WINDOW_DEFAULT_WIDTH * s;
  dst->h = (float)WINDOW_DEFAULT_HEIGHT * s;
  dst->x = (out_w - dst->w) * 0.5f;
  dst->y = (out_h - dst->h) * 0.5f;
  *scale = s;
}

static void PresentCanvas(void)
{
  SDL_SetRenderTarget(video_renderer, NULL);
  SDL_SetRenderDrawColor(video_renderer, 0, 0, 0, 255);
  SDL_RenderClear(video_renderer);

  int out_pw = 0;
  int out_ph = 0;
  SDL_GetRenderOutputSize(video_renderer, &out_pw, &out_ph);

  SDL_FRect dst;
  float scale = 0.0f;
  ComputeLetterbox((float)out_pw, (float)out_ph, &dst, &scale);
  SDL_RenderTexture(video_renderer, canvas_texture, NULL, &dst);
  SDL_RenderPresent(video_renderer);
}

void Video_SetLinearFilter(bool linear)
{
  if (!canvas_texture) return;
  SDL_SetTextureScaleMode(canvas_texture, linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
}

void Video_ConvertEventToCanvas(SDL_Event *event)
{
  if (!video_window) return;

  int win_pw = 0;
  int win_ph = 0;
  SDL_GetWindowSize(video_window, &win_pw, &win_ph);

  SDL_FRect dst;
  float scale = 0.0f;
  ComputeLetterbox((float)win_pw, (float)win_ph, &dst, &scale);
  if (scale <= 0.0f) return;

  float inv = 1.0f / scale;
  switch (event->type) {
  case SDL_EVENT_MOUSE_MOTION:
    event->motion.x = (event->motion.x - dst.x) * inv;
    event->motion.y = (event->motion.y - dst.y) * inv;
    event->motion.xrel *= inv;
    event->motion.yrel *= inv;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
    event->button.x = (event->button.x - dst.x) * inv;
    event->button.y = (event->button.y - dst.y) * inv;
    break;
  case SDL_EVENT_MOUSE_WHEEL:
    event->wheel.mouse_x = (event->wheel.mouse_x - dst.x) * inv;
    event->wheel.mouse_y = (event->wheel.mouse_y - dst.y) * inv;
    break;
  default:
    break;
  }
}

void Video_RenderFrame(void)
{
  if (!frame_buffer || !video_renderer || !stream_texture || !canvas_texture) return;

  SDL_SetRenderTarget(video_renderer, canvas_texture);

  if (Splash_IsActive()) {
    Splash_Render(video_renderer);
    PresentCanvas();
    return;
  }

  bool has_tiles = Tiles_IsLoaded();
  bool show_bolt = (flagBlink <= 0);
  OverlayViewMode overlay_mode = GUI_GetActiveOverlayMode();

  /* Pixels left of or above the map origin sit under the HUD; they get tile index -1 and draw black */
  for (int32_t sy = 0; sy < WINDOW_DEFAULT_HEIGHT; sy++) {
    int32_t my = sy - MAP_ORIGIN_Y;
    int32_t ty = (my < 0) ? -1 : cam_y + (my / TILE_PIXEL_SIZE);
    int32_t py = (my < 0) ? 0 : my % TILE_PIXEL_SIZE;

    for (int32_t sx = 0; sx < WINDOW_DEFAULT_WIDTH; sx++) {
      int32_t mx = sx - MAP_ORIGIN_X;
      int32_t tx = (mx < 0) ? -1 : cam_x + (mx / TILE_PIXEL_SIZE);
      int32_t px = (mx < 0) ? 0 : mx % TILE_PIXEL_SIZE;
      uint32_t pixel;

      if (tx >= 0 && tx < WORLD_X && ty >= 0 && ty < WORLD_Y) {
        uint16_t raw_tile = Map[tx][ty];

        if (IsTileFilterOverlay(overlay_mode)) {
          if (FilterViewHidesTile(overlay_mode, raw_tile)) {
            raw_tile = DIRT;
          }
        } else if (show_bolt && (raw_tile & ZONEBIT) && !(raw_tile & PWRBIT)) {
          raw_tile = LIGHTNINGBOLT;
        }

        if (has_tiles) {
          pixel = Tiles_GetPixel(raw_tile, px, py);
        } else {
          pixel = 0xFF000000;
        }

        if (overlay_mode != OVERLAY_VIEW_NORMAL) {
          uint32_t overlay_col = GetOverlayColorRamp(overlay_mode, tx, ty);
          pixel = BlendOverlayOnPixel(overlay_col, pixel);
        }
      } else {
        pixel = 0xFF000000;
      }

      frame_buffer[sy * WINDOW_DEFAULT_WIDTH + sx] = pixel;
    }
  }

  SDL_UpdateTexture(stream_texture, NULL, frame_buffer, WINDOW_DEFAULT_WIDTH * (int)sizeof(uint32_t));

  SDL_SetRenderDrawColor(video_renderer, 0, 0, 0, 255);
  SDL_RenderClear(video_renderer);

  uint64_t now_ms = SDL_GetTicks();
  if (ShakeNow != shake_seen_quakes) {
    shake_seen_quakes = ShakeNow;
    shake_deadline_ms = now_ms + QUAKE_SHAKE_MS;
  }
  if (ShakeNow > 0 && now_ms >= shake_deadline_ms) {
    ShakeNow = 0;
    shake_seen_quakes = 0;
  }

  SDL_FRect map_dest = { 0.0f, 0.0f, (float)WINDOW_DEFAULT_WIDTH, (float)WINDOW_DEFAULT_HEIGHT };
  if (ShakeNow > 0) {
    map_dest.x += (float)(Rand(9) - 4);
    map_dest.y += (float)(Rand(9) - 4);
  }

  SDL_RenderTexture(video_renderer, stream_texture, NULL, &map_dest);

  if (overlay_mode == OVERLAY_VIEW_NORMAL) {
    RenderSprites(video_renderer);
  }

  RenderMapCursor(video_renderer);

  GUI_Render(video_renderer);
  UpdateAndRenderMinimap(video_renderer, overlay_mode);

  PresentCanvas();
}

SDL_Renderer *Video_GetRenderer(void)
{
  return video_renderer;
}

SDL_Window *Video_GetWindow(void)
{
  return video_window;
}