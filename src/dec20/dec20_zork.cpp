#ifdef KISAK_SP
#include "dec20_local.h"
#include "dec20_zmachine.h"

#include <cstring>
#include <vector>

#include <database/db_registry.h>
#include <live/live_win.h>
#include <qcommon/com_clients.h>
#include <qcommon/common.h>
#include <universal/com_files.h>
#include <universal/dvar.h>
#include <universal/q_shared.h>
#include <win32/win_shared.h>

namespace
{
const char *const ZORK_STORY = "terminal_resources/Zork1.z3";
const char *const ZORK_SAVE = "cod7zork.zsv";   // in the players directory, like retail
constexpr unsigned ZORK_STEPS_PER_FRAME = 100;
constexpr int ZORK_COLUMNS = 80;
constexpr int ZORK_MAX_WORD = 120;

// Retail draws no status line and wraps the game's text at 80 columns, where a word
// is a run of letters plus the character that ends it.
class ZorkIo : public ZMachineIo
{
public:
    void print(const char *text, size_t length) override
    {
        for (size_t i = 0; i < length; ++i)
            putChar(text[i]);
    }

    void statusLine(const char *, int, int, bool) override
    {
    }

    bool saveGame(const uint8_t *data, size_t size) override
    {
        const int file = FS_FOpenFileWriteToDir((char *)ZORK_SAVE, (char *)"players", fs_homepath->current.string);
        if (!file)
            return false;
        const unsigned written = FS_Write((char *)data, unsigned(size), file);
        FS_FCloseFile(file);
        return written == size;
    }

    bool restoreGame(std::vector<uint8_t> &data) override
    {
        void *buffer = nullptr;
        const int size = FS_ReadFile(ZORK_SAVE, &buffer);
        if (size > 0 && buffer)
            data.assign((const uint8_t *)buffer, (const uint8_t *)buffer + size);
        if (buffer)
            FS_FreeFile(buffer);
        return size > 0;
    }

    void reset()
    {
        m_wordLength = 0;
        m_column = 0;
    }

private:
    void putChar(char ch)
    {
        m_word[m_wordLength++] = ch;
        m_word[m_wordLength] = 0;
        if (m_wordLength >= ZORK_MAX_WORD)
        {
            Dec20_Print(m_word);
            Dec20_Print("\n");
            reset();
            return;
        }
        if (ch == '\n' || ch == '\r')
        {
            Dec20_Print(m_word);
            reset();
            return;
        }
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
            return;
        if (m_column + m_wordLength >= ZORK_COLUMNS)
        {
            Dec20_Print("\n");
            m_column = m_wordLength;
        }
        else
        {
            m_column += m_wordLength;
        }
        Dec20_Print(m_word);
        m_wordLength = 0;
    }

    char m_word[ZORK_MAX_WORD + 1];
    int m_wordLength = 0;
    int m_column = 0;
};

ZorkIo s_io;
ZMachine s_machine;
bool s_lineReady;   // Enter was pressed; the game takes the line when it next reads
}

void Dec20_ZorkStart()
{
    if (!*fs_gameDirVar->current.string)
        Live_GiveAchievement(Com_LocalClient_GetControllerIndex(0), "SP_LVL_FRONTEND_ZORK");
    s_io.reset();
    s_lineReady = false;
    const RawFile *story = DB_FindXAssetHeader(ASSET_TYPE_RAWFILE, (char *)ZORK_STORY, true, -1).rawfile;
    const uint8_t *data = story ? (const uint8_t *)story->buffer : nullptr;
    s_machine.load(data, data ? size_t(story->len) : 0, &s_io, Sys_Milliseconds());
}

void Dec20_Cmd_Zork(int, const char **)
{
    Dec20Mode &mode = Dec20_Mode();
    if (mode.handler == Dec20_Cmd_Zork)
    {
        s_lineReady = true;
        return;
    }
    Dec20_QueueClearRegion(1, DEC20_ROWS, 1, DEC20_COLUMNS);
    g_dec20.pictureCount = 0;
    memset(g_dec20.line, 0, sizeof(g_dec20.line));
    Dec20_ZorkStart();
    mode.handler = Dec20_Cmd_Zork;
}

// Runs the game for a frame; false once it has ended and the shell is back.
bool Dec20_ZorkStep()
{
    if (s_lineReady && s_machine.state() == ZMachine::State::NeedInput)
    {
        s_lineReady = false;
        s_machine.input(g_dec20.line);
        memset(g_dec20.line, 0, sizeof(g_dec20.line));
    }
    if (s_machine.state() == ZMachine::State::Halted)
    {
        if (*s_machine.error())
            Com_PrintWarning(14, "Zork stopped: %s\n", s_machine.error());
        Dec20_Mode().handler = nullptr;
        return false;
    }
    s_machine.run(ZORK_STEPS_PER_FRAME);
    return true;
}
#endif
