#!/bin/zsh
# PHYSICS.md round 8: the tilt-thrust aircraft over the terrain, flown through a scripted course by airship.js in its
# aircraft form (aircraft-rig.js): the pilot alone (there is no deck to walk).
#   tests-manual/physics-baseline/run-aircraft.sh [path/to/Darkride.app] [results dir] [udp port|serverless] [loss] [delay_ms]
# Serverless: one client in world.json. On a domain (the physics sandbox, restarted here from the client's build with
# the earlier runs' test entities taken out of its world, and stopped at the end): the pilot under TIVOLI_PACKET_LOSS
# and TIVOLI_PACKET_DELAY_MS when given. The summary (PHYSICS-AIRCRAFT lines) is printed; the samples land in
# <results dir>/pilot/physics-airship.json. Clients walled off, see harness.sh.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/physics-aircraft"}
PORT=${3:-42102}
LOSS=${4:-0}
DELAY=${5:-0}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R/pilot"
. "$D/harness.sh"
SIZE=192; HEIGHT=32; VOXEL=0.5
HTTP_PORT=42180
[ "$PORT" != "serverless" ] && HTTP_PORT=$((PORT + 78))
TERRAIN_URL="http://127.0.0.1:$HTTP_PORT/rover.drkt"

[ -f "$R/rover.drkt" ] || python3 "$D/make-terrain.py" "$R/rover.drkt" "$SIZE" "$HEIGHT" "$VOXEL"
for ROLE in pilot; do
    { echo "var AIRSHIP_CONFIG = { role: \"$ROLE\", vehicle: \"aircraft\", terrainURL: \"$TERRAIN_URL\" };"; cat "$D/landrover.js" "$D/airship-rig.js" "$D/aircraft-rig.js" "$D/airship.js"; } > "$R/aircraft-$ROLE.js"
done
if ! lsof -nP -iTCP:$HTTP_PORT -sTCP:LISTEN > /dev/null 2>&1; then
    (cd "$R" && python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 > "$R/http.log" 2>&1) &
    HTTP_PID=$!
    trap 'kill $HTTP_PID 2>/dev/null || true' EXIT
    sleep 1
fi

echo "client: $BIN"
echo "terrain: $TERRAIN_URL"
echo "results: $R"
if [ "$PORT" = "serverless" ]; then
    echo "serverless: the pilot alone in world.json"
    client pilot --url "file://$D/world.json" --testScript "$R/aircraft-pilot.js" --testResultsLocation "$R/pilot" || true
    grep "PHYSICS-AIRCRAFT" "$R/pilot.log" || echo "no pilot summary in $R/pilot.log" >&2
    exit 0
fi

BUILD=$(cd "$APP/../.." && pwd)
sandbox_stop
sandbox_clean_world airship- aircraft- rover-terrain
sandbox_start "$BUILD" "$PORT"

URL="hifi://localhost:$PORT"
echo "domain: $URL; the pilot under packet loss $LOSS and a delay of $DELAY ms"
TIVOLI_PACKET_LOSS="$LOSS" TIVOLI_PACKET_DELAY_MS="$DELAY" \
    client pilot --url "$URL" --testScript "$R/aircraft-pilot.js" --testResultsLocation "$R/pilot" || true
echo "== pilot"
grep "PHYSICS-AIRCRAFT" "$R/pilot.log" || echo "no pilot summary in $R/pilot.log" >&2
sandbox_copy_logs
sandbox_stop
echo "== physics server"
grep "\[physics-server\]" "$R/sandbox-assignment-client.log" 2>/dev/null | grep -i "aircraft\|vehicle\|teleport\|terrain\|late\|error\|crash\|fatal" | tail -30
