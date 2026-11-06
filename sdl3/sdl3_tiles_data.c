/* sdl3_tiles_data.c: Compile-time embedding of Micropolis tiles.xpm
 */

#include <stddef.h>

/* Forward prototypes to satisfy -Wmissing-prototypes */
const char **Micropolis_GetTilesXpmData(void);
const char **Micropolis_GetWaterTowerXpmData(void);

/* Include the raw XPM files directly as C source arrays */
#define static
#include "../images/tiles.xpm"
#include "../images/water_tower.xpm"
#undef static

/* Export the array symbols cleanly */
const char **Micropolis_GetTilesXpmData(void)
{
    return (const char **)tiles_xpm;
}

const char **Micropolis_GetWaterTowerXpmData(void)
{
    return (const char **)water_tower_xpm;
}