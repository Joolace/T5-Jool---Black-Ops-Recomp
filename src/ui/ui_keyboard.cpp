#include "ui_keyboard.h"
#include <universal/assertive.h>
#include <qcommon/cmd.h>
#include <demo/demo_ui.h>
#include "ui_utils.h"
#include <qcommon/com_clients.h>
#include <qcommon/profanityfilter.h>
#include <stringed/stringed_hooks.h>
#ifdef KISAK_NX
#include <algorithm>
#include <stdio.h>
#include <switch.h>
#include <qcommon/common.h>
#include <ui/ui_main.h>
#include <universal/q_shared.h>
#endif

keyBoardUtitlity uiKeyboard;

int __cdecl UI_GetActiveKeyboardType()
{
    return uiKeyboard.keyboardType;
}

void __cdecl UI_SetActiveKeyboardType(int keyboardType)
{
    if ( uiKeyboard.keyboardType
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\ui\\ui_keyboard.cpp",
                    76,
                    0,
                    "%s",
                    "uiKeyboard.keyboardType == KEYBOARD_TYPE_NONE") )
    {
        __debugbreak();
    }
    if ( uiKeyboard.keyboardActive
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\ui\\ui_keyboard.cpp",
                    77,
                    0,
                    "%s",
                    "uiKeyboard.keyboardActive == false") )
    {
        __debugbreak();
    }
    uiKeyboard.keyboardType = keyboardType;
    uiKeyboard.keyboardActive = 1;
}

void __cdecl UI_ClearKeyboardActive()
{
    uiKeyboard.keyboardActive = 0;
    uiKeyboard.keyboardType = 0;
}

#ifdef KISAK_NX
static bool UI_NX_IsAllowedChar(unsigned char ch, UiNxTextFilter filter)
{
    if (ch < 32 || ch >= 127)
        return false;
    if (filter != UI_NX_TEXT_ANY && (ch == '@' || ch == '^'))
        return false;
    switch (filter)
    {
    case UI_NX_TEXT_SAFE:
        return !strchr("{}%\\;\"", ch);
    case UI_NX_TEXT_FILENAME:
        return I_isforfilename(ch);
    case UI_NX_TEXT_ALNUM:
        return I_isalnum(ch);
    case UI_NX_TEXT_DIGITS:
        return I_isdigit(ch);
    case UI_NX_TEXT_DECIMAL:
        return I_isdigit(ch) || ch == '.' || ch == static_cast<unsigned char>(Com_GetDecimalDelimiter());
    default:
        return true;
    }
}

static int UI_NX_EncodeNativeChar(const UiNxTextRequest &request, unsigned int code, char *encoded)
{
    if (!request.nativeText || (request.filter != UI_NX_TEXT_ANY && request.filter != UI_NX_TEXT_PLAIN
                                   && request.filter != UI_NX_TEXT_SAFE))
        return 0;
    // A Shift-JIS second byte in the ASCII range belongs to its letter, as the
    // renderer reads it; fields kept out of scripts refuse the ones they refuse
    // on their own.
    const int count = SEH_EncodeGameChar(code, encoded);
    if (count == 2 && request.filter == UI_NX_TEXT_SAFE && encoded[1] && strchr("{}\\", encoded[1]))
        return 0;
    return count;
}

static bool UI_NX_ConvertText(const UiNxTextRequest &request, int limit, const char *text, char *out,
    size_t outSize, char *message, size_t messageSize)
{
    size_t length = 0;
    for (const char *ch = text; *ch;)
    {
        const char *start = ch;
        const unsigned int code = SEH_ReadUtf8Char(&ch);
        char encoded[2];
        int count = 0;
        if (code < 0x80)
        {
            if (UI_NX_IsAllowedChar(static_cast<unsigned char>(code), request.filter))
                encoded[count++] = static_cast<char>(code);
        }
        else
            count = UI_NX_EncodeNativeChar(request, code, encoded);
        if (!count)
        {
            char character[5]{};
            memcpy(character, start, std::min<size_t>(ch - start, 4));
            snprintf(message, messageSize, SEH_GetPortString("OPENBLOPS_KEYBOARD_INVALID_CHAR", true), character);
            return false;
        }
        if (length + count > static_cast<size_t>(limit) || length + count >= outSize)
        {
            snprintf(message, messageSize, "%s", SEH_GetPortString("OPENBLOPS_KEYBOARD_TOO_LONG", true));
            return false;
        }
        if (request.filter == UI_NX_TEXT_DECIMAL && encoded[0] == Com_GetDecimalDelimiter())
            encoded[0] = '.';
        memcpy(out + length, encoded, count);
        length += count;
    }
    out[length] = 0;
    return true;
}

static const UiNxTextRequest *s_nxTextRequest;
static int s_nxTextLimit;

static SwkbdTextCheckResult UI_NX_CheckText(char *text, size_t size)
{
    char game[512];
    return UI_NX_ConvertText(*s_nxTextRequest, s_nxTextLimit, text, game, sizeof(game), text, size)
        ? SwkbdTextCheckResult_OK : SwkbdTextCheckResult_Bad;
}

bool UI_NX_EditText(const UiNxTextRequest &request, char *out, size_t outSize)
{
    if (!out || outSize < 2)
        return false;
    SwkbdConfig keyboard{};
    if (R_FAILED(swkbdCreate(&keyboard, 0)))
    {
        Com_PrintWarning(13, "Could not open the Switch software keyboard.\n");
        return false;
    }
    if (request.password)
        swkbdConfigMakePresetPassword(&keyboard);
    else
        swkbdConfigMakePresetDefault(&keyboard);
    if (request.filter == UI_NX_TEXT_DIGITS || request.filter == UI_NX_TEXT_DECIMAL)
        swkbdConfigSetType(&keyboard, SwkbdType_NumPad);
    if (request.filter == UI_NX_TEXT_DECIMAL)
        swkbdConfigSetLeftOptionalSymbolKey(&keyboard, ".");

    char utf8[1024];
    if (request.header && *request.header)
    {
        if (request.headerIsUtf8)
            swkbdConfigSetHeaderText(&keyboard, request.header);
        else if (SEH_GameTextToUtf8(request.header, utf8, sizeof(utf8)))
            swkbdConfigSetHeaderText(&keyboard, utf8);
    }
    SEH_GameTextToUtf8(request.initialText, utf8, sizeof(utf8));
    swkbdConfigSetInitialText(&keyboard, utf8);
    const int limit = std::clamp(std::min(request.maxChars, static_cast<int>(outSize) - 1), 1, 255);
    swkbdConfigSetStringLenMax(&keyboard, limit);
    s_nxTextRequest = &request;
    s_nxTextLimit = limit;
    swkbdConfigSetTextCheckCallback(&keyboard, UI_NX_CheckText);

    char value[1024]{};
    const Result result = swkbdShow(&keyboard, value, sizeof(value));
    swkbdClose(&keyboard);
    char message[256];
    return R_SUCCEEDED(result) && UI_NX_ConvertText(request, limit, value, out, outSize, message, sizeof(message));
}

static void UI_NX_OpenVirtualKeyboard(int keyboardType, const char *title, const char *initialText, int size)
{
    if (uiKeyboard.keyboardActive || size < 2)
        return;
    UiNxTextRequest request;
    request.header = title && *title == '@' ? UI_SafeTranslateString(title + 1) : title;
    request.initialText = initialText;
    request.maxChars = size - 1;
    request.filter = UI_NX_TEXT_SAFE;
    char value[256];
    if (!UI_NX_EditText(request, value, sizeof(value)))
        return;

    UI_SetActiveKeyboardType(keyboardType);
    Dvar_SetStringByName("ui_keyboard_dvar_new", value);
    UI_KeyboardComplete(0);
}
#endif

void __cdecl UI_Keyboard_New_f()
{
    const char *v0; // eax
    int keyboardType; // [esp+4Ch] [ebp-10h]
#ifndef KISAK_NX
    char *defaultString; // [esp+54h] [ebp-8h]
    char *title; // [esp+58h] [ebp-4h]

    title = 0;
    defaultString = 0;
#endif
    if ( Cmd_Argc() == 2 || Cmd_Argc() == 5 )
    {
        v0 = Cmd_Argv(1);
        keyboardType = atoi(v0);
        if ( keyboardType > 0 && keyboardType < 3 )
        {
#ifdef KISAK_NX
            if (keyboardType == 1)
                UI_NX_OpenVirtualKeyboard(keyboardType, Demo_GetKeyboardTitle(), Demo_GetKeyboardDefaultText(),
                    Demo_GetKeyboardTextSize());
            else if (Cmd_Argc() == 5)
                UI_NX_OpenVirtualKeyboard(keyboardType, Cmd_Argv(2), Cmd_Argv(3), atoi(Cmd_Argv(4)));
#else
            if ( keyboardType == 1 )
            {
                title = (char *)Demo_GetKeyboardTitle();
                defaultString = (char *)Demo_GetKeyboardDefaultText();
                Demo_GetKeyboardTextSize();
            }
            strlen(SEH_LocalizeTextMessage(title, "menu prompt", LOCMSG_SAFE));
            UI_SetActiveKeyboardType(keyboardType);
            Dvar_SetStringByName("ui_keyboardtitle", title);
            Dvar_SetStringByName("ui_keyboard_dvar_edit", defaultString);
            UI_OpenMenu(0, "popmenu_ui_keyboard_text");
#endif
        }
        else
        {
            Com_Printf(16, "Unsupported keyboard type %d.\n", keyboardType);
        }
    }
    else
    {
        Com_Printf(16, "Usage: ui_keyboard_new <type>\n OR \n ui_keyboard_new <type> <title> <defaultString> <size>.\n");
    }
}

void __cdecl UI_KeyboardComplete(int localClientNum)
{
    const char *String; // eax
    int ControllerIndex; // eax
    const char *errorString; // [esp+4h] [ebp-414h]
    char textReturned[1028]; // [esp+8h] [ebp-410h] BYREF
    bool errorReported; // [esp+413h] [ebp-5h]
    int localControllerIndex; // [esp+414h] [ebp-4h]

    errorReported = 0;
    errorString = (char *)"";
    String = Dvar_GetString("ui_keyboard_dvar_new");
    Com_sprintf(textReturned, 0x400u, "%s", String);
    if ( UI_VerifyString(textReturned) )
    {
        if ( textReturned[0] )
        {
            ControllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
            if ( isBadWord(ControllerIndex, textReturned) )
            {
                errorReported = 1;
                errorString = "MENU_OFFENSIVETEXT";
            }
        }
    }
    else
    {
        errorReported = 1;
        errorString = "MENU_INVALIDCHARS";
    }
    if ( !errorReported || UI_GetActiveKeyboardType() == 1 )
    {
        localControllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
        if ( UI_GetActiveKeyboardType() == 1 )
            Demo_KeyboardComplete(localClientNum, textReturned, errorReported, (char*)errorString);
        UI_ClearKeyboardActive();
    }
    else
    {
        Com_SetErrorMessage((char *)errorString);
        UI_OpenMenu(localClientNum, "error_popmenu_party");
        UI_ClearKeyboardActive();
    }
}

void __cdecl UI_Keyboard_Complete_f()
{
    UI_KeyboardComplete(0);
}

void __cdecl UI_Clear_Keyboard_Active_f()
{
    UI_ClearKeyboardActive();
}

