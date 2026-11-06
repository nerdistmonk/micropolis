/* sdl3_tiles.h: Tile asset loader and blitter for Micropolis.
 */

#ifndef SDL3_TILES_H
#define SDL3_TILES_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

#define TILE_SHEET_COLS     16
#define TILE_SHEET_ROWS     60
#define TOTAL_TILES         (TILE_SHEET_COLS * TILE_SHEET_ROWS) /* 960 tiles */
#define WATER_TOWER_TILES   9 /* 3x3 water tower block appended after the base sheet */
#define HOSPITAL_SPRITE_TILES 9 /* 3x3 hospital art drawn over the base sheet at HOSPITAL-4 */
#define TOTAL_SHEET_TILES   (TOTAL_TILES + WATER_TOWER_TILES)
#define TILE_W              16
#define TILE_H              16

bool     Tiles_Init(void);
void     Tiles_Shutdown(void);
bool     Tiles_IsLoaded(void);
uint32_t Tiles_GetPixel(uint16_t tile_id, int32_t px, int32_t py);

#endif /* SDL3_TILES_H */