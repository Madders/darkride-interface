#!/bin/sh
# The one way a test client of this checkout is started: with the absolute path of the build's client, its own
# settings and data under results/<name>, a hard time limit, and every client of this checkout stopped before
# and after. Prints the client's pid; the log is results/<name>/client.log. The run's texture cache is not kept.
#   tests-manual/wasm-scripting/run-client.sh <name> <seconds> [client arguments...]
# Environment: DARKRIDE_RUN_JS_ON_WASM=1 runs the JavaScript scripts on the WebAssembly engine; HIFI_METAVERSE_URL
# names the metaverse server (https://vr.darkride.ai unless given); DARKRIDE_CLIENT_LABEL is the label the window
# title starts with ("Darkride - Wasm - <name>" unless given), so that a test client is told apart from the other
# clients on the same desktop.
set -e
# The metaverse server the client signs in to and finds worlds on; the client's built-in default is out of date.
export HIFI_METAVERSE_URL=${HIFI_METAVERSE_URL:-https://vr.darkride.ai}
D=$(cd "$(dirname "$0")" && pwd)
NAME=$1
SECONDS_MAX=$2
shift 2
[ -n "$NAME" ] && [ -n "$SECONDS_MAX" ] || { echo "usage: $0 <name> <seconds> [client arguments...]" >&2; exit 1; }
export DARKRIDE_CLIENT_LABEL=${DARKRIDE_CLIENT_LABEL:-"Darkride - Wasm - $NAME"}
BIN="$D/../../build/interface/Darkride.app/Contents/MacOS/Darkride"
BIN=$(cd "$(dirname "$BIN")" && pwd)/Darkride
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
R="$D/results/$NAME"
# A scripted client (given --testScript) goes straight in, signed out: the sign-in screen would keep its script from
# running. `--tokens` and `{}` as two arguments; safe only because this client's settings and account file are its
# own (results/<name>). A client for a person's hands keeps the sign-in screen.
for ARG in "$@"; do
    if [ "$ARG" = "--testScript" ]; then
        set -- "$@" --tokens "{}"
        break
    fi
done
"$D/stop-clients.sh" > /dev/null
# A run's texture cache (data/ktx_cache: 280 MB for a short run, a few GB for a long one) is never used again: every
# run starts with new settings, and the client empties the cache when the cache's version setting is missing. So
# none is kept: this run's goes when the run ends, and any that an interrupted run left behind goes here.
rm -rf "$D"/results/*/data/ktx_cache
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
rm -rf "$R/data/ktx_cache"
rm -f "$R/pid"
