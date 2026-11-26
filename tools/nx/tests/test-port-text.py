"""Check the port's UI strings and the Switch keyboard's text handling.

Every OPENBLOPS_ key the code names must exist in port_ui_translations.tsv,
SEH_GetPortString must never return null for one (callers format with it),
game text must convert to UTF-8 for the keyboard, and the keyboard's text
check must accept and refuse the right characters.
"""
from pathlib import Path
import argparse
import csv
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/port-text-tests'
out.mkdir(parents=True, exist_ok=True)

with (root / 'src/stringed/port_ui_translations.tsv').open(encoding='utf-8', newline='') as source:
    keys = {'OPENBLOPS_' + row['key'] for row in csv.DictReader(source, delimiter='\t')}

prompts = {'OPENBLOPS_ZOMBIE_REFRESH', 'OPENBLOPS_ZOMBIE_JOIN'}
shared = (root / 'src/ui/ui_shared.cpp').read_text(encoding='utf-8')
for prompt in prompts:
    assert '{"%s", ' % prompt in shared, f'{prompt} is no longer a button prompt'

missing = {}
for path in (root / 'src').rglob('*'):
    if path.suffix not in ('.cpp', '.h') or path.name == 'port_ui_translations.inc':
        continue
    for key in re.findall(r'"@?(OPENBLOPS_[A-Z0-9_]+)"', path.read_text(encoding='utf-8', errors='replace')):
        if key not in keys and key not in prompts:
            missing.setdefault(key, path.relative_to(root).as_posix())
assert not missing, f'keys missing from port_ui_translations.tsv: {missing}'


def function(path, signature):
    source = (root / path).read_text(encoding='utf-8')
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


ENCODINGS = ['cp1252', 'cp1252', 'cp1252', 'cp1252', 'cp1252', 'cp1251', 'cp1250', 'cp949', 'cp932', 'cp1250']
LANGUAGES = ['en', 'fr', 'de', 'it', 'es', 'ru', 'pl', 'ko', 'ja', 'cs']


def literal(data):
    return '"' + ''.join(chr(b) if 32 <= b < 127 and b not in (34, 92, 63) else f'\\{b:03o}' for b in data) + '"'


with (root / 'src/stringed/port_ui_translations.tsv').open(encoding='utf-8', newline='') as source:
    rows = list(csv.DictReader(source, delimiter='\t'))
samples = ',\n'.join(
    f'    {{{column}, {literal(row[language].encode("utf-8"))}, {literal(row[language].encode(ENCODINGS[column]))}}}'
    for row in rows for column, language in enumerate(LANGUAGES))
subprocess.run([sys.executable, str(root / 'tools/nx/generate-game-encodings.py'), '--check'], check=True)

stringed = (root / 'src/stringed/stringed_hooks.cpp').read_text(encoding='utf-8')
conversion = stringed[stringed.index('namespace\n{\n#include "game_encodings.inc"'):]
conversion = conversion[:conversion.index('\n}\n', conversion.index('bool SEH_Utf8ToGameText(')) + 3]

code = r'''
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#define KISAK_NX
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
int portLanguage = 0;
const char *SEH_GetPortString(const char *reference, bool utf8 = false);
int SEH_PortLanguage() { return portLanguage; }
struct PortString { const char *reference; const char *game[10]; const char *utf8[10]; };
constexpr PortString portStrings[] = {
    {"OPENBLOPS_KEYBOARD_INVALID_CHAR", {}, {"Not allowed: %s", "Interdit : %s", "Interdit : %s", "Interdit : %s", "Interdit : %s", "Interdit : %s", "Interdit : %s", "Interdit : %s", "Interdit : %s", "Interdit : %s"}},
    {"OPENBLOPS_KEYBOARD_TOO_LONG", {}, {"Too long", "Trop long", "Trop long", "Trop long", "Trop long", "Trop long", "Trop long", "Trop long", "Trop long", "Trop long"}},
};
enum SwkbdTextCheckResult { SwkbdTextCheckResult_OK, SwkbdTextCheckResult_Bad };
char decimalDelimiter = ',';
char Com_GetDecimalDelimiter() { return decimalDelimiter; }
bool I_isforfilename(int c) { return isalnum(c) || c == '_' || c == '-'; }
bool I_isalnum(int c) { return isalnum(c); }
bool I_isdigit(int c) { return isdigit(c); }
bool SEH_GameTextToUtf8(const char *text, char *out, size_t size);
bool SEH_Utf8ToGameText(const char *text, char *out, size_t size);
unsigned int SEH_ReadUtf8Char(const char **text);
int SEH_EncodeGameChar(unsigned int code, char *out);
'''
code += (root / 'src/ui/ui_keyboard.h').read_text(encoding='utf-8').split('#ifdef KISAK_NX', 1)[1].split('#endif', 1)[0]
code += function('src/stringed/stringed_hooks.cpp', 'const char *SEH_GetPortString(')
code += conversion
for signature in ['static bool UI_NX_IsAllowedChar(', 'static int UI_NX_EncodeNativeChar(', 'static bool UI_NX_ConvertText(']:
    code += function('src/ui/ui_keyboard.cpp', signature)
code += 'static const UiNxTextRequest *s_nxTextRequest;\nstatic int s_nxTextLimit;\n'
code += function('src/ui/ui_keyboard.cpp', 'static SwkbdTextCheckResult UI_NX_CheckText(')
code += 'struct Sample { int column; const char *utf8; const char *game; };\nconst Sample samples[] = {\n' + samples + '\n};\n'
code += r'''
static std::string check(UiNxTextFilter filter, const char *input, bool native = false, int limit = 64)
{
    UiNxTextRequest request;
    request.filter = filter;
    request.nativeText = native;
    s_nxTextRequest = &request;
    s_nxTextLimit = limit;
    char text[128];
    snprintf(text, sizeof(text), "%s", input);
    return UI_NX_CheckText(text, sizeof(text)) == SwkbdTextCheckResult_OK ? "" : text;
}

static std::string utf8(const char *text, bool *exact = nullptr, size_t size = 64)
{
    char out[64];
    const bool converted = SEH_GameTextToUtf8(text, out, size);
    if (exact)
        *exact = converted;
    return out;
}

static std::string game(const char *text, bool *exact = nullptr, size_t size = 64)
{
    char out[64];
    const bool converted = SEH_Utf8ToGameText(text, out, size);
    if (exact)
        *exact = converted;
    return out;
}

int main()
{
    assert(!strcmp(SEH_GetPortString("OPENBLOPS_KEYBOARD_INVALID_CHAR", true), "Not allowed: %s"));
    assert(!strcmp(SEH_GetPortString("OPENBLOPS_NOT_A_KEY"), "OPENBLOPS_NOT_A_KEY"));
    assert(!SEH_GetPortString("MENU_OK"));

    for (const Sample &sample : samples)
    {
        portLanguage = sample.column;
        char back[512], encoded[512];
        assert(SEH_Utf8ToGameText(sample.utf8, encoded, sizeof(encoded)) && !strcmp(encoded, sample.game));
        assert(SEH_GameTextToUtf8(sample.game, back, sizeof(back)) && !strcmp(back, sample.utf8));
    }

    bool exact = false;
    portLanguage = 0;
    assert(utf8("caf\xe9 \x80", &exact) == "caf\xc3\xa9 \xe2\x82\xac" && exact);
    assert(utf8("\x81", &exact) == "?" && !exact);
    assert(utf8(nullptr, &exact).empty() && exact);
    assert(utf8("abcdef", &exact, 4) == "abc" && !exact);
    assert(game("\xd0\x9f", &exact) == "?" && !exact);           // Cyrillic in a Western language
    portLanguage = 5;                                              // Russian, Windows-1251
    assert(game("\xd0\x9f\xd1\x80\xd0\xb8", &exact) == "\xcf\xf0\xe8" && exact);
    portLanguage = 7;                                              // Korean: two-byte Hangul only
    assert(game("\xed\x95\x9c", &exact) == "\xc7\xd1" && exact);   // U+D55C
    assert(game("\xeb\x98\xa0", &exact) == "?" && !exact);         // U+B620, outside the 2350
    assert(utf8("\xc7\xd1", &exact) == "\xed\x95\x9c" && exact);
    assert(game("ab\xed\x95\x9c", &exact, 4) == "ab" && !exact);  // never half a character
    portLanguage = 8;                                              // Japanese, Shift-JIS
    assert(game("\xe6\x97\xa5", &exact) == "\x93\xfa" && exact);   // U+65E5
    assert(utf8("\x83\x5c", &exact) == "\xe3\x82\xbd" && exact);   // U+30BD, second byte '\'

    portLanguage = 1;
    assert(check(UI_NX_TEXT_ANY, "Server ^1@home").empty());
    assert(check(UI_NX_TEXT_PLAIN, "Player One").empty());
    assert(check(UI_NX_TEXT_PLAIN, "a@b") == "Interdit : @");
    assert(check(UI_NX_TEXT_PLAIN, "^1red") == "Interdit : ^");
    assert(check(UI_NX_TEXT_SAFE, "a;b") == "Interdit : ;" && check(UI_NX_TEXT_SAFE, "a%b") == "Interdit : %");
    assert(check(UI_NX_TEXT_ANY, "caf\xc3\xa9") == "Interdit : \xc3\xa9");
    assert(check(UI_NX_TEXT_PLAIN, "caf\xc3\xa9", true).empty());
    assert(check(UI_NX_TEXT_PLAIN, "\xd0\x9f", true) == "Interdit : \xd0\x9f");   // not French
    assert(check(UI_NX_TEXT_DIGITS, "\xc3\xa9", true) == "Interdit : \xc3\xa9");  // not a letter field
    assert(check(UI_NX_TEXT_ANY, "\xe2\x82\xac") == "Interdit : \xe2\x82\xac");
    assert(check(UI_NX_TEXT_DIGITS, "123").empty() && check(UI_NX_TEXT_DIGITS, "1a") == "Interdit : a");
    assert(check(UI_NX_TEXT_DECIMAL, "1.5").empty() && check(UI_NX_TEXT_DECIMAL, "1,5").empty());
    assert(check(UI_NX_TEXT_ALNUM, "ab 1") == "Interdit :  ");
    assert(check(UI_NX_TEXT_FILENAME, "my_save-1").empty() && check(UI_NX_TEXT_FILENAME, "a/b") == "Interdit : /");
    assert(check(UI_NX_TEXT_PLAIN, "abc", false, 3).empty() && check(UI_NX_TEXT_PLAIN, "abcd", false, 3) == "Trop long");
    portLanguage = 7;
    assert(check(UI_NX_TEXT_PLAIN, "\xed\x95\x9c", true, 2).empty());                       // two bytes
    assert(check(UI_NX_TEXT_PLAIN, "\xed\x95\x9c\xed\x95\x9c", true, 3) == "Trop long");
    assert(check(UI_NX_TEXT_PLAIN, "\xeb\x98\xa0", true) == "Interdit : \xeb\x98\xa0");
    portLanguage = 8;
    assert(check(UI_NX_TEXT_PLAIN, "\xe6\x97\xa5", true).empty());
    assert(check(UI_NX_TEXT_PLAIN, "\xe3\x82\xbd", true).empty());                          // 0x83 0x5C
    assert(check(UI_NX_TEXT_SAFE, "\xe3\x82\xbd", true) == "Interdit : \xe3\x82\xbd");
    puts("port text tests passed");
}
'''
source = out / 'port-text.cpp'
binary = out / 'port-text.exe'
source.write_text(code, encoding='utf-8')
subprocess.run([args.compiler, '-std=c++20', '-Wall', '-Werror', '-O1', '-I', str(root / 'src/stringed'),
                str(source), '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
