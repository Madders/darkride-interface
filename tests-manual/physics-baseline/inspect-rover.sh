#!/bin/zsh
# Pictures of the native Inspector's Vehicle section (PHYSICS.md 4.8): a walled-off client on the physics sandbox,
# the Land Rover made on its pad (rover-playground.js), build mode turned on and the chassis selected by inspect-rover.js
# through the Test hooks, the Inspector's content and the viewport saved as pictures in the results dir. The sandbox
# must be up with a physics server (domain-sandbox.sh physics <build> 42102, physics_server.enabled in its config).
#   tests-manual/physics-baseline/inspect-rover.sh [path/to/Darkride.app] [udp port] [results dir]
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
PORT=${2:-42102}
R=${3:-"$D/results/inspect-rover"}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R"
. "$D/harness.sh"
HTTP_PORT=$((PORT + 78))
TERRAIN_URL="http://127.0.0.1:$HTTP_PORT/rover.drkt"
[ -f "$R/rover.drkt" ] || python3 "$D/make-terrain.py" "$R/rover.drkt" 192 32 0.5
{ echo "var ROVER_CONFIG = { terrainURL: \"$TERRAIN_URL\" };"; echo "var INSPECT_DIR = \"$R\";";
  cat "$D/landrover.js" "$D/rover-playground.js" "$D/inspect-rover.js"; } > "$R/inspect-rover.js"
if ! lsof -nP -iTCP:$HTTP_PORT -sTCP:LISTEN > /dev/null 2>&1; then
    (cd "$R" && python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 > "$R/http.log" 2>&1) &
    HTTP_PID=$!
    trap 'kill $HTTP_PID 2>/dev/null || true' EXIT
    sleep 1
fi
export HIFI_METAVERSE_URL=${HIFI_METAVERSE_URL:-https://vr.darkride.ai}
# the client's snapshots (the viewport pictures) go beside the Inspector's
mkdir -p "$R/inspector-home/config/Tivoli Cloud VR - dev"
echo "{\"snapshotsLocation\": \"$R\"}" > "$R/inspector-home/config/Tivoli Cloud VR - dev/Interface.json"
client inspector --url "hifi://localhost:$PORT" --testScript "$R/inspect-rover.js" --testResultsLocation "$R/inspector" || true
grep -h "INSPECT-ROVER\|ROVER-PLAYGROUND" "$R/inspector.log" | tail -20
ls -la "$R"/*.png 2>/dev/null || echo "no pictures"
