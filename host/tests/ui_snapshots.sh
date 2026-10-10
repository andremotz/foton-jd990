#!/bin/bash
# UI-Referenzbilder: rendert feste Szenarien per --snapshot und vergleicht mit
# host/tests/golden/. Ohne Hardware, ohne Bildschirmaufnahme-Rechte, nur macOS.
#
#   host/tests/ui_snapshots.sh            vergleichen (Exit ≠ 0 bei Abweichung)
#   host/tests/ui_snapshots.sh --update   Referenzbilder neu schreiben (nach gewollter UI-Änderung)
#   UI_TOLERANCE=<n>                      erlaubte abweichende Pixel (Default 0; anderer Rechner/
#                                         andere macOS-Version rendern Schrift evtl. minimal anders)
#
# Szenarien nutzen eigene Test-Patches (host/tests/fixtures, erzeugt mit `jd990 init`)
# und eine Wegwerf-Bibliothek — nie Andrés echte Bibliothek.
set -u
cd "$(dirname "$0")/../.."
APP="host/build/FotonJD990_artefacts/Release/FotonJD990.app/Contents/MacOS/FotonJD990"
OUT="host/build/ui-snapshots"; GOLD="host/tests/golden"; FIX="host/tests/fixtures"
mkdir -p "$OUT" "$GOLD"
[ -x "$APP" ] || { echo "App fehlt: cmake --build host/build"; exit 2; }
IMGDIFF="host/build/imgdiff"
[ "$IMGDIFF" -nt host/tests/imgdiff.swift ] || swiftc -O host/tests/imgdiff.swift -o "$IMGDIFF" 2>/dev/null || exit 2

scenario() {   # name, dann App-Argumente
    local name=$1; shift
    rm -f "$OUT/$name.png" "$OUT/$name-lib.sqlite" "$OUT/vergleich2-lib.sqlite"
    perl -e 'alarm 40; exec @ARGV' "$APP" --offline --testmode --size=${SIZE:-1320x900} --delay=400 "$@" --snapshot="$OUT/$name.png" >/dev/null 2>&1
    [ -f "$OUT/$name.png" ] || { echo "FEHL  $name: kein Snapshot"; return 1; }
    if [ "${UPDATE:-0}" = 1 ]; then cp "$OUT/$name.png" "$GOLD/$name.png"; echo "neu   $name"; return 0; fi
    [ -f "$GOLD/$name.png" ] || { echo "FEHL  $name: keine Referenz (--update)"; return 1; }
    if r=$("$IMGDIFF" "$GOLD/$name.png" "$OUT/$name.png" "$OUT/$name-diff.png" "${UI_TOLERANCE:-0}"); then echo "ok    $name ($r)"; else echo "FEHL  $name ($r) → $OUT/$name-diff.png"; return 1; fi
}

[ "${1:-}" = "--update" ] && UPDATE=1
fail=0
scenario leer          --tab=0                                                             || fail=1
scenario panel         --init --set=tone.tvf.cutoff:A:40 --set=patch.level:-:90 --tab=0     || fail=1
scenario vergleich     --load=$FIX/bass.syx --link --set=tone.tvf.cutoff:A:60 --compare=TVF || fail=1
scenario effekte       --load=$FIX/bass.syx --set=patch.structure_ab:-:2 --movefx=A:0:2 --tab=1 || fail=1
scenario huellkurve    --load=$FIX/bass.syx --set=tone.pitch_env.level_1:A:75 --set=tone.pitch_env.time_1:A:20 \
                       --set=tone.tva_env.time_1:A:30 --tab=0                               || fail=1
scenario werte         --load=$FIX/bass.syx --envvalues --tab=0                            || fail=1
# Fenster muss auf jeden Bildschirm passen (macOS begrenzt sonst die Höhe → falsche Abweichung)
SIZE=1320x900 \
scenario zoom130       --scale=130 --load=$FIX/bass.syx --set=tone.lfo1.depth_pitch:A:58 \
                       --set=tone.lfo1.fade:A:75 --tab=0                                    || fail=1
scenario alle          --load=$FIX/bass.syx --tone=B --tab=2                                || fail=1
scenario bibliothek    --library=$OUT/bibliothek-lib.sqlite --libimport=$FIX/init.syx --libimport=$FIX/bass.syx \
                       --load=$FIX/init.syx --libsearch=bass --libselect=0 --tab=3          || fail=1
scenario vergleich2    --library=$OUT/vergleich2-lib.sqlite --libimport=$FIX/init.syx --libimport=$FIX/bass.syx \
                       --libselect=0,1 --tab=3                                              || fail=1
SIZE=1320x1000 \
scenario effektskizzen --scale=60 --load=$FIX/bass.syx --set=patch.fx.dl_feedback:-:80 --fxselect=A:1,B:1 --tab=1 || fail=1
scenario performance   --perf --part=2 --perfset=part.level:3:70 --perfset=perf.reserve_1:-:2 --tab=4 || fail=1
scenario perfpanel     --perf --part=2 --tab=1                                              || fail=1
[ $fail = 0 ] && echo "UI-Referenzbilder: alle gleich" || echo "UI-Referenzbilder: Abweichungen (bei gewollter Änderung: --update)"
exit $fail
