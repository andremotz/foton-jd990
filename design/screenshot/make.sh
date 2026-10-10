#!/bin/bash
# README-Bilder erzeugen: Screenshot (App im macOS-Fenster) und Icon in 256 px.
#   design/screenshot/make.sh     (App vorher bauen: cmake --build host/build)
set -euo pipefail
cd "$(dirname "$0")/../.."
APP=host/build/FotonJD990_artefacts/Release/FotonJD990.app/Contents/MacOS/FotonJD990
TMP=$(mktemp -d)
render() {   # Name, App-Argumente …
    local name=$1; shift
    perl -e 'alarm 40; exec @ARGV' "$APP" --offline --testmode --size=1640x900 --delay=400 --snapshotscale=2 "$@" --snapshot="$TMP/$name.png" >/dev/null 2>&1
    swift design/screenshot/compose.swift "$TMP/$name.png" "docs/images/$name.png"
}
mkdir -p docs/images
render screenshot  --load=host/tests/fixtures/demo.syx --set=tone.tva_env.time_1:A:38 --tab=0
render performance --perf --part=2 --perfset=part.level:3:80 --perfset=perf.reserve_1:-:8 --tab=4
sips -z 256 256 design/icon/icon-1024.png --out docs/images/icon.png >/dev/null
echo "→ docs/images/icon.png"
