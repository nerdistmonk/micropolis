/* sdl3_season.c: Calendar seasons and the eased autumn blend used for tree art.
 */

#include "sdl3_season.h"
#include "sim.h"
#include <stdbool.h>

#define SEASON_YEAR_TICKS 48
#define SEASON_STEPS_PER_MONTH 4
#define SEASON_SLEW_MS 2000u

Season Season_OfMonth(int32_t month)
{
    if (month == 11 || month <= 1) return SEASON_WINTER;
    if (month <= 4) return SEASON_SPRING;
    if (month <= 7) return SEASON_SUMMER;
    return SEASON_AUTUMN;
}

const char *Season_Name(Season season)
{
    switch (season) {
    case SEASON_WINTER: return "Winter";
    case SEASON_SPRING: return "Spring";
    case SEASON_SUMMER: return "Summer";
    case SEASON_AUTUMN: return "Autumn";
    }
    return "";
}

/* Autumn colours ramp in during September and out during March, one step per sim month quarter. */
static uint8_t AutumnTarget(void)
{
    int32_t p = ((CityTime % SEASON_YEAR_TICKS) + SEASON_YEAR_TICKS) % SEASON_YEAR_TICKS;
    int32_t month = p / SEASON_STEPS_PER_MONTH;
    int32_t sub = p % SEASON_STEPS_PER_MONTH;
    int32_t ramp = (sub + 1) * 255 / SEASON_STEPS_PER_MONTH;

    if (month == 8) return (uint8_t)ramp;
    if (month == 2) return (uint8_t)(255 - ramp);
    if (month >= 9 || month <= 1) return 255;
    return 0;
}

/* Mix is kept in 1/256 units so a slow frame rate can't stall the slew on integer rounding. */
static uint32_t shown_q8 = 0;
static uint32_t last_ms = 0;
static bool primed = false;

uint8_t Season_TreeAutumnMix(uint32_t now_ms)
{
    uint32_t target_q8 = (uint32_t)AutumnTarget() * 256u;
    if (!primed) {
        primed = true;
        shown_q8 = target_q8;
        last_ms = now_ms;
    }

    uint32_t dt = now_ms - last_ms;
    last_ms = now_ms;
    uint32_t max_step = dt * 255u * 256u / SEASON_SLEW_MS;

    if (shown_q8 < target_q8) {
        shown_q8 = (target_q8 - shown_q8 > max_step) ? shown_q8 + max_step : target_q8;
    } else if (shown_q8 > target_q8) {
        shown_q8 = (shown_q8 - target_q8 > max_step) ? shown_q8 - max_step : target_q8;
    }

    uint32_t mix = (shown_q8 + 128u) / 256u;
    return (uint8_t)(mix > 255u ? 255u : mix);
}

