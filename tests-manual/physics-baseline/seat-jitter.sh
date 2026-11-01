#!/bin/zsh
# The driver's own view of the car, frame by frame (seat-jitter.js): a walled-off client, the Land Rover made beside the
# pad, driven straight at a steady throttle for ten seconds while the chassis's shown pose, the avatar's and the
# camera's positions are sampled every 2 ms. Serverless (this client the authority) or on a port whose sandbox is up
# (not restarted here: someone may be driving in it; earlier rover and airship entities are cleared unless CLEAR=false, and the
# car is made OFFSET_X metres from the pad's centre, 0 by default).
#   tests-manual/physics-baseline/seat-jitter.sh [path/to/Darkride.app] [udp port|serverless] [results dir]
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
PORT=${2:-serverless}
R=${3:-"$D/results/seat-jitter-$PORT"}
BIN="$APP/Contents/MacOS/$(basename "$APP" .app)"   # Darkride.app, or interface.app before the rename
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R" "$R/driver"   # Test.saveObject writes into the results location only when it exists
. "$D/harness.sh"
HTTP_PORT=42180
[ "$PORT" != "serverless" ] && HTTP_PORT=$((PORT + 78))
TERRAIN_URL="http://127.0.0.1:$HTTP_PORT/rover.drkt"
[ -f "$R/rover.drkt" ] || python3 "$D/make-terrain.py" "$R/rover.drkt" 192 32 0.5
{ echo "var ROVER_CONFIG = { terrainURL: \"$TERRAIN_URL\", offsetX: ${OFFSET_X:-0}, clear: ${CLEAR:-true}, throttle: ${THROTTLE:-0.35}, seconds: ${SECONDS_DRIVEN:-10} };";
  cat "$D/landrover.js" "$D/seat-jitter.js"; } > "$R/seat-jitter.js"
if ! lsof -nP -iTCP:$HTTP_PORT -sTCP:LISTEN > /dev/null 2>&1; then
    (cd "$R" && python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 > "$R/http.log" 2>&1) &
    HTTP_PID=$!
    trap 'kill $HTTP_PID 2>/dev/null || true' EXIT
    sleep 1
fi
export HIFI_METAVERSE_URL=${HIFI_METAVERSE_URL:-https://vr.darkride.ai}
URL="hifi://localhost:$PORT"
[ "$PORT" = "serverless" ] && URL="file://$D/world.json"
echo "client: $BIN"; echo "world: $URL"; echo "results: $R"
client driver --url "$URL" --testScript "$R/seat-jitter.js" --testResultsLocation "$R/driver" || true
grep -h "SEAT-JITTER" "$R/driver.log" | sed 's/.*SEAT-JITTER //' | tail -40
