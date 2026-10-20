#!/bin/zsh
# PHYSICS.md round 6: the Land Rover on the terrain, driven over a scripted course (rover.js, landrover.js).
#   tests-manual/physics-baseline/run-rover.sh [path/to/Darkride.app] [results dir] [udp port|serverless] [loss] [delay_ms]
# make-terrain.py writes the terrain file into the results dir, which is served over HTTP on <port>+78 (the physics server
# and the clients fetch it from there); rover.js and landrover.js are copied beside it with the two one-line wrapper
# scripts that name the role and the terrain URL. Serverless: one client, the driver, in world.json (the fallback path).
# On a domain (the physics sandbox, restarted here from the client's build with the earlier runs' test entities taken
# out of its world, the physics server among its services, and stopped at the end): the passenger first, then the
# driver after 12 s, the driver under TIVOLI_PACKET_LOSS=<loss> and TIVOLI_PACKET_DELAY_MS=<delay> when given (0 and 0 by
# default). The summaries (PHYSICS-ROVER lines) are printed; the samples land in <results dir>/driver/physics-rover.json
# and <results dir>/passenger/physics-rover.json, and the sandbox's assignment-client log of the run (the physics
# server's lines are in it under [physics-server]) is copied beside them. Clients walled off, see harness.sh.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/physics-rover"}
PORT=${3:-42102}
LOSS=${4:-0}
DELAY=${5:-0}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R/driver" "$R/passenger"
. "$D/harness.sh"
SIZE=192; HEIGHT=32; VOXEL=0.5
HTTP_PORT=42180
[ "$PORT" != "serverless" ] && HTTP_PORT=$((PORT + 78))
TERRAIN_URL="http://127.0.0.1:$HTTP_PORT/rover.drkt"

[ -f "$R/rover.drkt" ] || python3 "$D/make-terrain.py" "$R/rover.drkt" "$SIZE" "$HEIGHT" "$VOXEL"
# one self-contained script per role: the config, the Land Rover rig, the test
for ROLE in driver passenger; do
    { echo "var ROVER_CONFIG = { role: \"$ROLE\", terrainURL: \"$TERRAIN_URL\" };"; cat "$D/landrover.js" "$D/rover.js"; } > "$R/rover-$ROLE.js"
done
if ! lsof -nP -iTCP:$HTTP_PORT -sTCP:LISTEN > /dev/null 2>&1; then
    (cd "$R" && python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 > "$R/http.log" 2>&1) &
    HTTP_PID=$!
    trap 'kill $HTTP_PID 2>/dev/null || true' EXIT
    sleep 1
fi

echo "client: $BIN"
echo "terrain: $TERRAIN_URL ($SIZE m, $HEIGHT m tall, $VOXEL m voxels)"
echo "results: $R"
if [ "$PORT" = "serverless" ]; then
    echo "serverless: the driver alone in world.json"
    client driver --url "file://$D/world.json" --testScript "$R/rover-driver.js" --testResultsLocation "$R/driver" || true
    grep "PHYSICS-ROVER" "$R/driver.log" || echo "no driver summary in $R/driver.log" >&2
    exit 0
fi

BUILD=$(cd "$APP/../.." && pwd)
sandbox_stop
sandbox_clean_world rover- airship- aircraft-
sandbox_start "$BUILD" "$PORT"

URL="hifi://localhost:$PORT"
echo "domain: $URL; the driver under packet loss $LOSS and a delay of $DELAY ms"
client passenger --url "$URL" --testScript "$R/rover-passenger.js" --testResultsLocation "$R/passenger" &
PASSENGER=$!
wait_for_client passenger
TIVOLI_PACKET_LOSS="$LOSS" TIVOLI_PACKET_DELAY_MS="$DELAY" \
    client driver --url "$URL" --testScript "$R/rover-driver.js" --testResultsLocation "$R/driver" || true
wait $PASSENGER || true
echo "== driver"
grep "PHYSICS-ROVER" "$R/driver.log" || echo "no driver summary in $R/driver.log" >&2
echo "== passenger"
grep "PHYSICS-ROVER" "$R/passenger.log" || echo "no passenger summary in $R/passenger.log" >&2
sandbox_copy_logs
sandbox_stop
echo "== physics server"
grep "\[physics-server\]" "$R/sandbox-assignment-client.log" 2>/dev/null | grep -i "vehicle\|terrain\|rover\|late\|error\|crash\|fatal" | tail -30
