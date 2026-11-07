/* sdl3_season.h: Calendar seasons and the eased autumn blend used for tree art.
 */

#ifndef SDL3_SEASON_H
#define SDL3_SEASON_H

#include <stdint.h>

typedef enum {
    SEASON_WINTER,
    SEASON_SPRING,
    SEASON_SUMMER,
    SEASON_AUTUMN
} Season;

Season      Season_OfMonth(int32_t month);
const char *Season_Name(Season season);
uint8_t     Season_TreeAutumnMix(uint32_t now_ms);

#endif /* SDL3_SEASON_H */
