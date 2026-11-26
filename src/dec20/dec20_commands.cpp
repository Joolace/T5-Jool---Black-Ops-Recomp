#ifdef KISAK_SP
#include "dec20_local.h"
#include "dec20_terminal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>

#include <bgame/bg_misc.h>
#include <database/db_registry.h>
#include <game_mp/ui_gameinfo_mp.h>
#include <gfx_d3d/r_material.h>
#include <live/live_win.h>
#include <qcommon/cmd.h>
#include <qcommon/com_clients.h>
#include <ui/ui_main.h>
#include <universal/com_buildinfo.h>
#include <universal/com_stringtable.h>
#include <universal/dvar.h>
#include <universal/q_parse.h>
#include <universal/q_shared.h>
#include <win32/win_gamerprofile.h>

namespace
{
enum Dec20Table
{
    TABLE_SYSTEMS,
    TABLE_USERS,
    TABLE_FILESYSTEM,
    TABLE_MAIL,
    TABLE_COUNT,
};

const char *const s_tableNames[TABLE_COUNT] = {
    "terminal_resources/systems.csv",
    "terminal_resources/users.csv",
    "terminal_resources/filesystem.csv",
    "terminal_resources/mail.csv",
};

enum FileType
{
    FILE_DIRECTORY = 0,
    FILE_TEXT = 1,
    FILE_PICTURE = 2,
    FILE_SOUND = 3,
};

constexpr int FILE_HIDDEN = 2;

struct SystemRecord
{
    int id;
    const char *name;
    int root;   // filesystem node of the login banner
};

struct UserRecord
{
    int id;
    int system;
    const char *name;
    const char *password;
    int home;
};

struct FileRecord
{
    int id;
    int system;
    int parent;
    int owner;
    const char *name;
    const char *data;   // text file id, material or sound alias
    int type;
    int visibility;
};

struct MailRecord
{
    int id;
    int system;
    int recipient;
    int body;
    const char *from;
    const char *subject;
    const char *date;
};

enum RloginState
{
    RLOGIN_USER = 1,
    RLOGIN_PASSWORD = 2,
};

constexpr int MAIL_PER_SCREEN = 30;

const StringTable *s_tables[TABLE_COUNT];
int s_queryRows[TABLE_COUNT];   // queries continue from the last match, like retail
int s_rloginState;
char s_rloginUser[64];
bool s_rloginRetry;
bool s_autoLogin;
int s_mailIds[MAIL_PER_SCREEN];
char s_path[DEC20_LINE_SIZE];
}

// ----- tables -----

void Dec20_LoadTables()
{
    for (int i = 0; i < TABLE_COUNT; ++i)
    {
        XAssetHeader header;
        StringTable_GetAsset(s_tableNames[i], &header);
        s_tables[i] = header.stringTable;
    }
}

static void Dec20_ResetQueries()
{
    std::fill(std::begin(s_queryRows), std::end(s_queryRows), 0);
}

static const char *Dec20_Cell(Dec20Table table, int row, int column)
{
    return StringTable_GetColumnValueForRow(s_tables[table], row, column);
}

static int Dec20_IntCell(Dec20Table table, int row, int column)
{
    return atoi(Dec20_Cell(table, row, column));
}

static SystemRecord Dec20_ReadSystem(int row)
{
    return { Dec20_IntCell(TABLE_SYSTEMS, row, 0), Dec20_Cell(TABLE_SYSTEMS, row, 1), Dec20_IntCell(TABLE_SYSTEMS, row, 2) };
}

static UserRecord Dec20_ReadUser(int row)
{
    return { Dec20_IntCell(TABLE_USERS, row, 0), Dec20_IntCell(TABLE_USERS, row, 1), Dec20_Cell(TABLE_USERS, row, 2),
             Dec20_Cell(TABLE_USERS, row, 3), Dec20_IntCell(TABLE_USERS, row, 4) };
}

static FileRecord Dec20_ReadFile(int row)
{
    return { Dec20_IntCell(TABLE_FILESYSTEM, row, 0), Dec20_IntCell(TABLE_FILESYSTEM, row, 1),
             Dec20_IntCell(TABLE_FILESYSTEM, row, 2), Dec20_IntCell(TABLE_FILESYSTEM, row, 3),
             Dec20_Cell(TABLE_FILESYSTEM, row, 5), Dec20_Cell(TABLE_FILESYSTEM, row, 4),
             Dec20_IntCell(TABLE_FILESYSTEM, row, 6), Dec20_IntCell(TABLE_FILESYSTEM, row, 7) };
}

static MailRecord Dec20_ReadMail(int row)
{
    return { Dec20_IntCell(TABLE_MAIL, row, 0), Dec20_IntCell(TABLE_MAIL, row, 1), Dec20_IntCell(TABLE_MAIL, row, 2),
             Dec20_IntCell(TABLE_MAIL, row, 3), Dec20_Cell(TABLE_MAIL, row, 5), Dec20_Cell(TABLE_MAIL, row, 6),
             Dec20_Cell(TABLE_MAIL, row, 7) };
}

// Next row of the table, in zone order, that matches.
template <typename Record, typename Match>
static bool Dec20_Next(Dec20Table table, Record (*read)(int), Match &&match, Record *out)
{
    const StringTable *strings = s_tables[table];
    while (strings && s_queryRows[table] < strings->rowCount)
    {
        const Record record = read(s_queryRows[table]++);
        if (match(record))
        {
            *out = record;
            return true;
        }
    }
    return false;
}

template <typename Match>
static bool Dec20_FindFile(Match &&match, FileRecord *out)
{
    Dec20_ResetQueries();
    return Dec20_Next(TABLE_FILESYSTEM, Dec20_ReadFile, match, out);
}

static bool Dec20_FindFileById(int id, FileRecord *out)
{
    return Dec20_FindFile([id](const FileRecord &file) { return file.id == id; }, out);
}

static bool Dec20_FindUser(int system, const char *name, UserRecord *out)
{
    Dec20_ResetQueries();
    return Dec20_Next(TABLE_USERS, Dec20_ReadUser,
                      [&](const UserRecord &user) { return user.system == system && !I_stricmp(user.name, name); }, out);
}

// ----- output helpers -----

static void Dec20_PrintLine(const char *text)
{
    Dec20_Print(text);
    Dec20_QueueNewline();
}

static void Dec20_PrintError(const char *message)
{
    Dec20_PrintLine(va("^3Error^1:  %s", message));
}

// ----- paths -----

// Retail looks each path element up by name anywhere on the current system, so a
// file opens from any directory; ".." is the parent of the last element found.
static int Dec20_ResolvePath(const char *path)
{
    const Dec20Mode &mode = Dec20_Mode();
    char buffer[248];
    I_strncpyz(buffer, path, sizeof(buffer));
    char *p = buffer;
    int current = mode.cwd;
    if (*p == '/')
    {
        ++p;
        current = -1;
    }
    FileRecord record = {};
    record.parent = -1;
    if (current != -1
        && !Dec20_FindFile([&](const FileRecord &file) { return file.id == current && file.system == mode.system; }, &record))
    {
        return current;
    }
    while (*p)
    {
        const char *element = p;
        if (char *slash = strchr(p, '/'))
        {
            *slash = 0;
            p = slash + 1;
        }
        else
        {
            p += strlen(p);
        }
        if (!strcmp(element, ".."))
        {
            current = record.parent;
        }
        else if (strcmp(element, "."))
        {
            if (!Dec20_FindFile([&](const FileRecord &file) { return file.system == mode.system && !I_stricmp(file.name, element); },
                                &record))
            {
                Dec20_PrintError("Invalid Path");
                return mode.cwd;
            }
            if (record.owner != -1 && record.owner != mode.user)
            {
                Dec20_PrintError("Insufficient Permissions");
                return mode.cwd;
            }
            current = record.id;
        }
    }
    return current;
}

static const char *Dec20_GetPath(int node)
{
    const Dec20Mode &mode = Dec20_Mode();
    constexpr int MAX_DEPTH = 32;
    I_strncpyz(s_path, "/", sizeof(s_path));
    FileRecord record;
    for (int depth = 0; node != -1 && depth < MAX_DEPTH; ++depth)
    {
        if (!Dec20_FindFile([&](const FileRecord &file) { return file.id == node && file.system == mode.system; }, &record))
            break;
        const std::string child = s_path;
        Com_sprintf(s_path, sizeof(s_path), "/%s%s", record.name, child.c_str());
        node = record.parent;
    }
    return s_path;
}

static void Dec20_DisplayFile(int localClientNum, const FileRecord &file)
{
    switch (file.type)
    {
    case FILE_TEXT:
    {
        char path[DEC20_LINE_SIZE];
        Com_sprintf(path, sizeof(path), "terminal_resources/terminal_FILES/%s", file.data);
        const RawFile *rawfile = DB_FindXAssetHeader(ASSET_TYPE_RAWFILE, path, true, -1).rawfile;
        if (rawfile && rawfile->buffer)
        {
            Dec20_Print(std::string(rawfile->buffer, rawfile->len).c_str());
            Dec20_QueueNewline();
        }
        break;
    }
    case FILE_PICTURE:
    {
        // The picture enters below the grid and scrolls up with the text.
        Material *material = Material_RegisterHandle(file.data, 7);
        if (!material || !material->textureCount || !material->textureTable[0].u.image || g_dec20.lineHeight <= 0)
            break;
        const GfxImage *image = material->textureTable[0].u.image;
        if (g_dec20.pictureCount == DEC20_MAX_PICTURES)
        {
            std::copy(&g_dec20.pictures[1], &g_dec20.pictures[DEC20_MAX_PICTURES], &g_dec20.pictures[0]);
            --g_dec20.pictureCount;
        }
        Dec20Picture &picture = g_dec20.pictures[g_dec20.pictureCount++];
        picture.material = material;
        picture.row = DEC20_ROWS;
        picture.heightRows = int(floorf(float(image->height) / float(g_dec20.lineHeight) + 1.0f));
        picture.width = image->width;
        Dec20_QueueScroll(1, DEC20_ROWS, 1, DEC20_COLUMNS, picture.heightRows);
        break;
    }
    case FILE_SOUND:
        Dec20_StopSound(localClientNum);
        I_strncpyz(g_dec20.sound, file.data, sizeof(g_dec20.sound));
        Dec20_PlaySound(localClientNum, g_dec20.sound);
        break;
    default:
        break;
    }
}

// ----- commands -----

static void Dec20_Cmd_Nothing(int, const char **)
{
}

static void Dec20_Cmd_Cat(int localClientNum, const char **args)
{
    const char *name = Com_Parse(args)->token;
    if (!*name)
    {
        Dec20_PrintError("Invalid Input");
        return;
    }
    FileRecord file;
    if (!Dec20_FindFileById(Dec20_ResolvePath(name), &file))
    {
        Dec20_PrintError("File Not Found");
        return;
    }
    if (file.type != FILE_DIRECTORY && file.visibility != FILE_HIDDEN)
        Dec20_DisplayFile(localClientNum, file);
}

static void Dec20_Cmd_Cd(int, const char **args)
{
    const char *path = Com_Parse(args)->token;
    if (*path)
        Dec20_Mode().cwd = Dec20_ResolvePath(path);
    else
        Dec20_PrintLine(Dec20_GetPath(Dec20_Mode().cwd));
}

static void Dec20_Cmd_Clear(int, const char **)
{
    memset(g_dec20.chars, 0, sizeof(g_dec20.chars));
    g_dec20.pictureCount = 0;
}

static void Dec20_Cmd_Dir(int, const char **)
{
    const Dec20Mode &mode = Dec20_Mode();
    Dec20_ResetQueries();
    FileRecord file;
    int columns = 0;
    while (Dec20_Next(TABLE_FILESYSTEM, Dec20_ReadFile,
                      [&](const FileRecord &entry) { return entry.system == mode.system && entry.parent == mode.cwd; }, &file))
    {
        if (file.visibility != FILE_HIDDEN)
        {
            char entry[DEC20_LINE_SIZE];
            Com_sprintf(entry, sizeof(entry), " %-19s", file.type == FILE_DIRECTORY ? va("%s/", file.name) : file.name);
            Dec20_Print(entry);
            ++columns;
        }
        if (columns > 3)
        {
            Dec20_QueueNewline();
            columns = 0;
        }
    }
    Dec20_QueueNewline();
}

static void Dec20_LoginAs(Dec20Mode &mode, const UserRecord &user)
{
    mode.handler = nullptr;
    mode.user = user.id;
    mode.cwd = user.home;
}

static void Dec20_Cmd_Rlogin(int localClientNum, const char **args)
{
    const char *token = Com_Parse(args)->token;
    Dec20Mode &mode = Dec20_Mode();
    if (mode.handler == Dec20_Cmd_Rlogin)
    {
        if (s_rloginState == RLOGIN_USER)
        {
            Dec20_Print("PASSWORD:");
            I_strncpyz(s_rloginUser, token, sizeof(s_rloginUser));
            s_rloginState = RLOGIN_PASSWORD;
        }
        else if (s_rloginState == RLOGIN_PASSWORD)
        {
            UserRecord user;
            if (Dec20_FindUser(mode.system, s_rloginUser, &user) && !I_stricmp(user.password, token))
            {
                Dec20_LoginAs(mode, user);
                return;
            }
            Dec20_PrintLine("Invalid Password");
            if (s_rloginRetry)
            {
                s_rloginState = RLOGIN_USER;
                Dec20_Print("USER:");
            }
            else
            {
                --g_dec20.modeIndex;
            }
        }
        return;
    }

    if (!*token)
    {
        Dec20_PrintError("Invalid Input - expected machine name");
        return;
    }
    if (g_dec20.modeIndex + 1 >= DEC20_MODE_DEPTH)
    {
        Dec20_PrintError("Too many logins - Use exit to close open shells");
        return;
    }
    Dec20_ResetQueries();
    SystemRecord system;
    if (!Dec20_Next(TABLE_SYSTEMS, Dec20_ReadSystem, [&](const SystemRecord &entry) { return !I_stricmp(entry.name, token); }, &system)
        || system.id == -1)
    {
        Dec20_PrintError("unknown system");
        return;
    }

    ++g_dec20.modeIndex;
    Dec20Mode &login = Dec20_Mode();
    login.system = system.id;
    login.handler = Dec20_Cmd_Rlogin;
    s_rloginState = RLOGIN_USER;
    s_rloginRetry = false;
    FileRecord banner;
    if (Dec20_FindFileById(system.root, &banner))
        Dec20_DisplayFile(localClientNum, banner);
    Dec20_Print("USER:");
    if (s_autoLogin)
    {
        UserRecord user;
        if (Dec20_FindUser(login.system, "amason", &user))
        {
            Dec20_PrintLine("amason");
            Dec20_Print("PASSWORD:");
            Dec20_PrintLine("********");
            Dec20_LoginAs(login, user);
        }
        s_autoLogin = false;
        Dec20_QueueNewline();
        Dec20_Print("$");
    }
}

// Logs Mason into the CIA system, as when the terminal first starts.
void Dec20_AutoLogin()
{
    s_rloginRetry = true;
    s_autoLogin = true;
    const char *system = "CIA";
    Dec20_Cmd_Rlogin(0, &system);
}

bool Dec20_IsPasswordPrompt()
{
    return Dec20_Mode().handler == Dec20_Cmd_Rlogin && s_rloginState == RLOGIN_PASSWORD;
}

static void Dec20_Cmd_Login(int localClientNum, const char **)
{
    const int current = Dec20_Mode().system;
    Dec20_ResetQueries();
    SystemRecord system;
    if (Dec20_Next(TABLE_SYSTEMS, Dec20_ReadSystem, [&](const SystemRecord &entry) { return entry.id == current; }, &system))
    {
        const char *name = system.name;
        Dec20_Cmd_Rlogin(localClientNum, &name);
    }
}

// Leaving the last shell logs in again automatically.
static void Dec20_Cmd_Exit(int, const char **)
{
    if (g_dec20.modeIndex > 0)
        --g_dec20.modeIndex;
    if (!g_dec20.modeIndex)
        Dec20_AutoLogin();
}

static void Dec20_PrintInbox()
{
    char line[160];
    Com_sprintf(line, sizeof(line), "id %10s%-14s %10s%-14s %10s%-16s", " ", "from", " ", "date", " ", "subject");
    Dec20_PrintLine(line);
    const Dec20Mode &mode = Dec20_Mode();
    Dec20_ResetQueries();
    MailRecord mail;
    for (int n = 0; n < MAIL_PER_SCREEN
         && Dec20_Next(TABLE_MAIL, Dec20_ReadMail,
                       [&](const MailRecord &entry) { return entry.system == mode.system && entry.recipient == mode.user; }, &mail);
         ++n)
    {
        Com_sprintf(line, sizeof(line), "%2d %-24s %-23s %-27s", n, mail.from, mail.date, mail.subject);
        s_mailIds[n] = mail.id;
        Dec20_PrintLine(line);
    }
}

static void Dec20_Cmd_Mail(int localClientNum, const char **args)
{
    static const char *const version = "Mail Version 0.72.  Type ? for help.";
    Dec20Mode &mode = Dec20_Mode();
    if (mode.handler != Dec20_Cmd_Mail)
    {
        std::fill(std::begin(s_mailIds), std::end(s_mailIds), -1);
        mode.handler = Dec20_Cmd_Mail;
        Dec20_PrintInbox();
        Dec20_PrintLine(version);
        Dec20_Print("&");
        return;
    }

    const char *token = Com_Parse(args)->token;
    if (!strcmp(token, "?"))
    {
        Dec20_PrintLine("Mail Help:");
        Dec20_PrintLine("     ?          This help information");
        Dec20_PrintLine("     i          Display inbox");
        Dec20_PrintLine("     [n]        Read Message [n]");
        Dec20_PrintLine("     q          Quit Mail");
    }
    else if (!I_stricmp(token, "i"))
    {
        Dec20_PrintInbox();
    }
    else if (isdigit(uint8_t(*token)))
    {
        Dec20_QueueNewline();
        const int index = atoi(token);
        MailRecord mail;
        FileRecord body;
        Dec20_ResetQueries();
        if (index < MAIL_PER_SCREEN
            && Dec20_Next(TABLE_MAIL, Dec20_ReadMail, [&](const MailRecord &entry) { return entry.id == s_mailIds[index]; }, &mail))
        {
            if (Dec20_FindFileById(mail.body, &body))
                Dec20_DisplayFile(localClientNum, body);
            Dec20_QueueNewline();
        }
    }
    else if (!I_stricmp(token, "q"))
    {
        mode.handler = nullptr;
        return;
    }
    else
    {
        Dec20_PrintLine(version);
    }
    Dec20_Print("&");
}

static void Dec20_Cmd_Who(int, const char **)
{
    const int system = Dec20_Mode().system;
    Dec20_ResetQueries();
    UserRecord user;
    while (Dec20_Next(TABLE_USERS, Dec20_ReadUser, [&](const UserRecord &entry) { return entry.system == system; }, &user))
        Dec20_PrintLine(user.name);
}

// Agency cypher: each character is four hex digits, each digit written as one of two
// letters. '6' and '7' end a character early, otherwise M or Z separates characters.
static const char *Dec20_CipherCode(int c)
{
    static const char *const digits[10] = { "337f", "837f", "237f", "287f", "227f", "226f", "926f", "326f", "396f", "336f" };
    static const char *const letters[26] = {
        "afff", "9cff", "9bff", "96ff", "6fff", "2bff", "36ff", "2cff", "cfff", "8dff", "97ff", "806f", "dfff",
        "bfff", "37ff", "8bff", "3aff", "86ff", "26ff", "7fff", "27ff", "2aff", "87ff", "9aff", "9dff", "3cff",
    };
    if (c >= '0' && c <= '9')
        return digits[c - '0'];
    if (c >= 'A' && c <= 'Z')
        return letters[c - 'A'];
    if (c >= 'a' && c <= 'z')
        return letters[c - 'a'];
    return "ffff";
}

static const char *Dec20_CipherLetters(char digit)
{
    static const char *const letters[16] = { "AN", "BO", "ER", "FS", nullptr, nullptr, "GT", "HU",
                                             "CP", "DQ", "IV", "JW", "KX", "LY", nullptr, nullptr };
    const char *hex = "0123456789abcdef";
    const char *found = digit ? strchr(hex, digit) : nullptr;
    return found ? letters[found - hex] : nullptr;
}

static char Dec20_CipherDigit(char letter)
{
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 16; ++i)
    {
        const char *letters = Dec20_CipherLetters(hex[i]);
        if (letters && (letter == letters[0] || letter == letters[1]))
            return hex[i];
    }
    return 0;
}

static const char *Dec20_SkipSpaces(const char *text)
{
    while (*text == ' ')
        ++text;
    return text;
}

static void Dec20_Cmd_Encode(int, const char **args)
{
    for (const char *c = Dec20_SkipSpaces(*args); *c; ++c)
    {
        bool terminated = false;
        for (const char *digit = Dec20_CipherCode(uint8_t(*c)); *digit; ++digit)
        {
            if (const char *letters = Dec20_CipherLetters(*digit))
            {
                Dec20_QueueChar(letters[rand() & 1], 1, 1);
                terminated |= *digit == '6' || *digit == '7';
            }
        }
        if (!terminated)
            Dec20_QueueChar("MZ"[rand() & 1], 1, 1);
    }
    Dec20_QueueNewline();
}

static void Dec20_DecodeChar(const char *code)
{
    for (int c = 0; c < 128; ++c)
    {
        if (!strcmp(Dec20_CipherCode(c), code))
        {
            Dec20_QueueChar(c ? char(c) : ' ', 1, 1);
            return;
        }
    }
}

static void Dec20_Cmd_Decode(int, const char **args)
{
    char code[5] = "ffff";
    int count = 0;
    bool done = false;
    for (const char *c = Dec20_SkipSpaces(*args); *c; ++c)
    {
        const char letter = char(toupper(uint8_t(*c)));
        if (const char digit = Dec20_CipherDigit(letter))
        {
            code[count] = digit;
            if (digit == '6' || digit == '7')
            {
                done = true;
                Dec20_DecodeChar(code);
                I_strncpyz(code, "ffff", sizeof(code));
                count = 0;
                continue;
            }
            done = false;
            ++count;
        }
        if (count == 4 || letter == 'M' || letter == 'Z')
        {
            if (!done)
            {
                done = true;
                Dec20_DecodeChar(code);
            }
            else
            {
                Dec20_QueueChar(' ', 1, 1);
            }
            I_strncpyz(code, "ffff", sizeof(code));
            count = 0;
        }
    }
    Dec20_QueueNewline();
}

static void Dec20_Cmd_Alicia(int, const char **args)
{
    static const char *const replies[10] = {
        "Really?",
        "I'd be interested in hearing more about that.",
        "Do you feel that is the source of your problems?",
        "Does that question interest you?",
        "Can you tell me more?",
        "I'd like to hear more about that.",
        "Can you go into greater detail?",
        "I think you're on to something there!",
        "Tell me more about yourself.",
        "This is very interesting.",
    };
    Dec20Mode &mode = Dec20_Mode();
    if (mode.handler != Dec20_Cmd_Alicia)
    {
        Dec20_PrintLine("Welcome to Alicia, your virtual therapist.  (type quit to exit)");
        Dec20_Print(">");
        mode.handler = Dec20_Cmd_Alicia;
        return;
    }
    if (!I_stricmp(Com_Parse(args)->token, "quit"))
    {
        Dec20_PrintLine("It's been nice talking with you!  Goodbye!");
        mode.handler = nullptr;
        return;
    }
    Dec20_PrintLine(replies[rand() % 10]);
    Dec20_Print(">");
}

static void Dec20_Cmd_Hello(int localClientNum, const char **args)
{
    const char *token = Com_Parse(args)->token;
    if (!*token)
        Dec20_PrintError("Invalid Input - common usages include:\n\thello brother, hello nurse, and hello sailor");
    else if (!I_stricmp(token, "sailor"))
        Dec20_Cmd_Zork(localClientNum, args);
}

static void Dec20_Cmd_Help(int, const char **args)
{
    static const char *const lines[] = {
        "System Help:",
        "",
        "help\t\t\t    displays this help information",
        "cat                prints the contents of a file (.txt,.pic,.snd)",
        "cd [ |.|..|path]   changes the current directory.",
        "clear              clear the display",
        "dir                displays the contents of the current directory",
        "decode             decodes a encrypted string using an agency standard cypher",
        "encode             encodes a string using an agency standard cypher",
        "exit               exits the current login session",
        "help               display's this help screen",
        "login              starts a new login session on the current system",
        "mail               opens the current users mailbox",
        "rlogin [system]    attempts a login session on a remote system",
        "who                lists the users that have accounts on the current system",
    };
    if (*Com_Parse(args)->token)
        return;
    for (const char *line : lines)
        Dec20_PrintLine(line);
}

// Waits for a key press without printing a prompt afterwards.
void Dec20_Cmd_More(int, const char **)
{
    Dec20Mode &mode = Dec20_Mode();
    if (mode.handler == Dec20_Cmd_More)
    {
        mode.handler = g_dec20.savedHandler;
        g_dec20.savedHandler = nullptr;
        return;
    }
    g_dec20.savedHandler = mode.handler;
    mode.handler = Dec20_Cmd_More;
    Dec20_Print("--MORE--");
}

// Dead Ops Arcade.
static void Dec20_Cmd_Doa(int localClientNum, const char **)
{
    const int controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
    if (CL_ControllerIndex_GetSignInState(controllerIndex) <= 0)
    {
        UI_OpenMenu(localClientNum, "popmenu_content_warning_doa");
        return;
    }
    Dvar_SetBoolByName("zombietron_discovered", true);
    GamerProfile_UpdateProfileFromDvars(controllerIndex, PROFILE_WRITE_IF_CHANGED);
    Dec20_Exit(localClientNum);
    UI_LoadMaps();
    Cbuf_AddText(0, "map zombietron\n");
}

static void Dec20_Cmd_Foobar(int, const char **)
{
    Dec20_PrintLine("Fee Fie Foe Foo!");
}

// Developer cheats: build version, campaign/zombie map unlock, all intel.
static void Dec20_Cmd_3arc(int localClientNum, const char **args)
{
    const char *option = Com_Parse(args)->token;
    const int controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
    if (!I_stricmp(option, "version"))
    {
        Dec20_PrintLine(Com_GetBuildVersion());
    }
    else if (!I_stricmp(option, "unlock"))
    {
        Dvar_SetIntByName("mis_01", 30);
        Dvar_SetBoolByName("zombietron_discovered", true);
        Dvar_SetBoolByName("zombiefive_discovered", true);
        char command[] = "updategamerprofile";
        Cmd_ExecuteSingleCommand(localClientNum, controllerIndex, command);
        Dec20_PrintLine("Cheat Enabled!");
        UI_LoadMaps();
    }
    else if (!I_stricmp(option, "intel"))
    {
        char collectibles[50] = {};
        I_strncpyz(collectibles, bg_collectibles->current.string, sizeof(collectibles));
        for (int i = 0; i < 41; ++i)
            collectibles[i] = '1';
        Dvar_SetString((dvar_s *)bg_collectibles, collectibles);
        Cbuf_AddText(0, "updategamerprofile\n");
        GamerProfile_UpdateProfileFromDvars(controllerIndex, PROFILE_WRITE_IF_CHANGED);
        Dec20_PrintLine("Cheat Enabled!");
    }
}

struct Dec20Command
{
    const char *name;
    Dec20Handler function;
};

static const Dec20Command s_commands[] = {
    { "alarm", Dec20_Cmd_Nothing },  { "cat", Dec20_Cmd_Cat },       { "cd", Dec20_Cmd_Cd },
    { "clear", Dec20_Cmd_Clear },    { "cls", Dec20_Cmd_Clear },     { "dir", Dec20_Cmd_Dir },
    { "decode", Dec20_Cmd_Decode },  { "encode", Dec20_Cmd_Encode }, { "alicia", Dec20_Cmd_Alicia },
    { "exit", Dec20_Cmd_Exit },      { "hello", Dec20_Cmd_Hello },   { "help", Dec20_Cmd_Help },
    { "lights", Dec20_Cmd_Nothing }, { "login", Dec20_Cmd_Login },   { "ls", Dec20_Cmd_Dir },
    { "more", Dec20_Cmd_More },      { "mail", Dec20_Cmd_Mail },     { "print", Dec20_Cmd_Cat },
    { "rlogin", Dec20_Cmd_Rlogin },  { "type", Dec20_Cmd_Cat },      { "who", Dec20_Cmd_Who },
    { "doa", Dec20_Cmd_Doa },        { "zork", Dec20_Cmd_Zork },     { "foobar", Dec20_Cmd_Foobar },
    { "3arc", Dec20_Cmd_3arc },
};

// Shell commands are also passed to the level script (CodeCallback_Dec20Message).
// Quotes are swapped so the line stays one argument of the forwarded command.
static void Dec20_ForwardLine(int localClientNum)
{
    char line[DEC20_LINE_SIZE];
    I_strncpyz(line, g_dec20.line, sizeof(line));
    std::replace(std::begin(line), std::end(line), '"', '\'');
    Cbuf_AddText(localClientNum, va("cmd dec20 \"%s\"\n", line));
}

void Dec20_ProcessCommand(int localClientNum)
{
    const char *args = g_dec20.line;
    if (const Dec20Handler handler = Dec20_Mode().handler)
    {
        handler(localClientNum, &args);
    }
    else if (const char *name = Com_Parse(&args)->token; *name)
    {
        const auto command = std::find_if(std::begin(s_commands), std::end(s_commands),
                                          [name](const Dec20Command &entry) { return !I_stricmp(name, entry.name); });
        if (command != std::end(s_commands))
        {
            command->function(localClientNum, &args);
            Dec20_ForwardLine(localClientNum);
        }
        else
        {
            Dec20_PrintError("Unknown Command - try \"help\"");
        }
    }
    if (!Dec20_Mode().handler)
        Dec20_Print("$");
}
#endif
