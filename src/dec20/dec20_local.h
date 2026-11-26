#pragma once

#include <cstdint>

struct Material;

// 80 x 27 character screen. Output always lands at the cursor, which sits on the
// last row: every newline scrolls the whole screen up.
constexpr int DEC20_COLUMNS = 80;
constexpr int DEC20_ROWS = 27;
constexpr int DEC20_CELLS = DEC20_COLUMNS * DEC20_ROWS;
constexpr int DEC20_LAST_ROW = DEC20_CELLS - DEC20_COLUMNS;
constexpr int DEC20_LINE_SIZE = 256;
constexpr int DEC20_HISTORY_SIZE = 8;
constexpr int DEC20_MODE_DEPTH = 16;
constexpr int DEC20_MAX_PICTURES = 6;
constexpr int DEC20_SOUND_ENTITY = 1022;

// Receives every entered line while installed on the top shell level.
using Dec20Handler = void (*)(int localClientNum, const char **args);

struct Dec20Mode
{
    int system;
    int user;
    int cwd;
    Dec20Handler handler;   // null for the plain shell
};

struct Dec20Picture
{
    Material *material;
    int row;
    int heightRows;
    int width;
};

struct Dec20Terminal
{
    char chars[DEC20_CELLS];
    uint8_t attrs[DEC20_CELLS];   // (foreground << 4) | background
    int cursor = DEC20_LAST_ROW;
    Dec20Picture pictures[DEC20_MAX_PICTURES];
    int pictureCount;
    int lineHeight;
    char line[DEC20_LINE_SIZE];
    char history[DEC20_HISTORY_SIZE][DEC20_LINE_SIZE];
    int historyPos;
    int historyMin;
    int historyMax;
    Dec20Mode modes[DEC20_MODE_DEPTH];
    int modeIndex;
    Dec20Handler savedHandler;   // restored by the next key press
    char sound[DEC20_LINE_SIZE];   // alias of the playing .snd file
    int ignoreCharsUntil;
    bool needInit;
};

extern Dec20Terminal g_dec20;

inline Dec20Mode &Dec20_Mode()
{
    return g_dec20.modes[g_dec20.modeIndex];
}

// dec20_terminal.cpp
void Dec20_Print(const char *text, int fg = 1, int bg = 1);
void Dec20_QueueChar(char ch, int fg, int bg);
void Dec20_QueueNewline();
void Dec20_QueueScroll(int top, int bottom, int left, int right, int count);
void Dec20_QueueClearRegion(int top, int bottom, int left, int right);
void Dec20_PlaySound(int localClientNum, const char *alias);
void Dec20_StopSound(int localClientNum);

// dec20_commands.cpp
void Dec20_LoadTables();
void Dec20_AutoLogin();
void Dec20_ProcessCommand(int localClientNum);
bool Dec20_IsPasswordPrompt();
void Dec20_Cmd_More(int localClientNum, const char **args);

// dec20_zork.cpp
void Dec20_Cmd_Zork(int localClientNum, const char **args);
void Dec20_ZorkStart();
bool Dec20_ZorkStep();
