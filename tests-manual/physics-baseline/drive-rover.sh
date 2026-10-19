#!/bin/zsh
# Drive the Land Rover yourself (PHYSICS.md round 6): a client on the physics sandbox (the server drives the car, you
# drive the server) with the terrain and the car made for you. Click the car to get in, W/S A/D Space, F to get out,
# R for a fresh car on the pad, T back to the pad.
#   tests-manual/physics-baseline/drive-rover.sh [path/to/Darkride.app] [udp port|serverless]
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
PORT=${2:-42102}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
R=${TMPDIR:-/tmp}/rover-playground
mkdir -p "$R"
HTTP_PORT=42180
[ "$PORT" != "serverless" ] && HTTP_PORT=$((PORT + 78))
TERRAIN_URL="http://127.0.0.1:$HTTP_PORT/rover.drkt"
[ -f "$R/rover.drkt" ] || python3 "$D/make-terrain.py" "$R/rover.drkt" 192 32 0.5
{ echo "var ROVER_CONFIG = { terrainURL: \"$TERRAIN_URL\" };"; cat "$D/landrover.js" "$D/rover-playground.js"; } > "$R/rover-playground.js"
if ! lsof -nP -iTCP:$HTTP_PORT -sTCP:LISTEN > /dev/null 2>&1; then
    (cd "$R" && python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 > "$R/http.log" 2>&1) &
    HTTP_PID=$!
    trap 'kill $HTTP_PID 2>/dev/null || true' EXIT
    sleep 1
fi
export HIFI_METAVERSE_URL=${HIFI_METAVERSE_URL:-https://vr.darkride.ai}
URL="hifi://localhost:$PORT"
[ "$PORT" = "serverless" ] && URL="file://$D/world.json"
echo "client: $BIN"
echo "world: $URL"
echo "terrain: $TERRAIN_URL"
echo "in the car: W/S throttle and brake, A/D steer, Space hand brake, F out; R fresh car, T back to the pad"
"$BIN" --allowMultipleInstances --url "$URL" --testScript "$R/rover-playground.js" > "$R/client.log" 2>&1 || true
