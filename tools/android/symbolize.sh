#!/bin/bash
# tools/android/symbolize.sh <crash-*.txt> - traduce gli indirizzi "libmain.so+0x..." di un crash di T5Jool
# in funzione e riga, con la libmain.so non strippata salvata da build.sh (out/libmain-symbols.so).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
DEPS=${T5_DEPS:-$HOME/t5jool-android}
SDK=${ANDROID_HOME:-$DEPS/sdk}
A2L=$(ls -d "$SDK"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-addr2line 2>/dev/null | tail -1)
LIB=${LIBMAIN:-$ROOT/out/libmain-symbols.so}
[ -f "$1" ] || { echo "uso: $0 crash.txt"; exit 1; }
# simboli della build giusta: il BuildId di libmain.so sta nel logcat del crash (stessa cartella dello zip dei log)
if [ -z "${LIBMAIN:-}" ]; then
    # prima il BuildId scritto dal gioco nel crash stesso (fix 289), poi quello del logcat
    BID=$(grep -o 'libmain.so BuildId: [0-9a-f]*' "$1" | head -1 | grep -o '[0-9a-f]*$' || true)
    [ -z "$BID" ] && BID=$(cat "$(dirname "$1")"/logcat-crash-*.txt 2>/dev/null | grep -o 'libmain.so (BuildId: [0-9a-f]*' | tail -1 | grep -o '[0-9a-f]*$' || true)
    if [ -n "$BID" ] && [ -f "$ROOT/out/symbols/libmain-$BID.so" ]; then
        LIB="$ROOT/out/symbols/libmain-$BID.so"
        echo "simboli della build $BID"
    elif [ -n "$BID" ]; then
        echo "ATTENZIONE: simboli della build $BID non trovati in out/symbols: i nomi qui sotto possono essere sbagliati"
    fi
fi
grep -oE 'libmain\.so\+0x[0-9a-f]+' "$1" | while read -r f; do
    off=${f#libmain.so+}
    echo "$f  $("$A2L" -f -C -i -e "$LIB" "$off" | paste -sd' ' -)"
done
