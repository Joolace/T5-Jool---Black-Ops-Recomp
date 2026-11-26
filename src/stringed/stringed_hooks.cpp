#include "stringed_hooks.h"
#include <universal/dvar.h>
#include <universal/com_files.h>
#include <qcommon/common.h>
#include "stringed_ingame.h"
#include "stringed_remote.h"
#include <ui/ui_shared.h>
#include <ui/ui_main.h>
#include <database/db_registry.h>
#include <win32/win_localize.h>

const dvar_t *loc_language;
const dvar_t *loc_forceEnglish;
const dvar_t *loc_translate;
const dvar_t *loc_warnings;
const dvar_t *loc_warningsAsErrors;

int g_currentAsian;

char szStrings[10][1024];
int iCurrString;

languageInfo_t g_languages[] =
{
  { "english", "en_", 0 },
  { "french", "fr_", 0 },
  { "frenchcan", "fc_", 0 },
  { "german", "ge_", 0 },
  { "austrian", "ge_", 0 },
  { "italian", "it_", 0 },
  { "spanish", "sp_", 0 },
  { "british", "br_", 0 },
  { "russian", "ru_", 0 },
  { "polish", "po_", 0 },
  { "korean", "ko_", 0 },
  { "japanese", "ja_", 0 },
  { "czech", "cz_", 0 },
#ifdef KISAK_SP
  { "fulljap", "fj_", 0 }
#endif
};

static const int SEH_LANGUAGE_COUNT = ARRAY_COUNT(g_languages);

namespace
{
struct PortString
{
    const char *reference;
    const char *game[10];
    const char *utf8[10];
};

constexpr PortString portStrings[] = {
#include "port_ui_translations.inc"
};

int SEH_PortLanguage()
{
    constexpr int columns[] = {0, 1, 1, 2, 2, 3, 4, 0, 5, 6, 7, 8, 9, 8};
    static_assert(ARRAY_COUNT(columns) >= SEH_LANGUAGE_COUNT);
    if (!loc_language || (loc_forceEnglish && loc_forceEnglish->current.enabled))
        return 0;
    const unsigned int language = SEH_GetCurrentLanguage();
    return language < ARRAY_COUNT(columns) ? columns[language] : 0;
}
}

const char *SEH_GetPortString(const char *reference, bool utf8)
{
    if (strncmp(reference, "OPENBLOPS_", 10))
        return nullptr;
    for (const PortString &entry : portStrings)
        if (!strcmp(reference, entry.reference))
            return (utf8 ? entry.utf8 : entry.game)[SEH_PortLanguage()];
    return reference;
}

namespace
{
#include "game_encodings.inc"

enum class GameEncoding { Western, Cyrillic, CentralEuropean, Korean, Japanese };

GameEncoding SEH_PortEncoding()
{
    switch (SEH_PortLanguage())
    {
    case 5: return GameEncoding::Cyrillic;
    case 6: case 9: return GameEncoding::CentralEuropean;
    case 7: return GameEncoding::Korean;
    case 8: return GameEncoding::Japanese;
    default: return GameEncoding::Western;
    }
}

const unsigned short *SEH_SingleByteTable(GameEncoding encoding)
{
    switch (encoding)
    {
    case GameEncoding::Cyrillic: return cp1251High;
    case GameEncoding::CentralEuropean: return cp1250High;
    case GameEncoding::Japanese: return cp932High;
    case GameEncoding::Korean: return nullptr;
    default: return cp1252High;
    }
}

int SEH_ShiftJisIndex(unsigned char lead, unsigned char trail)
{
    int row, column;
    if (lead >= 0x81 && lead <= 0x9F) row = lead - 0x81;
    else if (lead >= 0xE0 && lead <= 0xEF) row = lead - 0xE0 + 31;
    else return -1;
    if (trail >= 0x40 && trail <= 0x7E) column = trail - 0x40;
    else if (trail >= 0x80 && trail <= 0xFC) column = trail - 0x80 + 63;
    else return -1;
    return row * 188 + column;
}

unsigned int SEH_DecodeGameChar(GameEncoding encoding, const unsigned char *text, int *length)
{
    *length = 1;
    const unsigned char lead = text[0];
    if (lead < 0x80)
        return lead;
    if (encoding == GameEncoding::Korean && lead >= 0xB0 && lead <= 0xC8 && text[1] > 0xA0 && text[1] != 0xFF)
    {
        *length = 2;
        return koreanHangul[(lead - 0xB0) * 94 + text[1] - 0xA1];
    }
    if (encoding == GameEncoding::Japanese)
    {
        const int index = SEH_ShiftJisIndex(lead, text[1]);
        if (index >= 0)
        {
            *length = 2;
            return japaneseShiftJis[index];
        }
    }
    const unsigned short *table = SEH_SingleByteTable(encoding);
    return table ? table[lead - 0x80] : 0;
}

size_t SEH_WriteUtf8(unsigned int code, char *out)
{
    if (code < 0x80)
    {
        out[0] = static_cast<char>(code);
        return 1;
    }
    if (code < 0x800)
    {
        out[0] = static_cast<char>(0xC0 | (code >> 6));
        out[1] = static_cast<char>(0x80 | (code & 0x3F));
        return 2;
    }
    out[0] = static_cast<char>(0xE0 | (code >> 12));
    out[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out[2] = static_cast<char>(0x80 | (code & 0x3F));
    return 3;
}
}

bool SEH_GameTextToUtf8(const char *text, char *out, size_t size)
{
    if (!size)
        return false;
    const GameEncoding encoding = SEH_PortEncoding();
    bool exact = true;
    size_t length = 0;
    const unsigned char *ch = reinterpret_cast<const unsigned char *>(text ? text : "");
    while (*ch)
    {
        int used;
        unsigned int code = SEH_DecodeGameChar(encoding, ch, &used);
        ch += used;
        if (!code)
        {
            code = '?';
            exact = false;
        }
        char encoded[3];
        const size_t count = SEH_WriteUtf8(code, encoded);
        if (length + count >= size)
        {
            exact = false;
            break;
        }
        memcpy(out + length, encoded, count);
        length += count;
    }
    out[length] = 0;
    return exact;
}

unsigned int SEH_ReadUtf8Char(const char **text)
{
    const unsigned char *s = reinterpret_cast<const unsigned char *>(*text);
    if (s[0] < 0x80)
    {
        *text += 1;
        return s[0];
    }
    const int trailing = s[0] >= 0xF8 ? -1 : s[0] >= 0xF0 ? 3 : s[0] >= 0xE0 ? 2 : s[0] >= 0xC0 ? 1 : -1;
    if (trailing < 0)
    {
        *text += 1;
        return 0xFFFD;
    }
    unsigned int code = s[0] & (0x3F >> trailing);
    for (int i = 1; i <= trailing; ++i)
    {
        if ((s[i] & 0xC0) != 0x80)
        {
            *text += i;
            return 0xFFFD;
        }
        code = (code << 6) | (s[i] & 0x3F);
    }
    *text += trailing + 1;
    return code;
}

int SEH_EncodeGameChar(unsigned int code, char *out)
{
    if (!code)
        return 0;
    if (code < 0x80)
    {
        out[0] = static_cast<char>(code);
        return 1;
    }
    if (code > 0xFFFF)
        return 0;
    const GameEncoding encoding = SEH_PortEncoding();
    if (const unsigned short *table = SEH_SingleByteTable(encoding))
    {
        for (int i = 0; i < 128; ++i)
        {
            if (table[i] == code)
            {
                out[0] = static_cast<char>(0x80 + i);
                return 1;
            }
        }
    }
    if (encoding == GameEncoding::Korean)
    {
        for (int i = 0; i < static_cast<int>(ARRAY_COUNT(koreanHangul)); ++i)
        {
            if (koreanHangul[i] == code)
            {
                out[0] = static_cast<char>(0xB0 + i / 94);
                out[1] = static_cast<char>(0xA1 + i % 94);
                return 2;
            }
        }
    }
    if (encoding == GameEncoding::Japanese)
    {
        for (int i = 0; i < static_cast<int>(ARRAY_COUNT(japaneseShiftJis)); ++i)
        {
            if (japaneseShiftJis[i] == code)
            {
                const int lead = i / 188, trail = i % 188;
                out[0] = static_cast<char>(lead < 31 ? 0x81 + lead : 0xE0 + lead - 31);
                out[1] = static_cast<char>(trail < 63 ? 0x40 + trail : 0x80 + trail - 63);
                return 2;
            }
        }
    }
    return 0;
}

bool SEH_Utf8ToGameText(const char *text, char *out, size_t size)
{
    if (!size)
        return false;
    bool exact = true;
    size_t length = 0;
    const char *ch = text ? text : "";
    while (*ch)
    {
        char encoded[2];
        int count = SEH_EncodeGameChar(SEH_ReadUtf8Char(&ch), encoded);
        if (!count)
        {
            encoded[0] = '?';
            count = 1;
            exact = false;
        }
        if (length + count >= size)
        {
            exact = false;
            break;
        }
        memcpy(out + length, encoded, count);
        length += count;
    }
    out[length] = 0;
    return exact;
}

const char *SEH_LocalizePortText(const char *english)
{
    for (const PortString &entry : portStrings)
        if (!strcmp(english, entry.game[0]))
            return entry.game[SEH_PortLanguage()];
    return english;
}

int __cdecl SEH_GetCurrentLanguage()
{
    return loc_language->current.integer;
}

void __cdecl SEH_InitLanguage()
{
#ifdef KISAK_SP
    loc_language = _Dvar_RegisterInt("loc_language", 0, 0, SEH_LANGUAGE_COUNT - 1, 0x21u, "");
    loc_forceEnglish = _Dvar_RegisterBool("loc_forceEnglish", 0, 0x21u, "");
    loc_translate = _Dvar_RegisterBool("loc_translate", 1, 0x20u, "");
    loc_warnings = _Dvar_RegisterBool("loc_warnings", 0, 0, "");
    loc_warningsAsErrors = _Dvar_RegisterBool("loc_warningsAsErrors", 0, 0, "");
    int language;
    SEH_GetLanguageIndexForName(Win_GetLanguage(), &language);
    Dvar_SetInt((dvar_s *)loc_language, language);
#else
    loc_language = _Dvar_RegisterInt("loc_language", 0, 0, 12, 0x21u, "Language");
    loc_forceEnglish = _Dvar_RegisterBool("loc_forceEnglish", 0, 0x21u, "Force english localized strings");
    loc_translate = _Dvar_RegisterBool("loc_translate", 1, 0x20u, "Enable translations");
    loc_warnings = _Dvar_RegisterBool("loc_warnings", 0, 0, "Enable localization warnings");
    loc_warningsAsErrors = _Dvar_RegisterBool("loc_warningsAsErrors", 0, 0, "Throw an error for any unlocalized string");
    // The installed language (Win_PickInstalledLanguage), as SP does above.
    int language;
    if (SEH_GetLanguageIndexForName(Win_GetLanguage(), &language))
        Dvar_SetInt((dvar_s *)loc_language, language);
#endif
    SEH_UpdateCurrentLanguage();
}

// The config's archived loc_language runs after SEH_InitLanguage; it must not
// point the localized zones away from the installed language.
void SEH_ApplyInstalledLanguage()
{
    int language;
    if (!loc_language || !SEH_GetLanguageIndexForName(Win_GetLanguage(), &language)
        || loc_language->current.integer == language)
        return;
    Com_Printf(10, "loc_language %i is not the installed language; using %i (%s)\n",
               loc_language->current.integer, language, Win_GetLanguage());
    Dvar_SetInt((dvar_s *)loc_language, language);
    SEH_UpdateCurrentLanguage();
}

void SEH_UpdateCurrentLanguage()
{
    switch ( loc_language->current.integer )
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 8:
            Dvar_SetBoolByName("cg_subtitles", 0);
            break;
        case 0xA:
        case 0xB:
#ifdef KISAK_SP
        case 0xD:
#endif
            g_currentAsian = 1;
            break;
        default:
            g_currentAsian = 0;
            break;
    }
#ifdef KISAK_SP
    if (loc_language->current.integer == 13)
        Dvar_SetStringByName("language", (char *)g_languages[11].pszName);
    else
#endif
        Dvar_SetStringByName("language", (char *)g_languages[loc_language->current.integer].pszName);
}

void __cdecl SEH_UpdateLanguageInfo()
{
    int iNumLanguages; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]
    int ia; // [esp+4h] [ebp-4h]

#ifdef KISAK_SP
    _Dvar_RegisterInt(loc_language->name, 0, 0, SEH_LANGUAGE_COUNT - 1, 0x21u, "");
    _Dvar_RegisterBool(loc_forceEnglish->name, 0, 0x21u, "");
#else
    _Dvar_RegisterInt(loc_language->name, 0, 0, 12, 0x21u, "Language");
    _Dvar_RegisterBool(loc_forceEnglish->name, 0, 0x21u, "Force english language");
#endif
    SEH_UpdateCurrentLanguage();
    iNumLanguages = 0;
    for ( i = 0; i < SEH_LANGUAGE_COUNT; ++i )
    {
        if ( FS_LanguageHasAssets(i) )
        {
            g_languages[i].bPresent = 1;
            ++iNumLanguages;
        }
        else
        {
            g_languages[i].bPresent = 0;
        }
    }
    if ( iNumLanguages < 1 )
        Com_PrintError(16, "ERROR: No languages available because no localized assets were found\n");
    if ( !SEH_StringEd_SetLanguageStrings(loc_language->current.unsignedInt) )
    {
        for ( ia = 0; ia < SEH_LANGUAGE_COUNT; ++ia )
        {
            Dvar_SetInt((dvar_s *)loc_language, ia);
            SEH_UpdateCurrentLanguage();
            if ( SEH_StringEd_SetLanguageStrings(ia) )
                return;
        }
        Dvar_SetInt((dvar_s *)loc_language, 0);
        SEH_UpdateCurrentLanguage();
    }
}

int __cdecl SEH_VerifyLanguageSelection(int iLanguageSelection)
{
    int i; // [esp+0h] [ebp-4h]

    if ( g_languages[iLanguageSelection].bPresent )
        return iLanguageSelection;
    for ( i = 0; i < SEH_LANGUAGE_COUNT; ++i )
    {
        if ( g_languages[(i + iLanguageSelection) % SEH_LANGUAGE_COUNT].bPresent )
            return (i + iLanguageSelection) % SEH_LANGUAGE_COUNT;
    }
    return 0;
}

void __cdecl SEH_Init_StringEd()
{
    SE_Init();
}

void __cdecl SEH_Shutdown_StringEd()
{
    SE_ShutDown();
}

int __cdecl SEH_StringEd_SetLanguageStrings(unsigned int iLanguage)
{
    const char *LanguageName; // eax
    const char *v3; // eax
    char *pszError; // [esp+0h] [ebp-4h]

    if ( !g_languages[iLanguage].bPresent )
        return 0;
    pszError = SE_LoadLanguage(loc_forceEnglish->current.enabled);
    if ( !pszError )
        return 1;
    if ( !fs_ignoreLocalized->current.enabled && loc_warnings->current.enabled )
    {
        if ( loc_warningsAsErrors->current.enabled )
        {
            LanguageName = SEH_GetLanguageName(iLanguage);
            Com_Error(ERR_LOCALIZATION, "Could not load localization strings for %s: %s", LanguageName, pszError);
        }
        else
        {
            v3 = SEH_GetLanguageName(iLanguage);
            Com_PrintWarning(16, "WARNING: Could not load localization strings for %s: %s\n", v3, pszError);
        }
    }
    return 1;
}

const char *__cdecl SE_GetString(char *psPackageAndStringReference)
{
    if ( useFastFile->current.enabled )
        return SE_GetString_FastFile(psPackageAndStringReference);
    else
        return (const char *)SE_GetString_LoadObj(psPackageAndStringReference);
}

const char *__cdecl SE_GetString_FastFile(char *psPackageAndStringReference)
{
    LocalizeEntry *localize; // [esp+8h] [ebp-8h]
    const char *value; // [esp+Ch] [ebp-4h]

    value = GetString(psPackageAndStringReference);
    if ( value )
        return value;
    localize = DB_FindXAssetHeader(ASSET_TYPE_LOCALIZE_ENTRY, psPackageAndStringReference, 1, -1).localize;
    if ( localize )
        return localize->value;
    else
        return 0;
}

const char *__cdecl SEH_StringEd_GetString(char *pszReference)
{
    if ( !loc_translate || !loc_translate->current.enabled )
        return pszReference;
    if (const char *translation = SEH_GetPortString(pszReference))
        return translation;
    if ( *pszReference && pszReference[1] )
        return SE_GetString(pszReference);
    return pszReference;
}

char szErrorString[1024];
char *__cdecl SEH_SafeTranslateString(const char *pszReference)
{
    const char *pszTranslation; // [esp+0h] [ebp-4h]

    pszTranslation = SEH_StringEd_GetString((char*)pszReference);
    if ( !pszTranslation )
    {
        if ( loc_warnings->current.enabled )
        {
            if ( loc_warningsAsErrors->current.enabled )
                Com_Error(ERR_LOCALIZATION, "Could not translate exe string \"%s\"", pszReference);
            else
                Com_PrintWarning(16, "WARNING: Could not translate exe string \"%s\"\n", pszReference);
            strcpy(szErrorString, "^1UNLOCALIZED(^7");
            I_strncat(szErrorString, 1024, pszReference);
            I_strncat(szErrorString, 1024, "^1)^7");
        }
        else
        {
            I_strncpyz(szErrorString, pszReference, 1024);
        }
        return szErrorString;
    }
    return (char *)pszTranslation;
}

void __cdecl SEH_LocalizeTextMessageWithBindings(
    int localClientNum, const char *input, const char *messageType, char *output, unsigned int size)
{
    const char *localized = SEH_LocalizeTextMessage(input, messageType, LOCMSG_SAFE);
    unsigned int length = strlen(localized);
    if (length + 1 <= size)
    {
        memcpy(output, localized, length);
        output[length] = 0;
        unsigned int searchPos = 0;
        while (SEH_ReplaceDirective(localClientNum, &searchPos, &length, output, size))
            ;
    }
}

int __cdecl SEH_ReplaceDirectiveInStringWithBinding(int localClientNum, char *translatedString, char *finalString)
{
    const char *v3; // eax
    unsigned int stringLen; // [esp+10h] [ebp-8h] BYREF
    unsigned int searchPos; // [esp+14h] [ebp-4h] BYREF

    stringLen = strlen(translatedString);
    if ( stringLen + 1 <= 0x400 )
    {
        memcpy((unsigned __int8 *)finalString, (unsigned __int8 *)translatedString, stringLen);
        finalString[stringLen] = 0;
        searchPos = 0;
        while ( SEH_ReplaceDirective(localClientNum, &searchPos, &stringLen, finalString, 0x400u) )
            ;
        return 1;
    }
    else
    {
        v3 = va("Translated string too long to process\n");
        Com_Error(ERR_DROP, v3);
        return 0;
    }
}

char __cdecl SEH_ReplaceDirective(
                int localClientNum,
                unsigned int *searchPos,
                unsigned int *dstLen,
                char *dstString,
                unsigned int size)
{
    char *v5; // eax
    const char *v7; // eax
    const char *v8; // eax
    const char *v9; // eax
    const char *v10; // eax
    signed int directiveLen; // [esp+34h] [ebp-624h]
    const char *startTokenPos; // [esp+38h] [ebp-620h]
    unsigned int newStringLen; // [esp+3Ch] [ebp-61Ch]
    unsigned int bindingLen; // [esp+40h] [ebp-618h]
    unsigned int endLen; // [esp+44h] [ebp-614h]
    char keyBinding[256]; // [esp+48h] [ebp-610h] BYREF
    unsigned __int8 srcString[1028]; // [esp+148h] [ebp-510h] BYREF
    int beginLen; // [esp+54Ch] [ebp-10Ch]
    char directive[256]; // [esp+550h] [ebp-108h] BYREF
    const char *endTokenPos; // [esp+654h] [ebp-4h]
    char *dstStringa; // [esp+66Ch] [ebp+14h]
    unsigned __int8 *dstStringb; // [esp+66Ch] [ebp+14h]

    if ( size > 0x400
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\stringed\\stringed_hooks.cpp",
                    535,
                    0,
                    "%s\n\t(size) = %i",
                    "(size <= 1024)",
                    size) )
    {
        __debugbreak();
    }
    if ( *dstLen >= 0x400
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\stringed\\stringed_hooks.cpp",
                    537,
                    0,
                    "*dstLen doesn't index MAX_STRING_CHARS\n\t%i not in [0, %i)",
                    *dstLen,
                    1024) )
    {
        __debugbreak();
    }
    if ( *searchPos > *dstLen
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\stringed\\stringed_hooks.cpp",
                    538,
                    0,
                    "*searchPos <= *dstLen\n\t%i, %i",
                    *searchPos,
                    *dstLen) )
    {
        __debugbreak();
    }
    if ( *dstLen != strlen(dstString)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\stringed\\stringed_hooks.cpp",
                    540,
                    0,
                    "*dstLen == strlen( dstString )\n\t%i, %i",
                    *dstLen,
                    strlen(dstString)) )
    {
        __debugbreak();
    }
    memcpy(srcString, (unsigned __int8 *)dstString, *dstLen);
    srcString[*dstLen] = 0;
    v5 = strstr((char*) & srcString[*searchPos], "[{");
    startTokenPos = (const char *)v5;
    if ( !v5 )
        return 0;
    v7 = strstr(v5, "}]");
    endTokenPos = v7;
    if ( v7 )
    {
        directiveLen = endTokenPos - startTokenPos - 2;
        if ( directiveLen < 0
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\stringed\\stringed_hooks.cpp",
                        559,
                        1,
                        "%s\n\t(directiveLen) = %i",
                        "(directiveLen >= 0)",
                        directiveLen) )
        {
            __debugbreak();
        }
        if ( directiveLen )
        {
            memcpy((unsigned __int8 *)directive, (unsigned __int8 *)startTokenPos + 2, directiveLen);
            directive[directiveLen] = 0;
            SEH_GetBindingForDirective(localClientNum, directive, keyBinding);
            bindingLen = &keyBinding[strlen(keyBinding) + 1] - &keyBinding[1];
            newStringLen = *dstLen - directiveLen + bindingLen - 4;
            if ( newStringLen + 1 <= size )
            {
                beginLen = startTokenPos - (const char *)srcString;
                dstStringa = &dstString[startTokenPos - (const char *)srcString];
                memcpy((unsigned __int8 *)dstStringa, (unsigned __int8 *)keyBinding, bindingLen);
                dstStringb = (unsigned __int8 *)&dstStringa[bindingLen];
                endLen = newStringLen - beginLen - bindingLen;
                memcpy(dstStringb, (unsigned __int8 *)endTokenPos + 2, endLen);
                dstStringb[endLen] = 0;
                *searchPos = bindingLen + beginLen;
                *dstLen = newStringLen;
                if ( *searchPos > *dstLen
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\stringed\\stringed_hooks.cpp",
                                597,
                                1,
                                "*searchPos <= *dstLen\n\t%i, %i",
                                *searchPos,
                                *dstLen) )
                {
                    __debugbreak();
                }
                return 1;
            }
            else
            {
#ifndef KISAK_SP
                v10 = va("String too long to add key binding: %s\n", dstString);
                Com_Error(ERR_DROP, v10);
#endif
                return 0;
            }
        }
        else
        {
#ifndef KISAK_SP
            v9 = va("Directive empty in string '%s'", dstString);
            Com_Error(ERR_DROP, v9);
#endif
            return 0;
        }
    }
    else
    {
#ifndef KISAK_SP
        v8 = va("No end token to match begin token in string '%s'", dstString);
        Com_Error(ERR_DROP, v8);
#endif
        return 0;
    }
}

void __cdecl SEH_GetBindingForDirective(int localClientNum, char *directive, char *result)
{
    char *v3; // eax
    char arg0[256]; // [esp+0h] [ebp-208h] BYREF
    char name[260]; // [esp+100h] [ebp-108h] BYREF

    if ( UI_GetKeyBindingLocalizedString(localClientNum, directive, result, 0) )
    {
        UI_FilterStringForButtonAnimation(result, 0x100u);
    }
    else
    {
        SEH_ParseDirective(directive, name, arg0);
#ifdef KISAK_NX
        if (UI_GetSticksTranslatedString(localClientNum, name, result))
            return;
#endif
        if ( !I_stricmp(name, "gocrouch")
            && (UI_GetKeyBindingLocalizedString(localClientNum, "togglecrouch", result, 0)
             || UI_GetKeyBindingLocalizedString(localClientNum, "+movedown", result, 0)) )
        {
            UI_FilterStringForButtonAnimation(result, 0x100u);
        }
        else
        {
            v3 = UI_SafeTranslateString("KEY_UNBOUND");
            Com_sprintf(result, 0x100u, "\"%s\"", v3);
        }
    }
}

void __cdecl SEH_ParseDirective(char *directive, char *resultName, char *resultArg0)
{
    const char *v3; // eax
    const char *argpos; // [esp+4h] [ebp-4h]

    v3 = strstr((char*)directive, ":");
    argpos = v3;
    if ( v3 )
    {
        memcpy((unsigned __int8 *)resultName, (unsigned __int8 *)directive, v3 - directive);
        resultName[argpos - directive] = 0;
        I_strncpyz(resultArg0, argpos + 1, 256);
    }
    else
    {
        I_strncpyz(resultName, directive, 256);
        *resultArg0 = 0;
    }
}

char *__cdecl SEH_LocalizeTextMessage(const char *pszInputBuffer, const char *pszMessageType, msgLocErrType_t errType)
{
    char v4; // [esp+3h] [ebp-885h]
    char *v5; // [esp+8h] [ebp-880h]
    char *v6; // [esp+Ch] [ebp-87Ch]
    char v7; // [esp+13h] [ebp-875h]
    char *v8; // [esp+18h] [ebp-870h]
    char *v9; // [esp+1Ch] [ebp-86Ch]
    char v10; // [esp+23h] [ebp-865h]
    char *v11; // [esp+28h] [ebp-860h]
    char *v12; // [esp+2Ch] [ebp-85Ch]
    char v13; // [esp+33h] [ebp-855h]
    char *v14; // [esp+38h] [ebp-850h]
    char *v15; // [esp+3Ch] [ebp-84Ch]
    char szInsertBuf[1024]; // [esp+50h] [ebp-838h] BYREF
    char szTokenBuf[1028]; // [esp+450h] [ebp-438h] BYREF
    int bLocOn; // [esp+858h] [ebp-30h]
    int iTokenLen; // [esp+85Ch] [ebp-2Ch]
    int iInsertLevel; // [esp+860h] [ebp-28h]
    int iLen; // [esp+864h] [ebp-24h]
    int bInsertEnabled; // [esp+868h] [ebp-20h]
    int insertIndex; // [esp+86Ch] [ebp-1Ch]
    const char *pszIn; // [esp+870h] [ebp-18h]
    int bLocSkipped; // [esp+874h] [ebp-14h]
    const char *pszTokenStart; // [esp+878h] [ebp-10h]
    int i; // [esp+87Ch] [ebp-Ch]
    char *pszString; // [esp+880h] [ebp-8h]
    int digit; // [esp+884h] [ebp-4h]

    iCurrString = (iCurrString + 1) % 10;
    memset(szStrings[iCurrString], 0, sizeof(char[1024]));
    pszString = szStrings[iCurrString];
    iLen = 0;
    bLocOn = 1;
    bInsertEnabled = 1;
    iInsertLevel = 0;
    insertIndex = 1;
    bLocSkipped = 0;
    pszTokenStart = pszInputBuffer;
    pszIn = pszInputBuffer;
    while ( *pszTokenStart )
    {
        if ( !*pszIn || *pszIn == 20 || *pszIn == 21 || *pszIn == 22 )
        {
            if ( pszIn > pszTokenStart )
            {
                iTokenLen = pszIn - pszTokenStart;
                I_strncpyz(szTokenBuf, pszTokenStart, pszIn - pszTokenStart + 1);
                if ( bLocOn )
                {
                    if ( !SEH_GetLocalizedTokenReference(szTokenBuf, szTokenBuf, pszMessageType, errType) )
                        return 0;
                    iTokenLen = &szTokenBuf[strlen(szTokenBuf) + 1] - &szTokenBuf[1];
                }
                if ( iTokenLen + iLen >= 1024 )
                {
                    if ( loc_warnings
                        && loc_warnings->current.enabled
                        && loc_warningsAsErrors
                        && loc_warningsAsErrors->current.enabled
                        && errType != LOCMSG_NOERR )
                    {
                        Com_Error(ERR_DROP, "%s too long when translated: \"%s\"", pszMessageType, pszInputBuffer);
                    }
                    Com_Printf(16, "%s too long when translated: \"%s\"\n", pszMessageType, pszInputBuffer);
                }
                for ( i = 0; i < iTokenLen - 2; ++i )
                {
                    if ( !strncmp(&szTokenBuf[i], "&&", 2u) && isdigit(szTokenBuf[i + 2]))
                    {
                        if ( bInsertEnabled )
                        {
                            ++iInsertLevel;
                        }
                        else
                        {
                            szTokenBuf[i] = 22;
                            bLocSkipped = 1;
                        }
                    }
                }
                if ( iInsertLevel <= 0 || iLen <= 0 )
                {
                    v6 = szTokenBuf;
                    v5 = &pszString[iLen];
                    do
                    {
                        v4 = *v6;
                        *v5++ = *v6++;
                    }
                    while ( v4 );
                }
                else
                {
                    for ( i = 0; i < iLen - 2; ++i )
                    {
                        if ( !strncmp(&pszString[i], "&&", 2u) && isdigit(pszString[i + 2]))
                        {
                            digit = pszString[i + 2] - 48;
                            if ( !digit )
                                Com_Printf(16, "%s cannot have &&0 as conversion format: \"%s\"\n", pszMessageType, pszInputBuffer);
                            if ( digit == insertIndex )
                            {
                                v15 = &pszString[i + 3];
                                v14 = szInsertBuf;
                                do
                                {
                                    v13 = *v15;
                                    *v14++ = *v15++;
                                }
                                while ( v13 );
                                pszString[i] = 0;
                                ++insertIndex;
                                break;
                            }
                        }
                    }
                    v12 = szTokenBuf;
                    v11 = &pszString[i];
                    do
                    {
                        v10 = *v12;
                        *v11++ = *v12++;
                    }
                    while ( v10 );
                    v9 = szInsertBuf;
                    v8 = &pszString[iTokenLen + i];
                    do
                    {
                        v7 = *v9;
                        *v8++ = *v9++;
                    }
                    while ( v7 );
                    iLen -= 3;
                    --iInsertLevel;
                }
                iLen += iTokenLen;
            }
            bInsertEnabled = 1;
            if ( *pszIn == 20 )
            {
                bLocOn = 1;
                ++pszIn;
            }
            else if ( *pszIn == 21 )
            {
                bLocOn = 0;
                ++pszIn;
            }
            if ( *pszIn == 22 )
            {
                bInsertEnabled = 0;
                ++pszIn;
            }
            pszTokenStart = pszIn;
        }
        else
        {
            ++pszIn;
        }
    }
    if ( bLocSkipped )
    {
        for ( i = 0; i < iLen; ++i )
        {
            if ( pszString[i] == 22 )
                pszString[i] = 37;
        }
    }
    return pszString;
}

int __cdecl SEH_GetLocalizedTokenReference(
                char *token,
                char *reference,
                const char *messageType,
                msgLocErrType_t errType)
{
    char *translation; // [esp+0h] [ebp-4h]

    translation = (char *)SEH_StringEd_GetString(reference);
    if ( !translation )
    {
        if ( loc_warnings && loc_warnings->current.enabled )
        {
            if ( loc_warningsAsErrors && loc_warningsAsErrors->current.enabled && errType != LOCMSG_NOERR )
                Com_Error(ERR_LOCALIZATION, "Could not translate part of %s: \"%s\"", messageType, reference);
            else
                Com_PrintWarning(16, "WARNING: Could not translate part of %s: \"%s\"\n", messageType, reference);
            translation = va("^1UNLOCALIZED(^7%s^1)^7", reference);
        }
        else
        {
            translation = va("%s", reference);
        }
        if ( errType == LOCMSG_NOERR )
            return 0;
    }
    I_strncpyz(token, translation, 1024);
    return 1;
}

bool __cdecl Japanese_ValidShiftJISCode(unsigned int _iHi, unsigned int _iLo)
{
    return (_iHi >= 0x81 && _iHi <= 0x9F || _iHi >= 0xE0 && _iHi <= 0xEF)
            && (_iLo >= 0x40 && _iLo <= 0x7E || _iLo >= 0x80 && _iLo <= 0xFC);
}

int __cdecl SEH_DecodeLetter(
                unsigned __int8 firstChar,
                unsigned __int8 secondChar,
                int *usedCount,
                int *pbIsTrailingPunctuation)
{
    bool v5; // [esp+0h] [ebp-10h]
    int CurrentLanguage; // [esp+8h] [ebp-8h]

    if ( !Language_IsAsian() )
        goto LABEL_20;
    CurrentLanguage = SEH_GetCurrentLanguage();
    if ( CurrentLanguage != 10 )
    {
        if ((CurrentLanguage == 11
#ifdef KISAK_SP
            || CurrentLanguage == 13
#endif
            ) && Japanese_ValidShiftJISCode(firstChar, secondChar))
        {
            *usedCount = 2;
            if ( pbIsTrailingPunctuation )
                *pbIsTrailingPunctuation = Japanese_IsTrailingPunctuation(secondChar + (firstChar << 8));
            return secondChar + (firstChar << 8);
        }
        goto LABEL_20;
    }
    if ( firstChar < 0xB0u || firstChar > 0xC8u || secondChar <= 0xA0u || secondChar == 255 )
    {
LABEL_20:
        *usedCount = 1;
        if ( pbIsTrailingPunctuation )
        {
            v5 = firstChar == 33
                || firstChar == 63
                || firstChar == 44
                || firstChar == 46
                || firstChar == 59
                || firstChar == 58;
            *pbIsTrailingPunctuation = v5;
        }
        return firstChar;
    }
    *usedCount = 2;
    if ( pbIsTrailingPunctuation )
        *pbIsTrailingPunctuation = 0;
    return secondChar + (firstChar << 8);
}

bool __cdecl Japanese_IsTrailingPunctuation(unsigned int uiCode)
{
    return uiCode >= 0x8140 && uiCode < 0x8152;
}

unsigned int __cdecl SEH_ReadCharFromString(const char **text, int *isTrailingPunctuation)
{
    int usedCount; // [esp+0h] [ebp-8h] BYREF
    unsigned int letter; // [esp+4h] [ebp-4h]

    letter = SEH_DecodeLetter(**text, (*text)[1], &usedCount, isTrailingPunctuation);
    *text += usedCount;
    return letter;
}

int __cdecl Language_IsAsian()
{
    return g_currentAsian;
}

int __cdecl SEH_PrintStrlen(const char *string)
{
    unsigned int c; // [esp+0h] [ebp-Ch]
    int len; // [esp+4h] [ebp-8h]
    const char *p; // [esp+8h] [ebp-4h] BYREF

    if ( !string )
        return 0;
    len = 0;
    p = string;
    while ( *p )
    {
        c = SEH_ReadCharFromString(&p, 0);
        if ( c == 94 && p && *p != 94 && *p >= 48 && *p <= 64 )
        {
            ++p;
        }
        else if ( c != 10 && c != 13 )
        {
            ++len;
        }
    }
    return len;
}

const char *__cdecl SEH_GetLanguageName(unsigned int iLanguage)
{
    if ( iLanguage < SEH_LANGUAGE_COUNT )
        return g_languages[iLanguage].pszName;
    else
        return g_languages[0].pszName;
}

const char *__cdecl SEH_GetLanguageNameAbbr(unsigned int iLanguage)
{
    if ( iLanguage < SEH_LANGUAGE_COUNT )
        return g_languages[iLanguage].pszNameAbbr;
    else
        return g_languages[0].pszNameAbbr;
}

int __cdecl SEH_GetLanguageIndexForName(const char *pszLanguageName, int *piLanguageIndex)
{
    int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < SEH_LANGUAGE_COUNT; ++i )
    {
        if ( !I_stricmp(pszLanguageName, g_languages[i].pszName) )
        {
            *piLanguageIndex = i;
            return 1;
        }
    }
    *piLanguageIndex = 0;
    return 0;
}
