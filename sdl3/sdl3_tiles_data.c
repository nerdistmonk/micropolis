/* sdl3_tiles_data.c: Compile-time embedding of Micropolis tiles.xpm
 */

#include <stddef.h>

/* Forward prototypes to satisfy -Wmissing-prototypes */
const char **Micropolis_GetTilesXpmData(void);
const char **Micropolis_GetWaterTowerXpmData(void);
const char **Micropolis_GetHospitalXpmData(void);
const char **Micropolis_GetLandfillXpmData(void);
const char **Micropolis_GetAutumnTilesXpmData(void);

/* Include the raw XPM files directly as C source arrays */
#define static
#include "../images/tiles.xpm"
#include "../images/tiles_autumn.xpm"
#include "../images/water_tower.xpm"
#include "../images/hospital.xpm"
#include "../images/landfill.xpm"
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

const char **Micropolis_GetHospitalXpmData(void)
{
    return (const char **)hospital_xpm;
}

const char **Micropolis_GetLandfillXpmData(void)
{
    return (const char **)landfill_xpm;
}

const char **Micropolis_GetAutumnTilesXpmData(void)
{
    return (const char **)tiles_autumn_xpm;
}