#include "win_localize.h"
#include <universal/com_fileaccess.h>
#include <universal/assertive.h>
#include <stringed/stringed_hooks.h>
#include <universal/q_parse.h>
#include <universal/q_shared.h>
#include <cstdio>
#include <cstring>
#ifdef KISAK_NX
#include <database/db_kbz.h>
#else
#include "win_common.h"
#endif

struct// $8CB265A9D3778DFC1F2AA7A5F0192391 // sizeof=0x8
{                                       // XREF: .data:localization/r
    char *language;                     // XREF: Win_InitLocalization(void)+6/w
    char *strings;                      // XREF: Win_InitLocalization(void)+10/w
} localization;

char language_buffer[4096];

int Win_InitLocalization()
{
    signed int size; // [esp+0h] [ebp-10h]
    int sizea; // [esp+0h] [ebp-10h]
    FILE *fp; // [esp+4h] [ebp-Ch]
    int i; // [esp+8h] [ebp-8h]
    int lang; // [esp+Ch] [ebp-4h] BYREF

    localization.language = 0;
    localization.strings = 0;
    fp = FS_FileOpenReadText("localization.txt");

    if (!fp)
    {
        iassert(0); // LWSS ADD: you probably need to change the working dir!
        return 0;
    }

    size = FS_FileGetFileSize(fp);
    if (size >= 4096
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\win32\\win_localize.cpp",
            39,
            0,
            "%s",
            "size < LANGUAGE_BUF_SIZE"))
    {
        __debugbreak();
    }
    localization.language = language_buffer;
    sizea = FS_FileRead(language_buffer, size, fp);
    FS_FileClose(fp);
#ifdef KISAK_NX
    // Opened with "rt": on Windows the CRT strips CR in text mode. newlib
    // has no text mode, so strip them here -- otherwise the language parses
    // as "english\r" and every zone\<lang>\*.ff path misses.
    {
        int rd = 0, wr = 0;
        for (; rd < sizea; ++rd)
            if (language_buffer[rd] != 0x0D)
                language_buffer[wr++] = language_buffer[rd];
        sizea = wr;
    }
#endif
    if (sizea)
    {
        localization.language[sizea] = 0;
        lang = 0;
        for (i = 0; localization.language[i]; ++i)
        {
            if (localization.language[i] == 10)
            {
                localization.language[i] = 0;
                localization.strings = &localization.language[i + 1];
                SEH_GetLanguageIndexForName(localization.language, &lang);
                return lang;
            }
        }
        return lang;
    }
    else
    {
        localization.language = 0;
        return 0;
    }
}

// The game's language is the one localization.txt names (its first line),
// which every localized path is built from: zone\<language>\<abbr>*.ff, and on
// the Switch kbz/<abbr>*.kbz. An install has one or a few languages, and with
// the named one missing the localized zones are not found and the game does
// not load. So when the named language is not installed, the first one that
// is is used instead: English, then the language table's order. A language
// counts as installed when its localized code_post_gfx zone is there.
// loc_language is set to match (SEH_InitLanguage, Com_ApplyInstalledLanguage).
static bool Win_FileExists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

static bool Win_LanguageInstalled(unsigned int language)
{
    // Austrian shares German's ge_ zones, and DB_BuildOSPath its zone\german\.
    const char *name = SEH_GetLanguageName(language == 4 ? 3 : language);
    const char *abbr = SEH_GetLanguageNameAbbr(language);
#ifdef KISAK_SP
    const char *zone = "code_post_gfx";
#else
    const char *zone = "code_post_gfx_mp";
#endif
    char path[512];
#ifdef KISAK_NX
    const char *dir = Sys_KbzInstallDir();
    snprintf(path, sizeof(path), "%s/kbz/%s%s.kbz", dir, abbr, zone);
    if (Win_FileExists(path))
        return true;
    snprintf(path, sizeof(path), "%s/zone/%s/%s%s.ff", dir, name, abbr, zone);
#else
    snprintf(path, sizeof(path), "%s\\zone\\%s\\%s%s.ff", Sys_DefaultInstallPath(), name, abbr, zone);
#endif
    return Win_FileExists(path);
}

void Win_PickInstalledLanguage()
{
    static char s_installedLanguage[64];
    int configured = -1;
    if (localization.language && !SEH_GetLanguageIndexForName(localization.language, &configured))
        configured = -1;
    if (configured >= 0 && Win_LanguageInstalled(configured))
        return;
    for (unsigned int i = 0;; ++i)
    {
        const char *name = SEH_GetLanguageName(i);
        if (i > 0 && name == SEH_GetLanguageName(0))
            break;   // past the end: the table's first entry again
        if ((int)i == configured || i == 4 || !Win_LanguageInstalled(i))
            continue;
        I_strncpyz(s_installedLanguage, name, sizeof(s_installedLanguage));
        printf("localization: '%s' is not installed; using '%s'\n",
               localization.language ? localization.language : "(none)", s_installedLanguage);
        localization.language = s_installedLanguage;
        return;
    }
}

void __cdecl Win_ShutdownLocalization()
{
    localization.language = 0;
    localization.strings = 0;
}

char *__cdecl Win_LocalizeRef(const char *ref)
{
    const char *v1; // eax
    const char *v3; // eax
    const char *strings; // [esp+14h] [ebp-Ch] BYREF
    int useRef; // [esp+18h] [ebp-8h]
    const char *token; // [esp+1Ch] [ebp-4h]

    Com_BeginParseSession("localization");
    strings = localization.strings;
    do
    {
        token = (const char *)Com_Parse(&strings);
        if ( !*token )
        {
            Com_EndParseSession();
            v1 = va("unlocalized: %s", ref);
            if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\win32\\win_localize.cpp", 112, 0, v1) )
                __debugbreak();
            return Win_CopyLocalizationString(ref);
        }
        useRef = strcmp(token, ref) == 0;
        token = (const char *)Com_Parse(&strings);
        if ( !*token )
        {
            Com_EndParseSession();
            v3 = va("missing value: %s", ref);
            if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\win32\\win_localize.cpp", 121, 0, v3) )
                __debugbreak();
            return Win_CopyLocalizationString(ref);
        }
    }
    while ( !useRef );
    Com_EndParseSession();
    return Win_CopyLocalizationString(token);
}

char *__cdecl Win_CopyLocalizationString(const char *string)
{
    return va("%s", string);
}

char *__cdecl Win_GetLanguage()
{
    if ( !localization.language
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\win32\\win_localize.cpp",
                    140,
                    0,
                    "%s",
                    "localization.language") )
    {
        __debugbreak();
    }
    return localization.language;
}

