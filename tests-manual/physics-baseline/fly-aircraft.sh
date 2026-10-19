#!/bin/zsh
# Fly the tilt-thrust aircraft yourself (PHYSICS.md round 8): a client on the physics sandbox (the server flies it, you give
# the orders) with the terrain and the craft made for you. Climb aboard and click it to take the stick: W/S set the
# throttle (X cuts it), A/D roll and yaw, up/down the elevator, Space tilts the nacelles up toward hover while held, C
# tilts them down toward forward flight, F leaves the stick. R for a fresh craft on the pad, T back to the pad.
#   tests-manual/physics-baseline/fly-aircraft.sh [path/to/Darkride.app] [udp port|serverless]
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
PORT=${2:-42102}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
R=${TMPDIR:-/tmp}/aircraft-playground
mkdir -p "$R"
HTTP_PORT=42180
[ "$PORT" != "serverless" ] && HTTP_PORT=$((PORT + 78))
TERRAIN_URL="http://127.0.0.1:$HTTP_PORT/rover.drkt"
[ -f "$R/rover.drkt" ] || python3 "$D/make-terrain.py" "$R/rover.drkt" 192 32 0.5
{ echo "var AIRSHIP_CONFIG = { vehicle: \"aircraft\", terrainURL: \"$TERRAIN_URL\" };"; cat "$D/landrover.js" "$D/airship-rig.js" "$D/aircraft-rig.js" "$D/airship-playground.js"; } > "$R/aircraft-playground.js"
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
echo "aboard, click the craft for the stick: W/S throttle (X cuts), A/D roll and yaw, up/down elevator, Space tilts to hover, C to forward flight, F leave; R fresh craft, T to the pad"
"$BIN" --allowMultipleInstances --url "$URL" --testScript "$R/aircraft-playground.js" > "$R/client.log" 2>&1 || true
