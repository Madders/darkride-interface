#!/bin/sh
# Run the scripting baseline (WASM-SCRIPTING.md round 0) against a built client, serverless.
#   tests-manual/wasm-scripting/run.sh [path/to/Darkride.app] [results dir] [time limit in seconds, default 300]
# Prints the summary lines; the numbers land in <results dir>/wasm-baseline.json, the client's log in
# <results dir>/client.log, and the client's resident memory, sampled once a second, in <results dir>/rss.txt.
# The client gets its own settings and data directories under the results dir, so Caity's settings and the
# caches of any client already running are not touched (TIVOLI_APP_DATA_DIR needs 15a3068783 or later).
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/wasm-baseline"}
SECONDS_MAX=${3:-300}
mkdir -p "$R/config" "$R/data"
BIN="$APP/Contents/MacOS/Darkride"
BIN=$(cd "$(dirname "$BIN")" && pwd)/Darkride
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
echo "client: $BIN"
echo "results: $R"
# --allowMultipleInstances: without it a second client hands its --url to the client already running and exits
XDG_CONFIG_HOME="$R/config" TIVOLI_APP_DATA_DIR="$R/data" "$BIN" --allowMultipleInstances --url "file://$D/world.json" \
    --testScript "$D/baseline.js" --testResultsLocation "$R" > "$R/client.log" 2>&1 &
PID=$!
: > "$R/rss.txt"
WAITED=0
while kill -0 $PID 2>/dev/null; do
    ps -o rss= -p $PID >> "$R/rss.txt" 2>/dev/null || true
    if [ $WAITED -ge "$SECONDS_MAX" ]; then
        echo "time limit of $SECONDS_MAX s reached, stopping the client"
        "$D/stop-clients.sh" > /dev/null
        break
    fi
    sleep 1
    WAITED=$((WAITED + 1))
done
PEAK=$(sort -n "$R/rss.txt" | tail -1)
echo "WASM-BASELINE peak resident memory: $((PEAK / 1024)) MB" | tee -a "$R/client.log"
grep "WASM-BASELINE" "$R/client.log" || { echo "no summary in $R/client.log" >&2; exit 1; }
