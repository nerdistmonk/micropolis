/* sdl3_dialog_common.h: Shared layout constants, palettes, and internal modal hooks.
 */

#ifndef SDL3_DIALOG_COMMON_H
#define SDL3_DIALOG_COMMON_H

#include "sdl3_gui.h"
#include "sdl3_gui_dialogs.h"
#include "sdl3_gui_text.h"
#include "sdl3_audio.h"
#include "sdl3_video.h"
#include "sim.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Shared GUI Palette */
static const SDL_Color col_white = { 245, 245, 250, 255 };
static const SDL_Color col_gold  = { 255, 215, 0, 255 };
static const SDL_Color col_green = { 100, 255, 120, 255 };
static const SDL_Color col_cyan  = { 90, 210, 255, 255 };
static const SDL_Color col_red   = { 255, 90, 90, 255 };
static const SDL_Color col_gray  = { 160, 168, 185, 255 };

/* Modal Module Handlers */

/* City Management (Budget, Taxes, Evaluation, Graphs, City Info) */
void Dialog_City_Open(CityMgmtTab tab);
void Dialog_City_Reset(void);
bool Dialog_City_HandleClick(float mx, float my);
bool Dialog_City_HandleKeyDown(SDL_Keycode key);
bool Dialog_City_HandleTextInput(const char *text);
void Dialog_City_Render(SDL_Renderer *renderer);

/* File, Scenario & Options Manager */
void Dialog_File_Open(FileDialogTab tab);
void Dialog_File_Reset(void);
const char *Dialog_File_GetSaveName(void);
bool Dialog_File_HandleClick(float mx, float my);
bool Dialog_File_HandleKeyDown(SDL_Keycode key);
bool Dialog_File_HandleTextInput(const char *text);
void Dialog_File_Render(SDL_Renderer *renderer);

/* Map Overlays */
void Dialog_Overlays_Open(void);
bool Dialog_Overlays_HandleClick(float mx, float my);
void Dialog_Overlays_Render(SDL_Renderer *renderer);

/* Disasters */
void Dialog_Disasters_Open(void);
bool Dialog_Disasters_HandleClick(float mx, float my);
void Dialog_Disasters_Render(SDL_Renderer *renderer);

/* Scenario Result */
void Dialog_Scenario_Show(bool won, const char *msg);
bool Dialog_Scenario_HandleClick(float mx, float my);
void Dialog_Scenario_Render(SDL_Renderer *renderer);

/* Query Inspector */
void Dialog_Inspector_Show(int32_t tx, int32_t ty);
bool Dialog_Inspector_HandleClick(float mx, float my);
void Dialog_Inspector_Render(SDL_Renderer *renderer);

/* Picture Notice (non-modal) */
void Dialog_Notice_Show(int16_t picture_id, int32_t tile_x, int32_t tile_y);
bool Dialog_Notice_HandleClick(float mx, float my);
bool Dialog_Notice_HandleKeyDown(SDL_Keycode key);
void Dialog_Notice_Update(uint64_t now);
void Dialog_Notice_Render(SDL_Renderer *renderer);

/* Quit confirmation */
void Dialog_Quit_Show(void);
bool Dialog_Quit_HandleClick(float mx, float my);
bool Dialog_Quit_TakeConfirmed(void);
void Dialog_Quit_Render(SDL_Renderer *renderer);

#endif /* SDL3_DIALOG_COMMON_H */