#!/bin/sh
# The one way a test client of this checkout is started: with the absolute path of the build's client, its own
# settings and data under results/<name>, a hard time limit, and every client of this checkout stopped before
# and after. Prints the client's pid; the log is results/<name>/client.log.
#   tests-manual/wasm-scripting/run-client.sh <name> <seconds> [client arguments...]
# Environment: DARKRIDE_RUN_JS_ON_WASM=1 runs the JavaScript scripts on the WebAssembly engine; HIFI_METAVERSE_URL
# names the metaverse server (https://vr.darkride.ai unless given).
set -e
# The metaverse server the client signs in to and finds worlds on; the client's built-in default is out of date.
export HIFI_METAVERSE_URL=${HIFI_METAVERSE_URL:-https://vr.darkride.ai}
D=$(cd "$(dirname "$0")" && pwd)
NAME=$1
SECONDS_MAX=$2
shift 2
[ -n "$NAME" ] && [ -n "$SECONDS_MAX" ] || { echo "usage: $0 <name> <seconds> [client arguments...]" >&2; exit 1; }
BIN="$D/../../build/interface/Darkride.app/Contents/MacOS/Darkride"
BIN=$(cd "$(dirname "$BIN")" && pwd)/Darkride
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
R="$D/results/$NAME"
"$D/stop-clients.sh" > /dev/null
rm -rf "$R"
mkdir -p "$R/config" "$R/data"
XDG_CONFIG_HOME="$R/config" TIVOLI_APP_DATA_DIR="$R/data" "$BIN" --allowMultipleInstances "$@" > "$R/client.log" 2>&1 &
PID=$!
echo "$PID" > "$R/pid"
echo "client $PID, log $R/client.log"
WAITED=0
while kill -0 $PID 2>/dev/null; do
    if [ $WAITED -ge "$SECONDS_MAX" ]; then
        echo "time limit of $SECONDS_MAX s reached, stopping the client"
        break
    fi
    sleep 1
    WAITED=$((WAITED + 1))
done
"$D/stop-clients.sh" > /dev/null
rm -f "$R/pid"
