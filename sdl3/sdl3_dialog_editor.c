/* sdl3_dialog_editor.c: Scenario editor entry, settings modal, and palette save/step/exit.
 */

#include "sdl3_dialog_common.h"
#include "sdl3_gui.h"
#include "sdl3_input.h"
#include "scenario.h"
#include <stdlib.h>
#include <string.h>

#define MODAL_W         600.0f
#define MODAL_H         580.0f
#define EDIT_BUF_BYTES  SCN_DESC_BYTES
#define NUMERIC_CHARS   9
#define MSG_BYTES       160

typedef enum {
    FIELD_NONE = 0,
    FIELD_NAME,
    FIELD_DESC,
    FIELD_START,
    FIELD_FUNDS,
    FIELD_DEADLINE,
    FIELD_WIN_VALUE,
    FIELD_LOSE_VALUE
} EditField;

static EditField focus_kind = FIELD_NONE;
static int focus_index = 0;
static char edit_buf[EDIT_BUF_BYTES];
static char editor_message[MSG_BYTES];
static bool editor_message_is_error = false;

static SDL_FRect MakeRect(float x, float y, float w, float h)
{
    SDL_FRect r = { x, y, w, h };
    return r;
}

static bool PointIn(SDL_FRect r, float x, float y)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

static bool IsNumeric(EditField kind)
{
    return kind != FIELD_NAME && kind != FIELD_DESC;
}

/* Byte capacity of the edit buffer for a field, excluding the NUL. */
static size_t CapacityOf(EditField kind)
{
    switch (kind) {
    case FIELD_NAME: return SCN_NAME_BYTES - 1;
    case FIELD_DESC: return SCN_DESC_BYTES - 1;
    default:         return NUMERIC_CHARS;
    }
}

static void SetEditorMessage(bool error, const char *text)
{
    snprintf(editor_message, sizeof(editor_message), "%s", text);
    editor_message_is_error = error;
}

static void StopEditing(void)
{
    if (focus_kind != FIELD_NONE) SDL_StopTextInput(Video_GetWindow());
    focus_kind = FIELD_NONE;
    focus_index = 0;
    edit_buf[0] = '\0';
}

static void FormatField(EditField kind, int index, const ScenarioDef *def, char *out, size_t cap)
{
    switch (kind) {
    case FIELD_NAME:       snprintf(out, cap, "%s", def->name); break;
    case FIELD_DESC:       snprintf(out, cap, "%s", def->description); break;
    case FIELD_START:      snprintf(out, cap, "%d", (int)def->start_year); break;
    case FIELD_FUNDS:      snprintf(out, cap, "%d", (int)def->funds); break;
    case FIELD_DEADLINE:   snprintf(out, cap, "%d", (int)def->deadline_year); break;
    case FIELD_WIN_VALUE:  snprintf(out, cap, "%d", (int)def->win[index].value); break;
    case FIELD_LOSE_VALUE: snprintf(out, cap, "%d", (int)def->lose[index].value); break;
    default:               out[0] = '\0'; break;
    }
}

/* Empty numeric fields read as 0, which means "none" for the deadline. */
static int32_t ParseNumber(void)
{
    return edit_buf[0] ? (int32_t)strtol(edit_buf, NULL, 10) : 0;
}

/* Applies the focused field to the editor definition. Returns false, keeping focus, when the value is invalid. */
static bool CommitField(void)
{
    if (focus_kind == FIELD_NONE) return true;

    ScenarioDef *def = Scenario_EditorDef();
    if (!def) {
        StopEditing();
        return true;
    }

    switch (focus_kind) {
    case FIELD_NAME:
        if (edit_buf[0] == '\0') {
            SetEditorMessage(true, "The scenario needs a name.");
            return false;
        }
        snprintf(def->name, sizeof(def->name), "%.*s", SCN_NAME_BYTES - 1, edit_buf);
        break;
    case FIELD_DESC:
        snprintf(def->description, sizeof(def->description), "%s", edit_buf);
        break;
    case FIELD_START: {
        int32_t year = ParseNumber();
        if (year < SCN_FIRST_YEAR || year > SCN_LAST_YEAR) {
            SetEditorMessage(true, "Start year must be between 1900 and 2100.");
            return false;
        }
        Scenario_EditorSetStartYear(year);
        break;
    }
    case FIELD_FUNDS:
        Scenario_EditorSetFunds(ParseNumber());
        break;
    case FIELD_DEADLINE: {
        int32_t year = ParseNumber();
        if (year != 0 && (year < SCN_FIRST_YEAR || year > SCN_LAST_YEAR)) {
            SetEditorMessage(true, "Deadline must be 0 (none) or a year from 1900 to 2100.");
            return false;
        }
        def->deadline_year = year;
        break;
    }
    case FIELD_WIN_VALUE:
        def->win[focus_index].value = ParseNumber();
        break;
    case FIELD_LOSE_VALUE:
        def->lose[focus_index].value = ParseNumber();
        break;
    default:
        break;
    }

    StopEditing();
    return true;
}

static void BeginEditing(EditField kind, int index)
{
    const ScenarioDef *def = Scenario_EditorDef();
    if (!def) return;

    focus_kind = kind;
    focus_index = index;
    FormatField(kind, index, def, edit_buf, sizeof(edit_buf));
    SDL_StartTextInput(Video_GetWindow());
}

/* Moves editing to another field. Stays put if the current field is invalid. */
static void FocusField(EditField kind, int index)
{
    if (focus_kind == kind && focus_index == index) return;
    if (!CommitField()) return;
    BeginEditing(kind, index);
}

static void DropLastChar(char *s)
{
    size_t len = strlen(s);
    if (len == 0) return;
    len--;
    while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80) len--;
    s[len] = '\0';
}

/* Appends typed text, keeping whole UTF-8 characters and the field's byte capacity. */
static void AppendText(const char *text)
{
    size_t cap = CapacityOf(focus_kind);
    bool numeric = IsNumeric(focus_kind);

    for (const unsigned char *p = (const unsigned char *)text; *p; ) {
        size_t n = 1;
        if (*p >= 0xF0) n = 4;
        else if (*p >= 0xE0) n = 3;
        else if (*p >= 0xC0) n = 2;

        if (*p < 0x20 || (numeric && (*p < '0' || *p > '9'))) {
            p++;
            continue;
        }

        size_t len = strlen(edit_buf);
        if (len + n > cap) break;
        memcpy(edit_buf + len, p, n);
        edit_buf[len + n] = '\0';
        p += n;
    }
}

static bool SaveScenario(char *out, size_t cap)
{
    char path[1280];
    const char *err = Scenario_EditorSave(path, sizeof(path));
    if (err) {
        snprintf(out, cap, "%s", err);
        return false;
    }
    snprintf(out, cap, "%s", "Scenario saved to the scenarios folder.");
    return true;
}

static void SaveFromSetup(void)
{
    char text[MSG_BYTES];
    bool ok = SaveScenario(text, sizeof(text));
    SetEditorMessage(!ok, text);
    GUI_ShowStatus(text);
}

/* Layout. Coordinates are relative to the modal's top-left corner (dx, dy). */

static float ColX(float dx, bool lose)
{
    return dx + (lose ? 316.0f : 14.0f);
}

static float RowY(float dy, int index)
{
    return dy + 200.0f + (float)index * 30.0f;
}

static SDL_FRect CloseXRect(float dx, float dy)  { return MakeRect(dx + MODAL_W - 28.0f, dy + 8.0f, 20.0f, 20.0f); }
static SDL_FRect NameBox(float dx, float dy)     { return MakeRect(dx + 112.0f, dy + 44.0f, 468.0f, 24.0f); }
static SDL_FRect DescBox(float dx, float dy)     { return MakeRect(dx + 112.0f, dy + 74.0f, 468.0f, 24.0f); }
static SDL_FRect StartBox(float dx, float dy)    { return MakeRect(dx + 112.0f, dy + 104.0f, 70.0f, 24.0f); }
static SDL_FRect FundsBox(float dx, float dy)    { return MakeRect(dx + 240.0f, dy + 104.0f, 120.0f, 24.0f); }
static SDL_FRect DeadlineBox(float dx, float dy) { return MakeRect(dx + 440.0f, dy + 104.0f, 70.0f, 24.0f); }
static SDL_FRect DisasterBox(float dx, float dy) { return MakeRect(dx + 112.0f, dy + 134.0f, 150.0f, 24.0f); }
static SDL_FRect SaveRect(float dx, float dy)    { return MakeRect(dx + 14.0f, dy + 524.0f, 150.0f, 30.0f); }
static SDL_FRect CloseRect(float dx, float dy)   { return MakeRect(dx + MODAL_W - 104.0f, dy + 524.0f, 90.0f, 30.0f); }

static SDL_FRect MatchRect(float cx, float dy)   { return MakeRect(cx + 60.0f, dy + 166.0f, 110.0f, 22.0f); }
static SDL_FRect MetricRect(float cx, float y)   { return MakeRect(cx, y, 112.0f, 24.0f); }
static SDL_FRect OpRect(float cx, float y)       { return MakeRect(cx + 116.0f, y, 40.0f, 24.0f); }
static SDL_FRect ValueRect(float cx, float y)    { return MakeRect(cx + 160.0f, y, 90.0f, 24.0f); }
static SDL_FRect RemoveRect(float cx, float y)   { return MakeRect(cx + 254.0f, y, 16.0f, 24.0f); }
static SDL_FRect AddRect(float cx, float dy, int count) { return MakeRect(cx, RowY(dy, count), 110.0f, 24.0f); }

static void RemoveCondition(ScenarioCondition *list, uint8_t *count, int index)
{
    for (int i = index; i + 1 < *count; i++) list[i] = list[i + 1];
    (*count)--;
    memset(&list[*count], 0, sizeof(list[0]));
}

/* Handles a click on one win or lose column. Returns true when the click landed in it. */
static bool ConditionClick(ScenarioDef *def, float dx, float dy, bool lose, float mx, float my)
{
    float cx = ColX(dx, lose);
    ScenarioCondition *list = lose ? def->lose : def->win;
    uint8_t *count = lose ? &def->lose_count : &def->win_count;
    uint8_t *match_all = lose ? &def->lose_all : &def->win_all;
    EditField value_kind = lose ? FIELD_LOSE_VALUE : FIELD_WIN_VALUE;

    if (PointIn(MatchRect(cx, dy), mx, my)) {
        if (CommitField()) *match_all = *match_all ? 0 : 1;
        return true;
    }
    for (int i = 0; i < *count; i++) {
        float y = RowY(dy, i);
        if (PointIn(MetricRect(cx, y), mx, my)) {
            if (CommitField()) list[i].metric = (uint8_t)((list[i].metric + 1) % SCN_METRIC_COUNT);
            return true;
        }
        if (PointIn(OpRect(cx, y), mx, my)) {
            if (CommitField()) list[i].op = (uint8_t)((list[i].op + 1) % SCN_OP_COUNT);
            return true;
        }
        if (PointIn(ValueRect(cx, y), mx, my)) {
            FocusField(value_kind, i);
            return true;
        }
        if (PointIn(RemoveRect(cx, y), mx, my)) {
            if (CommitField()) RemoveCondition(list, count, i);
            return true;
        }
    }
    if (*count < SCN_MAX_CONDITIONS && PointIn(AddRect(cx, dy, *count), mx, my)) {
        if (CommitField()) {
            list[*count].metric = SCN_METRIC_CITY_CLASS;
            list[*count].op = SCN_OP_AT_LEAST;
            list[*count].value = 1;
            (*count)++;
        }
        return true;
    }
    return false;
}

/* Modal lifecycle and input */

void Dialog_Editor_Open(void)
{
    StopEditing();
    SetEditorMessage(false, "Set the name and rules, then Save Scenario.");
}

void Dialog_Editor_Reset(void)
{
    StopEditing();
}

bool Dialog_Editor_HandleKeyDown(SDL_Keycode key)
{
    if (focus_kind == FIELD_NONE) return false;

    if (key == SDLK_ESCAPE) {
        StopEditing();
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        (void)CommitField();
    } else if (key == SDLK_BACKSPACE) {
        DropLastChar(edit_buf);
    }
    return true;
}

bool Dialog_Editor_HandleTextInput(const char *text)
{
    if (text && focus_kind != FIELD_NONE) AppendText(text);
    return true;
}

bool Dialog_Editor_HandleClick(float mx, float my)
{
    ScenarioDef *def = Scenario_EditorDef();
    if (!def) return true;

    float dx = DialogCenteredX(MODAL_W);
    float dy = DialogCenteredY(MODAL_H);

    if (!PointIn(MakeRect(dx, dy, MODAL_W, MODAL_H), mx, my)) return true;

    if (PointIn(CloseXRect(dx, dy), mx, my) || PointIn(CloseRect(dx, dy), mx, my)) {
        if (CommitField()) GUI_Dialogs_Close();
        return true;
    }
    if (PointIn(SaveRect(dx, dy), mx, my)) {
        if (CommitField()) SaveFromSetup();
        return true;
    }

    if (PointIn(NameBox(dx, dy), mx, my))     { FocusField(FIELD_NAME, 0); return true; }
    if (PointIn(DescBox(dx, dy), mx, my))     { FocusField(FIELD_DESC, 0); return true; }
    if (PointIn(StartBox(dx, dy), mx, my))    { FocusField(FIELD_START, 0); return true; }
    if (PointIn(FundsBox(dx, dy), mx, my))    { FocusField(FIELD_FUNDS, 0); return true; }
    if (PointIn(DeadlineBox(dx, dy), mx, my)) { FocusField(FIELD_DEADLINE, 0); return true; }

    if (PointIn(DisasterBox(dx, dy), mx, my)) {
        if (CommitField()) {
            def->disasters = def->disasters ? 0 : 1;
            NoDisasters = def->disasters ? 0 : 1;
        }
        return true;
    }

    if (ConditionClick(def, dx, dy, false, mx, my)) return true;
    if (ConditionClick(def, dx, dy, true, mx, my)) return true;

    (void)CommitField();
    return true;
}

/* Rendering */

/* Shows the end of the text that fits in max_w pixels, followed by a caret. */
static void CaretTail(const char *text, int max_w, char *out, size_t cap)
{
    char trial[EDIT_BUF_BYTES + 2];
    snprintf(trial, sizeof(trial), "%s_", text);

    size_t start = 0;
    size_t len = strlen(trial);
    while (start + 1 < len && GUI_GetTextWidth(trial + start) > max_w) {
        start++;
        while (start + 1 < len && ((unsigned char)trial[start] & 0xC0) == 0x80) start++;
    }
    snprintf(out, cap, "%s", trial + start);
}

static void DrawBox(SDL_Renderer *renderer, SDL_FRect r, const char *label, SDL_Color text, bool on)
{
    SDL_SetRenderDrawColor(renderer, on ? 60 : 40, on ? 80 : 60, on ? 120 : 95, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 90, 130, 200, 255);
    SDL_RenderRect(renderer, &r);
    GUI_DrawTextCentered(renderer, r, label, text);
}

static void DrawField(SDL_Renderer *renderer, SDL_FRect box, EditField kind, int index, const ScenarioDef *def)
{
    bool focused = (focus_kind == kind && focus_index == index);
    char text[EDIT_BUF_BYTES + 2];
    if (focused) {
        CaretTail(edit_buf, (int)box.w - 12, text, sizeof(text));
    } else {
        FormatField(kind, index, def, text, sizeof(text));
    }

    SDL_SetRenderDrawColor(renderer, 14, 18, 28, 255);
    SDL_RenderFillRect(renderer, &box);
    if (focused) {
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
    }
    SDL_RenderRect(renderer, &box);

    SDL_Rect clip = { (int)box.x + 2, (int)box.y + 2, (int)box.w - 4, (int)box.h - 4 };
    SDL_SetRenderClipRect(renderer, &clip);
    GUI_DrawText(renderer, (int)box.x + 6, (int)box.y + 5, text, col_white);
    SDL_SetRenderClipRect(renderer, NULL);
}

static void DrawConditions(SDL_Renderer *renderer, float dx, float dy, const ScenarioDef *def, bool lose)
{
    float cx = ColX(dx, lose);
    const ScenarioCondition *list = lose ? def->lose : def->win;
    int count = lose ? def->lose_count : def->win_count;
    EditField value_kind = lose ? FIELD_LOSE_VALUE : FIELD_WIN_VALUE;

    for (int i = 0; i < count; i++) {
        float y = RowY(dy, i);
        DrawBox(renderer, MetricRect(cx, y), Scenario_MetricName(list[i].metric), col_white, false);
        DrawBox(renderer, OpRect(cx, y), Scenario_OpName(list[i].op), col_white, false);
        DrawField(renderer, ValueRect(cx, y), value_kind, i, def);
        DrawBox(renderer, RemoveRect(cx, y), "X", col_red, false);
    }
    if (count < SCN_MAX_CONDITIONS) {
        DrawBox(renderer, AddRect(cx, dy, count), "+ Add", col_green, false);
    }
}

/* Writes "Name: value" pairs for metrics first..last, separated by spaces. */
static void FormatReadout(char *out, size_t cap, int first, int last)
{
    out[0] = '\0';
    for (int m = first; m <= last; m++) {
        size_t len = strlen(out);
        snprintf(out + len, cap - len, "%s%s: %d", m > first ? "    " : "",
                 Scenario_MetricName(m), (int)Scenario_MetricValue(m));
    }
}

void Dialog_Editor_Render(SDL_Renderer *renderer)
{
    const ScenarioDef *def = Scenario_EditorDef();
    if (!def) return;

    float dx = DialogCenteredX(MODAL_W);
    float dy = DialogCenteredY(MODAL_H);

    SDL_FRect scrim = { 0.0f, 0.0f, (float)Video_GetCanvasWidth(), (float)Video_GetCanvasHeight() };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &scrim);

    SDL_FRect win = MakeRect(dx, dy, MODAL_W, MODAL_H);
    SDL_SetRenderDrawColor(renderer, 24, 28, 38, 250);
    SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 80, 110, 150, 255);
    SDL_RenderRect(renderer, &win);

    GUI_DrawText(renderer, (int)dx + 16, (int)dy + 14, "SCENARIO SETUP", col_gold);
    SDL_FRect close_x = CloseXRect(dx, dy);
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_x);
    GUI_DrawText(renderer, (int)close_x.x + 6, (int)close_x.y + 4, "X", col_white);

    GUI_DrawText(renderer, (int)dx + 14, (int)dy + 49, "Name", col_gray);
    DrawField(renderer, NameBox(dx, dy), FIELD_NAME, 0, def);
    GUI_DrawText(renderer, (int)dx + 14, (int)dy + 79, "Description", col_gray);
    DrawField(renderer, DescBox(dx, dy), FIELD_DESC, 0, def);

    GUI_DrawText(renderer, (int)dx + 14, (int)dy + 109, "Start year", col_gray);
    DrawField(renderer, StartBox(dx, dy), FIELD_START, 0, def);
    GUI_DrawText(renderer, (int)dx + 190, (int)dy + 109, "Funds", col_gray);
    DrawField(renderer, FundsBox(dx, dy), FIELD_FUNDS, 0, def);
    GUI_DrawText(renderer, (int)dx + 370, (int)dy + 109, "Deadline", col_gray);
    DrawField(renderer, DeadlineBox(dx, dy), FIELD_DEADLINE, 0, def);
    GUI_DrawText(renderer, (int)dx + 518, (int)dy + 109, "0 = none", col_gray);

    GUI_DrawText(renderer, (int)dx + 14, (int)dy + 139, "Disasters", col_gray);
    DrawBox(renderer, DisasterBox(dx, dy), def->disasters ? "Disasters: On" : "Disasters: Off",
            col_white, def->disasters != 0);

    GUI_DrawText(renderer, (int)ColX(dx, false), (int)dy + 170, "WIN", col_green);
    GUI_DrawText(renderer, (int)ColX(dx, true), (int)dy + 170, "LOSE", col_red);
    DrawBox(renderer, MatchRect(ColX(dx, false), dy), def->win_all ? "Match ALL" : "Match ANY",
            col_white, false);
    DrawBox(renderer, MatchRect(ColX(dx, true), dy), def->lose_all ? "Match ALL" : "Match ANY",
            col_white, false);
    DrawConditions(renderer, dx, dy, def, false);
    DrawConditions(renderer, dx, dy, def, true);

    char line[160];
    FormatReadout(line, sizeof(line), SCN_METRIC_CITY_CLASS, SCN_METRIC_TRAFFIC);
    GUI_DrawText(renderer, (int)dx + 14, (int)dy + 446, line, col_cyan);
    FormatReadout(line, sizeof(line), SCN_METRIC_CRIME, SCN_METRIC_FUNDS);
    GUI_DrawText(renderer, (int)dx + 14, (int)dy + 462, line, col_cyan);

    if (editor_message[0]) {
        GUI_DrawText(renderer, (int)dx + 14, (int)dy + 484, editor_message,
                     editor_message_is_error ? col_red : col_green);
    }
    GUI_DrawText(renderer, (int)dx + 14, (int)dy + 502,
                 "Click a field to edit. Enter applies it, Esc discards it.", col_gray);

    DrawBox(renderer, SaveRect(dx, dy), "Save Scenario", col_white, false);
    DrawBox(renderer, CloseRect(dx, dy), "Close", col_white, false);
}

/* Palette actions (called from the editor buttons in sdl3_gui.c) */

void GUI_Editor_Save(void)
{
    char text[MSG_BYTES];
    (void)SaveScenario(text, sizeof(text));
    GUI_ShowStatus(text);
}

void GUI_Editor_Step(void)
{
    if (SimSpeed != SIM_SPEED_PAUSED) {
        GUI_ShowStatus("Pause the editor before stepping.");
        return;
    }
    Scenario_EditorStep();
    GUI_ShowStatus("Advanced one month.");
}

void GUI_Editor_Exit(void)
{
    if (GUI_Dialogs_IsModalOpen()) GUI_Dialogs_Close();
    Scenario_EditorEnd();
    Input_SetActiveTool(residentialState);
    GUI_ShowStatus("Left editor mode.");
}

void Dialog_Editor_Start(void)
{
    Scenario_EditorBegin();
    Input_SetActiveTool(editorLandState);
    GUI_ShowStatus("Editor mode: paused. Paint terrain and buildings, then Save.");
}
