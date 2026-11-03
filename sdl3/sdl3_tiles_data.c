/* sdl3_tiles_data.c: Compile-time embedding of Micropolis tiles.xpm
 */

#include <stddef.h>

/* Forward prototype to satisfy -Wmissing-prototypes */
const char **Micropolis_GetTilesXpmData(void);

/* Include the raw XPM file directly as C source array */
#define static
#include "../images/tiles.xpm"
#undef static

/* Export the array symbol cleanly */
const char **Micropolis_GetTilesXpmData(void)
{
    return (const char **)tiles_xpm;
}