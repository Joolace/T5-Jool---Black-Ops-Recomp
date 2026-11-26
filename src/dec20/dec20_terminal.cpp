#ifdef KISAK_SP
#include "dec20_terminal.h"
#include "dec20_local.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

#include <cgame/cg_sound.h>
#include <cgame_mp/cg_local_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <client/cl_keys.h>
#include <client/screen_placement.h>
#include <client_mp/cl_main_mp.h>
#include <database/db_registry.h>
#include <qcommon/common.h>
#include <sound/snd_utils.h>
#include <ui/keycodes.h>
#include <ui/ui_atoms.h>
#include <ui/ui_main.h>
#include <universal/dvar.h>
#include <universal/q_shared.h>
#include <win32/win_shared.h>

Dec20Terminal g_dec20;

namespace
{
enum QueueType : uint8_t
{
    QUEUE_TEXT = 1,
    QUEUE_SCROLL = 2,
    QUEUE_CLEAR = 4,
    QUEUE_MORE = 5,
};

struct QueueEntry
{
    uint8_t type;
    char text[80];
    uint8_t params[5];   // text: fg, bg, read position; scroll and clear: region, count
};

// All output goes through this queue and is revealed by the paint at one step per
// millisecond. Retail keeps 100 entries and overwrites unread ones on overflow.
constexpr unsigned QUEUE_SIZE = 1024;
QueueEntry s_queue[QUEUE_SIZE];
unsigned s_queueTail;
unsigned s_queueHead;
unsigned s_lastPaintTime;
Dec20Handler s_pagedHandler;   // the handler a --MORE-- pause replaced
constexpr int PAGE_LINES = 22;
constexpr int CATCHER_TERMINAL = 0x100;
}

// ----- screen -----

static void Dec20_ScrollRegion(int top, int bottom, int left, int right, int count)
{
    top = std::clamp(top, 1, DEC20_ROWS);
    bottom = std::clamp(bottom, top, DEC20_ROWS);
    left = std::clamp(left, 1, DEC20_COLUMNS);
    right = std::clamp(right, left, DEC20_COLUMNS);
    if (bottom >= DEC20_ROWS)
        g_dec20.cursor = DEC20_LAST_ROW;
    const int width = right - left + 1;
    for (int step = 0; step < count; ++step)
    {
        for (int row = top - 1; row < bottom - 1; ++row)
        {
            const int to = row * DEC20_COLUMNS + left - 1;
            memmove(&g_dec20.chars[to], &g_dec20.chars[to + DEC20_COLUMNS], width);
            memmove(&g_dec20.attrs[to], &g_dec20.attrs[to + DEC20_COLUMNS], width);
        }
        const int last = (bottom - 1) * DEC20_COLUMNS + left - 1;
        memset(&g_dec20.chars[last], 0, width);
        memset(&g_dec20.attrs[last], 0, width);
    }
    for (int i = 0; i < g_dec20.pictureCount;)
    {
        Dec20Picture &picture = g_dec20.pictures[i];
        if (picture.row + picture.heightRows >= top - 1 && picture.row - 1 <= bottom - 1)
        {
            picture.row -= count;
            if (picture.row <= -picture.heightRows)
            {
                std::copy(&g_dec20.pictures[i + 1], &g_dec20.pictures[g_dec20.pictureCount], &g_dec20.pictures[i]);
                --g_dec20.pictureCount;
                continue;
            }
        }
        ++i;
    }
}

static void Dec20_ClearRegion(int top, int bottom, int left, int right)
{
    top = std::clamp(top, 1, DEC20_ROWS);
    bottom = std::clamp(bottom, top, DEC20_ROWS);
    left = std::clamp(left, 1, DEC20_COLUMNS);
    right = std::clamp(right, left, DEC20_COLUMNS);
    for (int row = top - 1; row < bottom; ++row)
    {
        const int start = row * DEC20_COLUMNS + left - 1;
        memset(&g_dec20.chars[start], 0, right - left + 1);
        memset(&g_dec20.attrs[start], 0, right - left + 1);
    }
}

static void Dec20_PutCharAtCursor(char ch, int fg, int bg)
{
    switch (ch)
    {
    case 1:
    case '\b':
    case '\t':
    case '\r':
        return;
    case '\n':
        g_dec20.cursor = DEC20_LAST_ROW;
        Dec20_ScrollRegion(1, DEC20_ROWS, 1, DEC20_COLUMNS, 1);
        return;
    default:
        break;
    }
    if (g_dec20.cursor >= DEC20_CELLS)
    {
        Dec20_ScrollRegion(1, DEC20_ROWS, 1, DEC20_COLUMNS, 1);
        g_dec20.cursor = DEC20_LAST_ROW;
    }
    g_dec20.chars[g_dec20.cursor] = ch;
    g_dec20.attrs[g_dec20.cursor] = uint8_t(fg << 4 | bg);
    ++g_dec20.cursor;
}

// ----- output queue -----

static QueueEntry *Dec20_QueueAdd(QueueType type)
{
    if (s_queueTail - s_queueHead >= QUEUE_SIZE)
        return nullptr;
    QueueEntry *entry = &s_queue[s_queueTail++ % QUEUE_SIZE];
    memset(entry, 0, sizeof(*entry));
    entry->type = type;
    return entry;
}

void Dec20_QueueChar(char ch, int fg, int bg)
{
    if (s_queueTail != s_queueHead)
    {
        QueueEntry &last = s_queue[(s_queueTail - 1) % QUEUE_SIZE];
        const size_t length = strlen(last.text);
        if (last.type == QUEUE_TEXT && last.params[0] == uint8_t(fg) && last.params[1] == uint8_t(bg)
            && length + 1 < sizeof(last.text))
        {
            last.text[length] = ch;
            last.text[length + 1] = 0;
            return;
        }
    }
    if (QueueEntry *entry = Dec20_QueueAdd(QUEUE_TEXT))
    {
        entry->text[0] = ch;
        entry->params[0] = uint8_t(fg);
        entry->params[1] = uint8_t(bg);
    }
}

void Dec20_QueueScroll(int top, int bottom, int left, int right, int count)
{
    if (QueueEntry *entry = Dec20_QueueAdd(QUEUE_SCROLL))
    {
        entry->params[0] = uint8_t(top);
        entry->params[1] = uint8_t(bottom);
        entry->params[2] = uint8_t(left);
        entry->params[3] = uint8_t((std::min)(right, DEC20_COLUMNS));
        entry->params[4] = uint8_t((std::min)(count, int(UINT8_MAX)));
    }
}

void Dec20_QueueNewline()
{
    Dec20_QueueScroll(1, DEC20_ROWS, 1, DEC20_COLUMNS, 1);
}

void Dec20_QueueClearRegion(int top, int bottom, int left, int right)
{
    if (QueueEntry *entry = Dec20_QueueAdd(QUEUE_CLEAR))
    {
        entry->params[0] = uint8_t(top);
        entry->params[1] = uint8_t(bottom);
        entry->params[2] = uint8_t(left);
        entry->params[3] = uint8_t((std::min)(right, DEC20_COLUMNS));
    }
}

// '^n' sets the foreground colour and "\x01\x01" swaps to reverse video.
template <typename Emit>
static void Dec20_ParseText(const char *text, int fg, int bg, Emit &&emit)
{
    for (const char *c = text; *c; ++c)
    {
        if (*c == '^')
        {
            if (!*++c)
                return;
            fg = *c - '0';
            bg = 1;
        }
        else if (*c == 1)
        {
            if (*++c != 1)
                return;
            bg = fg;
            fg = 2;
        }
        else
        {
            emit(*c, fg, bg);
        }
    }
}

// Queues text; a single print longer than a page pauses on --MORE--.
void Dec20_Print(const char *text, int fg, int bg)
{
    int lines = 0;
    int column = 0;
    Dec20_ParseText(text, fg, bg, [&](char c, int textFg, int textBg) {
        if (c == '\n')
        {
            ++lines;
            column = 0;
        }
        else if (++column > DEC20_COLUMNS)
        {
            column = 0;
            ++lines;
        }
        Dec20_QueueChar(c, textFg, textBg);
        if (lines >= PAGE_LINES)
        {
            Dec20_QueueAdd(QUEUE_MORE);
            lines = 0;
        }
    });
}

static void Dec20_PrintImmediate(const char *text)
{
    Dec20_ParseText(text, 1, 1, Dec20_PutCharAtCursor);
}

// Toggles a wait for any key, restoring the previous handler.
static void Dec20_WaitKey(int, const char **)
{
    Dec20Mode &mode = Dec20_Mode();
    if (mode.handler == Dec20_WaitKey)
    {
        mode.handler = g_dec20.savedHandler;
        g_dec20.savedHandler = nullptr;
    }
    else
    {
        g_dec20.savedHandler = mode.handler;
        mode.handler = Dec20_WaitKey;
    }
}

// Marks a --MORE-- pause the player dismissed; the next paint erases the prompt.
static void Dec20_MoreDismissed(int, const char **)
{
}

// Returns true once the page break is finished with.
static bool Dec20_PageBreak()
{
    Dec20Mode &mode = Dec20_Mode();
    if (mode.handler == Dec20_MoreDismissed)
    {
        g_dec20.cursor = (std::max)(g_dec20.cursor - 8, 0);
        Dec20_PrintImmediate("        ");
        g_dec20.cursor = (std::max)(g_dec20.cursor - 8, 0);
        mode.handler = s_pagedHandler;
        return true;
    }
    if (mode.handler != Dec20_WaitKey)
    {
        s_pagedHandler = mode.handler;
        mode.handler = Dec20_MoreDismissed;
        Dec20_WaitKey(0, nullptr);
        Dec20_PrintImmediate("--MORE--");
    }
    return false;
}

static void Dec20_DrainQueue()
{
    if (s_queueHead == s_queueTail)
        return;
    const unsigned now = Sys_Milliseconds();
    for (int budget = int(now - s_lastPaintTime); budget > 0 && s_queueHead != s_queueTail; --budget)
    {
        QueueEntry &entry = s_queue[s_queueHead % QUEUE_SIZE];
        const uint8_t *p = entry.params;
        switch (entry.type)
        {
        case QUEUE_TEXT:
            if (p[2] < sizeof(entry.text) && entry.text[p[2]])
                Dec20_PutCharAtCursor(entry.text[entry.params[2]++], p[0], p[1]);
            else
                ++s_queueHead;
            break;
        case QUEUE_SCROLL:
            Dec20_ScrollRegion(p[0], p[1], p[2], p[3], p[4]);
            ++s_queueHead;
            break;
        case QUEUE_CLEAR:
            Dec20_ClearRegion(p[0], p[1], p[2], p[3]);
            ++s_queueHead;
            break;
        case QUEUE_MORE:
            if (!Dec20_PageBreak())
                budget = 0;
            else
                ++s_queueHead;
            break;
        default:
            ++s_queueHead;
            break;
        }
    }
    s_lastPaintTime = now;
}

// ----- paint -----

static void Dec20_AttrColors(uint8_t attr, float *text, float *fill)
{
    static const float palette[10][3] = {
        { 1, 1, 1 }, { 1, 1, 1 }, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 },
        { 1, 1, 0 }, { 0, 0, 1 }, { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 },
    };
    static const float black[3] = { 0, 0, 0 };
    const int fg = attr >> 4;
    const int bg = attr & 15;
    std::copy_n(palette[fg < 10 ? fg : 0], 3, text);
    std::copy_n(bg < 10 ? palette[bg] : black, 3, fill);
    text[3] = 1.0f;
    fill[3] = 1.0f;
}

void Dec20_Paint(int localClientNum, const rectDef_s *rect, Font_s *font, float scale, int textStyle)
{
    Dec20_DrainQueue();

    const ScreenPlacement *scrPlace = &scrPlaceView[localClientNum];
    const int lineHeight = UI_TextHeight(font, scale);
    g_dec20.lineHeight = lineHeight;

    // The grid is bottom-aligned in the item: row r has its baseline on line r + 9.
    int width = 0;
    for (int row = 0; row < DEC20_ROWS; ++row)
    {
        char *chars = &g_dec20.chars[row * DEC20_COLUMNS];
        const uint8_t *attrs = &g_dec20.attrs[row * DEC20_COLUMNS];
        const float y = rect->y + float(lineHeight * (row + 9));
        width = 0;
        for (int start = 0, col = 0; col < DEC20_COLUMNS;)
        {
            // A run ends at the first cell whose attribute differs, or at a NUL, and
            // includes that cell; retail draws the whole run in that cell's colours.
            if (col + 1 < DEC20_COLUMNS && attrs[col] == attrs[start] && chars[col])
            {
                ++col;
                continue;
            }
            const int count = col - start + 1;
            const int runWidth = UI_TextWidth(&chars[start], count, font, scale);
            float textColor[4];
            float fillColor[4];
            Dec20_AttrColors(attrs[col], textColor, fillColor);
            if ((attrs[col] & 15) > 2)
            {
                UI_FillRect(scrPlace, rect->x + float(width), y - float(lineHeight), float(runWidth), float(lineHeight),
                            rect->horzAlign, rect->vertAlign, fillColor);
            }
            UI_DrawText(scrPlace, &chars[start], count, font, rect->x + float(width), y, rect->horzAlign,
                        rect->vertAlign, scale, textColor, textStyle);
            width += runWidth;
            if (!chars[col])
                break;
            start = ++col;
        }
    }

    // The blinking cursor always follows the text of the last row.
    const cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    const int interval = cg_invalidCmdHintBlinkInterval->current.integer;
    if (interval > 0 && float((cgameGlob->time - cgameGlob->invalidCmdHintTime) % interval) / float(interval) > 0.5f)
    {
        char cursorText[] = "_";
        UI_DrawText(scrPlace, cursorText, 0x7FFFFFFF, font, rect->x + float(width),
                    rect->y + float(lineHeight * (DEC20_ROWS + 8)), rect->horzAlign, rect->vertAlign, scale, colorWhite,
                    textStyle);
    }

    for (int i = 0; i < g_dec20.pictureCount; ++i)
    {
        const Dec20Picture &picture = g_dec20.pictures[i];
        UI_DrawHandlePic(scrPlace, rect->x, rect->y + float((picture.row + 8) * lineHeight), float(picture.width),
                         float(picture.heightRows * lineHeight), rect->horzAlign, rect->vertAlign, colorWhite,
                         picture.material);
    }
}

// ----- sound -----

void Dec20_PlaySound(int localClientNum, const char *alias)
{
    CG_PlaySound(localClientNum, DEC20_SOUND_ENTITY, nullptr, 0, false, 1.0f, alias);
}

void Dec20_StopSound(int localClientNum)
{
    CG_StopSoundAlias(localClientNum, DEC20_SOUND_ENTITY, SND_HashName(g_dec20.sound));
}

// ----- input -----

static void Dec20_RecallHistory(int direction)
{
    const int pos = g_dec20.historyPos + direction;
    if (direction < 0 ? pos < g_dec20.historyMin : pos > g_dec20.historyMax)
        return;
    const int length = int(strlen(g_dec20.line));
    g_dec20.cursor = (std::max)(g_dec20.cursor - length, 0);
    const int row = g_dec20.cursor / DEC20_COLUMNS;
    const int col = g_dec20.cursor % DEC20_COLUMNS;
    Dec20_QueueClearRegion(row + 1, row + 1, col + 1, col + length + 1);
    g_dec20.historyPos = pos;
    I_strncpyz(g_dec20.line, g_dec20.history[pos % DEC20_HISTORY_SIZE], DEC20_LINE_SIZE);
    Dec20_Print(g_dec20.line);
}

static void Dec20_EnterLine(int localClientNum)
{
    Dec20_QueueNewline();
    Dec20_ProcessCommand(localClientNum);
    I_strncpyz(g_dec20.history[g_dec20.historyPos % DEC20_HISTORY_SIZE], g_dec20.line, DEC20_LINE_SIZE);
    ++g_dec20.historyPos;
    g_dec20.historyMax = (std::max)(g_dec20.historyMax, g_dec20.historyPos);
    if (g_dec20.historyPos - DEC20_HISTORY_SIZE > g_dec20.historyMin)
        ++g_dec20.historyMin;
    // Zork reads the line itself on its next step.
    if (Dec20_Mode().handler != Dec20_Cmd_Zork)
        memset(g_dec20.line, 0, sizeof(g_dec20.line));
}

static void Dec20_PlayTypeSound(int localClientNum)
{
    Dec20_PlaySound(localClientNum, "uin_text_type_zork");
}

static void Dec20_TypeChar(char ch)
{
    const size_t length = strlen(g_dec20.line);
    g_dec20.line[length] = ch;
    Dec20_QueueChar(Dec20_IsPasswordPrompt() ? '*' : ch, 1, 1);
}

void Dec20_KeyEvent(int localClientNum, int key, int down, unsigned int)
{
    if (!down)
        return;
    for (;;)
    {
        if (key == K_BUTTON_X || key == K_ESCAPE)
        {
            Dec20_Exit(localClientNum);
            return;
        }
        Dec20Mode &mode = Dec20_Mode();
        if (mode.handler == Dec20_Cmd_More || mode.handler == Dec20_WaitKey)
        {
            mode.handler = g_dec20.savedHandler;
            g_dec20.savedHandler = nullptr;
            return;
        }
        const size_t length = strlen(g_dec20.line);
        switch (key)
        {
        case K_BACKSPACE:
            if (length && g_dec20.cursor > 0)
            {
                g_dec20.line[length - 1] = 0;
                g_dec20.chars[--g_dec20.cursor] = 0;
            }
            return;
        case K_UPARROW:
            Dec20_RecallHistory(-1);
            break;
        case K_DOWNARROW:
            Dec20_RecallHistory(1);
            break;
        case K_ENTER:
        case K_KP_ENTER:
            Dec20_EnterLine(localClientNum);
            return;
        default:
            break;
        }

        // The pad drives the on-screen keyboard menu; A types its focused key.
        UI_KeyEvent(localClientNum, key, down);
        if (key != K_BUTTON_A || length >= DEC20_LINE_SIZE - 1)
            return;
        Dec20_PlayTypeSound(localClientNum);
        const char *focus = Dvar_GetString("ui_keyboard_focus_key");
        if (!I_stricmp(focus, "SPACE"))
        {
            g_dec20.line[length] = ' ';
            Dec20_QueueChar(' ', 1, 1);
            return;
        }
        if (!I_stricmp(focus, "ENTER"))
        {
            key = K_ENTER;
            continue;
        }
        if (!I_stricmp(focus, "DEL"))
        {
            key = K_BACKSPACE;
            continue;
        }
        if (*focus)
            Dec20_TypeChar(*focus);
        return;
    }
}

// Highlights the on-screen keyboard key matching a typed character.
static void Dec20_FocusKeyForChar(int ch)
{
    static const char *const digits[10] = { "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE" };
    char name[16] = {};
    switch (ch)
    {
    case '\b': I_strncpyz(name, "DELETE", sizeof(name)); break;
    case '\r': I_strncpyz(name, "ENTER", sizeof(name)); break;
    case ' ': I_strncpyz(name, "SPACE", sizeof(name)); break;
    case '/': I_strncpyz(name, "SLASH", sizeof(name)); break;
    case '?': I_strncpyz(name, "QUESTION", sizeof(name)); break;
    case '-': I_strncpyz(name, "MINUS", sizeof(name)); break;
    case '.': I_strncpyz(name, "DOT", sizeof(name)); break;
    default:
        if (ch >= '0' && ch <= '9')
            I_strncpyz(name, digits[ch - '0'], sizeof(name));
        else if (ch > 0 && ch < 0x80)
            name[0] = char(toupper(ch));
        break;
    }
    Dvar_SetStringByName("ui_keyboard_focus", name);
}

void Dec20_CharEvent(int localClientNum, int ch)
{
    if (CG_GetLocalClientGlobals(localClientNum)->time < g_dec20.ignoreCharsUntil)
        return;
    if (strlen(g_dec20.line) > DEC20_COLUMNS)
        return;
    Dec20_FocusKeyForChar(ch);
    Dec20_PlayTypeSound(localClientNum);
    if (ch < 0x20 || ch > 0x7F)
    {
        UI_KeyEvent(localClientNum, ch | 0x400, 1);
        return;
    }
    Dec20_TypeChar(char(ch));
}

// ----- lifetime -----

void Dec20_Init()
{
    g_dec20.needInit = true;
    if (!DB_IsZoneLoaded("terminal") && useFastFile->current.enabled)
    {
        XZoneInfo zoneInfo = { "terminal", 0x800000, 0 };
        DB_LoadXAssets(&zoneInfo, 1, 0);
    }
}

void Dec20_Attach(int localClientNum)
{
    g_dec20.ignoreCharsUntil = CG_GetLocalClientGlobals(localClientNum)->time + 100;
    g_dec20.historyPos = 0;
    g_dec20.historyMin = 0;
    g_dec20.historyMax = 0;
    memset(g_dec20.line, 0, sizeof(g_dec20.line));
    Key_AddCatcher(localClientNum, CATCHER_TERMINAL);
    UI_OpenMenu(localClientNum, "keyboard");
    Dvar_SetBool((dvar_s *)cl_bypassMouseInput, true);
}

void Dec20_Exit(int localClientNum)
{
    Key_RemoveCatcher(localClientNum, ~CATCHER_TERMINAL);
    UI_CloseMenu(localClientNum, "keyboard");
    Dvar_SetBool((dvar_s *)cl_bypassMouseInput, false);
    Dvar_SetIntByName("dec20_inuse", 0);
    Dec20_StopSound(localClientNum);
}

void Dec20_Update()
{
    if (g_dec20.needInit && DB_IsZoneLoaded("terminal"))
    {
        g_dec20.needInit = false;
        Dec20_LoadTables();
        Dec20_AutoLogin();
    }
    if (Dec20_Mode().handler == Dec20_Cmd_Zork && !Dec20_ZorkStep())
    {
        Dec20_QueueNewline();
        Dec20_Print("$");
    }
}

// The screen, history and shell levels; the output queue and Zork survive.
void Dec20_Reset()
{
    memset(g_dec20.chars, 0, sizeof(g_dec20.chars));
    memset(g_dec20.attrs, 0, sizeof(g_dec20.attrs));
    g_dec20.cursor = DEC20_LAST_ROW;
    g_dec20.pictureCount = 0;
    g_dec20.historyPos = 0;
    g_dec20.historyMin = 0;
    g_dec20.historyMax = 0;
    g_dec20.savedHandler = nullptr;
    g_dec20.modeIndex = 0;
    std::fill(std::begin(g_dec20.modes), std::end(g_dec20.modes), Dec20Mode{});
}

void Dec20_Shutdown()
{
    if (DB_IsZoneLoaded("terminal") && useFastFile->current.enabled)
    {
        XZoneInfo zoneInfo = { nullptr, 0, 0x800000 };
        DB_LoadXAssets(&zoneInfo, 1, 0);
    }
    Dec20_Reset();
}
#endif
