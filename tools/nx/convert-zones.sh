#!/bin/sh
# Convert the zones the port uses into <game>/kbz/ and validate each with
# kbzdump: the multiplayer set (all 26 maps), then the SP / Zombies set (code_*, common,
# patch, frontend, terminal, common_zombie, zombie_theater, zombie_pentagon). SP zones have no _mp
# suffix, so both share kbz/. Localized zones are converted for every
# language folder the install has (zone/English, zone/Spanish, ...). Run from
# the msys2 UCRT64 shell after
# tools/ffconv/build.sh:
#   GAME=/f/pluto_t5_full_game sh tools/nx/convert-zones.sh
# ALL=1 converts every zone of the install instead, campaign missions
# included, as the web installer does:
#   ALL=1 GAME=/c/Games/BlackOps sh tools/nx/convert-zones.sh
root=$(cd "$(dirname "$0")/../.." && pwd)
GAME=${GAME:?set GAME to the Black Ops install}
TOOLS=$root/tools/ffconv
OUT="$GAME/kbz"
mkdir -p "$OUT"
LOG=$OUT/convert.log
: > "$LOG"
convert_zone() {
    z=$1
    name=$(basename "$z")
    kbz="$OUT/$name.kbz"
    echo "===== $name" >> "$LOG"
    if "$TOOLS/convert.exe" "$GAME/zone/$z.ff" "$kbz" >> "$LOG" 2>&1; then
        if "$TOOLS/kbzdump.exe" "$kbz" >> "$LOG" 2>&1; then
            printf '%-22s OK        %s bytes\n' "$name" "$(stat -c %s "$kbz")"
        else
            printf '%-22s CONVERTED, kbzdump FAILED\n' "$name"
        fi
    else
        printf '%-22s CONVERT FAILED (exit %s)\n' "$name" "$?"
        rm -f "$kbz"
    fi
}

if [ -n "$ALL" ]; then
    zones=$(cd "$GAME/zone" && find . -iname "*.ff" | sed -e "s|^\./||" -e "s|\.[fF][fF]$||" | sort)
else
    zones="\
    Common/code_pre_gfx_mp   English/en_code_pre_gfx_mp \
    Common/code_post_gfx_mp  English/en_code_post_gfx_mp \
    Common/common_mp         English/en_common_mp \
    Common/ui_mp             English/en_ui_mp \
    Common/patch_mp          English/en_patch_mp \
    Common/patch_ui_mp       Common/ui_viewer_mp \
    Common/mp_area51         English/en_mp_area51 \
    Common/mp_array          English/en_mp_array \
    Common/mp_berlinwall2    English/en_mp_berlinwall2 \
    Common/mp_cairo          English/en_mp_cairo \
    Common/mp_cosmodrome     English/en_mp_cosmodrome \
    Common/mp_cracked        English/en_mp_cracked \
    Common/mp_crisis         English/en_mp_crisis \
    Common/mp_discovery      English/en_mp_discovery \
    Common/mp_drivein        English/en_mp_drivein \
    Common/mp_duga           English/en_mp_duga \
    Common/mp_firingrange    English/en_mp_firingrange \
    Common/mp_golfcourse     English/en_mp_golfcourse \
    Common/mp_gridlock       English/en_mp_gridlock \
    Common/mp_hanoi          English/en_mp_hanoi \
    Common/mp_havoc          English/en_mp_havoc \
    Common/mp_hotel          English/en_mp_hotel \
    Common/mp_kowloon        English/en_mp_kowloon \
    Common/mp_mountain       English/en_mp_mountain \
    Common/mp_nuked          English/en_mp_nuked \
    Common/mp_outskirts      English/en_mp_outskirts \
    Common/mp_radiation      English/en_mp_radiation \
    Common/mp_russianbase    English/en_mp_russianbase \
    Common/mp_silo           English/en_mp_silo \
    Common/mp_stadium        English/en_mp_stadium \
    Common/mp_villa          English/en_mp_villa \
    Common/mp_zoo            English/en_mp_zoo \
    Common/code_pre_gfx      English/en_code_pre_gfx \
    Common/code_post_gfx     English/en_code_post_gfx \
    Common/common            English/en_common \
    Common/patch             English/en_patch \
    Common/frontend          Common/frontend_patch      English/en_frontend \
    Common/terminal          English/en_terminal \
    Common/common_zombie     Common/common_zombie_patch English/en_common_zombie \
    Common/zombie_theater    Common/zombie_theater_patch English/en_zombie_theater \
    Common/zombie_pentagon   Common/zombie_pentagon_patch English/en_zombie_pentagon"
fi

# The default set above names the English localized zones. Each of them is
# converted for every language the install has instead: zone/<Language>/
# holds that language's zones, prefixed with its abbreviation (the game's
# language table, src/stringed/stringed_hooks.cpp; Austrian uses German's).
lang_prefix() {
    case $(echo "$1" | tr 'A-Z' 'a-z') in
        english) echo en_ ;;   french) echo fr_ ;;   frenchcan) echo fc_ ;;
        german|austrian) echo ge_ ;;   italian) echo it_ ;;   spanish) echo sp_ ;;
        british) echo br_ ;;   russian) echo ru_ ;;   polish) echo po_ ;;
        korean) echo ko_ ;;   japanese) echo ja_ ;;   czech) echo cz_ ;;
        fulljap) echo fj_ ;;
    esac
}
if [ -z "$ALL" ]; then
    localized=
    for z in $zones; do
        case $z in
            English/en_*)
                base=${z#English/en_}
                for dir in "$GAME"/zone/*/; do
                    lang=$(basename "$dir")
                    prefix=$(lang_prefix "$lang")
                    [ -n "$prefix" ] && [ -f "$dir$prefix$base.ff" ] && localized="$localized $lang/$prefix$base"
                done ;;
            *) localized="$localized $z" ;;
        esac
    done
    zones=$localized
fi

for z in $zones; do
    convert_zone "$z"
done
