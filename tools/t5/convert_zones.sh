#!/bin/bash
# tools/t5/convert_zones.sh <Black Ops folder> - converts every zone of the install (zone/**/*.ff) to
# the 64-bit layout the engine loads (<folder>/kbz/*.kbz), once. Takes a few minutes.
# Mods (mods/<name>/*.ff) are converted next to their .ff (mods/<name>/mod.kbz): every mod has its own
# "mod" zone. Start a mod with +set fs_game mods/<name>.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
GAME=${1:?usage: convert_zones.sh <Black Ops folder>}
CONVERT=$ROOT/build-tools/convert
[ -x "$CONVERT" ] || { echo "Build first: tools/t5/build_linux.sh"; exit 1; }
[ -d "$GAME/zone" ] || { echo "No zone/ folder in $GAME"; exit 1; }
OUT="$GAME/kbz"
mkdir -p "$OUT"
LOG="$OUT/convert.log"
: > "$LOG"
ok=0; bad=0
while IFS= read -r ff; do
    rel=${ff#"$GAME/zone/"}
    name=$(basename "${rel%.*}")
    if "$CONVERT" "$ff" "$OUT/$name.kbz" >> "$LOG" 2>&1; then
        ok=$((ok + 1)); printf '.'
    else
        bad=$((bad + 1)); rm -f "$OUT/$name.kbz"; printf '\n%s FAILED\n' "$rel"
    fi
done < <(find "$GAME/zone" -iname "*.ff" | sort)
if [ -d "$GAME/mods" ]; then
    while IFS= read -r ff; do
        mod=$(basename "$(dirname "$ff")")
        case "$mod" in _*|t5save) continue;; esac
        out="${ff%.*}.kbz"
        [ -f "$out" ] && [ "$out" -nt "$ff" ] && continue
        if "$CONVERT" "$ff" "$out" >> "$LOG" 2>&1; then
            ok=$((ok + 1)); printf '.'
        else
            bad=$((bad + 1)); rm -f "$out"; printf '\nmods/%s/%s FAILED\n' "$mod" "$(basename "$ff")"
        fi
    done < <(find "$GAME/mods" -mindepth 2 -maxdepth 2 -iname "*.ff" | sort)
fi
printf '\nConverted %d zones (%d failed). Log: %s\n' "$ok" "$bad" "$LOG"
