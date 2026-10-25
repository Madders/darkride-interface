#!/bin/zsh
# The flight assistant's scripted course (assist-pilot.js) in a walled-off client, on the physics sandbox (the server
# flies the craft) or serverless (the client does):
#   tests-manual/physics-baseline/run-assist.sh <path/to/Darkride.app> <results dir> [udp port|serverless] [scripts dir]
# The scripts dir (default this checkout's scripts/) supplies libraries/flightAssist.js, so a branch's assistant can be
# flown with another build's client. Prints the ASSIST lines; the samples are in <results dir>/assist-pilot.json.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:?usage: run-assist.sh <app> <results dir> [udp port|serverless] [scripts dir]}
R=${2:?results dir}
PORT=${3:-serverless}
SCRIPTS=${4:-"$D/../../scripts"}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R"
HTTP_PORT=42180
[ "$PORT" != "serverless" ] && HTTP_PORT=$((PORT + 78))
TERRAIN_URL="http://127.0.0.1:$HTTP_PORT/rover.drkt"
[ -f "$R/rover.drkt" ] || python3 "$D/make-terrain.py" "$R/rover.drkt" 192 32 0.5
{ echo "var ASSIST_CONFIG = { terrainURL: \"$TERRAIN_URL\" };"; cat "$D/landrover.js" "$D/aircraft-rig.js" "$SCRIPTS/system/libraries/flightAssist.js" "$D/assist-pilot.js"; } > "$R/assist-pilot.js"
if ! lsof -nP -iTCP:$HTTP_PORT -sTCP:LISTEN > /dev/null 2>&1; then
    (cd "$R" && python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 > "$R/http.log" 2>&1) &
    HTTP_PID=$!
    trap 'kill $HTTP_PID 2>/dev/null || true' EXIT
    sleep 1
fi
URL="hifi://localhost:$PORT"
[ "$PORT" = "serverless" ] && URL="file://$D/world.json"
# harness.sh's client: walled off in <results>/assist-home (its own settings and data, no sign-in), the machine lock,
# the small corner window (CLIENT_WINDOW="@Rect(x y w h)" moves it) and the "TEST, not yours" title; its texture cache
# (up to 1.5 GB) is deleted when it exits
. "$D/harness.sh"
client assist --url "$URL" --testScript "$R/assist-pilot.js" quitWhenFinished --testResultsLocation "$R" || true
grep "ASSIST" "$R/assist.log" | sed 's/.*ASSIST/ASSIST/' || { echo "no ASSIST lines in $R/assist.log" >&2; exit 1; }
