#!/bin/zsh
# Feet put under a Terrain must come back out (bury.js), serverless, on the rover terrain:
#   tests-manual/physics-baseline/run-bury.sh [path/to/Darkride.app] [results dir]
# make-terrain.py writes the terrain into the results dir, served over HTTP on 127.0.0.1:42181; one walled-off client
# (harness.sh's client: its own settings and app data in <results>/bury-home, no sign-in, the machine lock) runs
# landrover.js and bury.js in world.json. Prints the PHYSICS-BURY lines and how often the lift fired during the walk
# (it must be 0).
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/physics-bury"}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
HTTP_PORT=42181
mkdir -p "$R"
[ -f "$R/rover.drkt" ] || python3 "$D/make-terrain.py" "$R/rover.drkt" 192 32 0.5
{ echo "var BURY_CONFIG = { terrainURL: \"http://127.0.0.1:$HTTP_PORT/rover.drkt\" };"; cat "$D/landrover.js" "$D/bury.js"; } > "$R/bury-test.js"
(cd "$R" && python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 > "$R/http.log" 2>&1) &
HTTP_PID=$!
trap 'kill $HTTP_PID 2>/dev/null || true' EXIT
sleep 1
echo "client: $BIN"
echo "results: $R"
. "$D/harness.sh"
client bury --url "file://$D/world.json" --testScript "$R/bury-test.js" quitWhenFinished --testResultsLocation "$R" || true
grep -a "PHYSICS-BURY" "$R/bury.log" | sed 's/.*PHYSICS-BURY/PHYSICS-BURY/' || { echo "no summary in $R/bury.log" >&2; exit 1; }
echo "lifts during the walk: $(sed -n '/PHYSICS-BURY walk start/,/PHYSICS-BURY walk end/p' "$R/bury.log" | grep -a -c 'lifted onto it')"
