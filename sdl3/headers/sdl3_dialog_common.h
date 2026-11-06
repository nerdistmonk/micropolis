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

/* Top-left origin that centers a dialog of the given size on the live canvas. */
static inline float DialogCenteredX(float width)
{
    return ((float)Video_GetCanvasWidth() - width) / 2.0f;
}

static inline float DialogCenteredY(float height)
{
    return ((float)Video_GetCanvasHeight() - height) / 2.0f;
}

/* Modal Module Handlers */

/* City Management (Budget, Taxes, Evaluation, Graphs, City Info) */
void Dialog_City_Open(CityMgmtTab tab);
void Dialog_City_Reset(void);
bool Dialog_City_HandleClick(float mx, float my);
bool Dialog_City_HandleKeyDown(SDL_Keycode key);
bool Dialog_City_HandleTextInput(const char *text);
void Dialog_City_Render(SDL_Renderer *renderer);

/* File, Scenario & Options Manager */
typedef enum {
    NATIVE_SAVE_FILE,
    NATIVE_LOAD_FILE,
    NATIVE_SAVE_QUIT
} NativeFileRequest;

void Dialog_File_Open(FileDialogTab tab);
bool Dialog_File_HandleClick(float mx, float my);
void Dialog_File_Render(SDL_Renderer *renderer);
void Dialog_File_RequestNative(NativeFileRequest req);
bool Dialog_File_HandleNativeEvent(const SDL_Event *event);
bool Dialog_File_NativePending(void);

/* Load Scenario list (replaces the Save / Load tab body while open) */
void ScnList_Open(void);
void ScnList_Close(void);
bool ScnList_IsOpen(void);
void ScnList_Render(SDL_Renderer *renderer, float dx, float dy);
bool ScnList_HandleClick(float mx, float my, float dx, float dy);

/* Scenario editor: entry (after the choose-another-city confirmation) and settings modal */
void Dialog_Editor_Start(void);
void Dialog_Editor_Open(void);
void Dialog_Editor_Reset(void);
bool Dialog_Editor_HandleClick(float mx, float my);
bool Dialog_Editor_HandleKeyDown(SDL_Keycode key);
bool Dialog_Editor_HandleTextInput(const char *text);
void Dialog_Editor_Render(SDL_Renderer *renderer);

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
void Dialog_Quit_SaveFinished(bool saved, const char *name);
void Dialog_Quit_Render(SDL_Renderer *renderer);

/* Choose another city confirmation (gates New City and scenario load) */
void Dialog_ChooseCity_Show(void (*on_confirm)(int32_t), int32_t arg);
void Dialog_ChooseCity_Reset(void);
bool Dialog_ChooseCity_HandleClick(float mx, float my);
void Dialog_ChooseCity_Render(SDL_Renderer *renderer);

#endif /* SDL3_DIALOG_COMMON_H */