/* sdl3_tiles.c: In-memory XPM decoder and tile pixel blitter for Micropolis.
 *
 * Modernized C11 implementation.
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "sdl3_tiles.h"
#include "sim.h"
#include <strings.h>

extern const char **Micropolis_GetTilesXpmData(void);
extern const char **Micropolis_GetWaterTowerXpmData(void);
extern const char **Micropolis_GetHospitalXpmData(void);

static uint32_t *tile_pixels = NULL;
static int32_t sheet_width = 0;
static int32_t sheet_height = 0;
static bool tiles_ready = false;

typedef struct {
    char symbol[16];
    uint32_t rgba;
} XpmColorMap;

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

    /* SDL_PIXELFORMAT_RGBA32 in memory on Little-Endian x86_64: [R, G, B, A] */
    return (0xFFu << 24) | (b << 16) | (g << 8) | r;
}

static bool ParseXpmSheet(const char **xpm, uint32_t **out_pixels, int32_t *out_width, int32_t *out_height)
{
    if (!xpm) {
        SDL_Log("Error: Embedded XPM data is NULL.");
        return false;
    }

    int width = 0, height = 0, num_colors = 0, cpp = 0;
    if (sscanf(xpm[0], "%d %d %d %d", &width, &height, &num_colors, &cpp) != 4) {
        SDL_Log("Error: Failed to parse XPM header: %s", xpm[0]);
        return false;
    }

    if (cpp <= 0 || cpp >= (int)sizeof(((XpmColorMap *)0)->symbol) || width <= 0 || height <= 0 || num_colors <= 0) {
        SDL_Log("Error: Invalid XPM dimensions: w=%d h=%d colors=%d cpp=%d", width, height, num_colors, cpp);
        return false;
    }

    XpmColorMap *cmap = (XpmColorMap *)calloc((size_t)num_colors, sizeof(XpmColorMap));
    if (!cmap) return false;

    /* Parse color palette */
    for (int i = 0; i < num_colors; i++) {
        const char *entry = xpm[1 + i];
        for (int k = 0; k < cpp; k++) {
            cmap[i].symbol[k] = entry[k];
        }
        cmap[i].symbol[cpp] = '\0';

        const char *c_entry = strstr(entry + cpp, "c ");
        if (!c_entry) c_entry = strstr(entry + cpp, "c\t");
        if (c_entry) {
            c_entry += 2;
            while (*c_entry == ' ' || *c_entry == '\t') c_entry++;

            char col_str[64] = {0};
            int ci = 0;
            while (c_entry[ci] && c_entry[ci] != ' ' && c_entry[ci] != '\t' && ci < 63) {
                col_str[ci] = c_entry[ci];
                ci++;
            }
            col_str[ci] = '\0';

            if (SDL_strncasecmp(col_str, "None", 4) == 0) {
                cmap[i].rgba = 0x00000000;
            } else {
                cmap[i].rgba = ParseHexColor(col_str);
            }
        }
    }

    uint32_t *pixels = (uint32_t *)calloc((size_t)(width * height), sizeof(uint32_t));
    if (!pixels) {
        free(cmap);
        return false;
    }

    /* Parse pixel rows */
    const char **pixel_rows = &xpm[1 + num_colors];
    for (int row = 0; row < height; row++) {
        const char *row_str = pixel_rows[row];
        for (int col = 0; col < width; col++) {
            const char *sym = row_str + (col * cpp);
            uint32_t color = 0xFF000000;

            for (int ci = 0; ci < num_colors; ci++) {
                bool match = true;
                for (int k = 0; k < cpp; k++) {
                    if (cmap[ci].symbol[k] != sym[k]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    color = cmap[ci].rgba;
                    break;
                }
            }
            pixels[row * width + col] = color;
        }
    }

    free(cmap);
    *out_pixels = pixels;
    *out_width = width;
    *out_height = height;
    return true;
}

bool Tiles_Init(void)
{
    uint32_t *tiles_px = NULL;
    uint32_t *tower_px = NULL;
    uint32_t *hosp_px = NULL;
    int32_t tiles_w = 0, tiles_h = 0, tower_w = 0, tower_h = 0, hosp_w = 0, hosp_h = 0;

    if (!ParseXpmSheet(Micropolis_GetTilesXpmData(), &tiles_px, &tiles_w, &tiles_h)) return false;

    if (!ParseXpmSheet(Micropolis_GetWaterTowerXpmData(), &tower_px, &tower_w, &tower_h) || tower_w != tiles_w) {
        SDL_Log("Error: Water tower sheet is missing or does not match the tile width.");
        free(tiles_px);
        free(tower_px);
        return false;
    }

    if (!ParseXpmSheet(Micropolis_GetHospitalXpmData(), &hosp_px, &hosp_w, &hosp_h) ||
        hosp_w != tiles_w || hosp_h != HOSPITAL_SPRITE_TILES * TILE_H) {
        SDL_Log("Error: Hospital sheet is missing or does not match the tile sheet.");
        free(tiles_px);
        free(tower_px);
        free(hosp_px);
        return false;
    }

    /* Hospital art replaces the 3x3 block indexed by HOSPITAL-4..HOSPITAL+4, so zone tile IDs are unchanged. */
    memcpy(tiles_px + (size_t)(HOSPITAL - 4) * TILE_H * (size_t)tiles_w, hosp_px,
           (size_t)hosp_w * (size_t)hosp_h * sizeof(uint32_t));
    free(hosp_px);

    /* Water tower tiles follow the base sheet, so tile IDs 960+ index the appended rows. */
    size_t tiles_count = (size_t)tiles_w * (size_t)tiles_h;
    size_t tower_count = (size_t)tower_w * (size_t)tower_h;
    uint32_t *combined = (uint32_t *)calloc(tiles_count + tower_count, sizeof(uint32_t));
    if (!combined) {
        free(tiles_px);
        free(tower_px);
        return false;
    }
    memcpy(combined, tiles_px, tiles_count * sizeof(uint32_t));
    memcpy(combined + tiles_count, tower_px, tower_count * sizeof(uint32_t));
    free(tiles_px);
    free(tower_px);

    free(tile_pixels);
    tile_pixels = combined;
    sheet_width = tiles_w;
    sheet_height = tiles_h + tower_h;
    tiles_ready = true;

    SDL_Log("Embedded tiles successfully initialized: %dx%d (%d base + %d water tower rows)", tiles_w, tiles_h, tiles_h, tower_h);
    return true;
}

void Tiles_Shutdown(void)
{
    if (tile_pixels) {
        free(tile_pixels);
        tile_pixels = NULL;
    }
    tiles_ready = false;
}

bool Tiles_IsLoaded(void)
{
    return tiles_ready;
}

uint32_t Tiles_GetPixel(uint16_t tile_id, int32_t px, int32_t py)
{
    if (!tiles_ready || !tile_pixels) return 0xFF000000;

    uint16_t id = tile_id & LOMASK;
    if (id >= TOTAL_SHEET_TILES) id = 0;

    int32_t sheet_x, sheet_y;

    if (sheet_width == TILE_W) {
        /* Vertical single-column strip: 16 x (960 * 16) */
        sheet_x = px;
        sheet_y = (id * TILE_H) + py;
    } else {
        /* Multi-column grid (e.g. 256 wide = 16 columns) */
        int32_t cols = sheet_width / TILE_W;
        if (cols <= 0) cols = 1;
        int32_t tile_col = id % cols;
        int32_t tile_row = id / cols;

        sheet_x = (tile_col * TILE_W) + px;
        sheet_y = (tile_row * TILE_H) + py;
    }

    if (sheet_x >= sheet_width || sheet_y >= sheet_height) return 0xFF000000;

    return tile_pixels[sheet_y * sheet_width + sheet_x];
}