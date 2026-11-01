#!/bin/sh
# Run the scripting baseline (WASM-SCRIPTING.md round 0) against a built client, serverless.
#   tests-manual/wasm-scripting/run.sh [path/to/Darkride.app] [results dir] [time limit in seconds, default 300]
# Prints the summary lines; the numbers land in <results dir>/wasm-baseline.json, the client's log in
# <results dir>/client.log, and the client's resident memory, sampled once a second, in <results dir>/rss.txt.
# The client starts through run-client.sh (release's client_start, the machine lock, a time limit), with its own
# settings and data directories under the results dir, so Caity's settings and the caches of any client already
# running are not touched (TIVOLI_APP_DATA_DIR needs 15a3068783 or later).
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/wasm-baseline"}
SECONDS_MAX=${3:-300}
mkdir -p "$R"
rm -f "$R/pid"
echo "client: $APP"
echo "results: $R"
: > "$R/rss.txt"
CLIENT_APP=$APP CLIENT_RESULTS=$R "$D/run-client.sh" wasm-baseline "$SECONDS_MAX" --url "file://$D/world.json" \
    --testScript "$D/baseline.js" --testResultsLocation "$R" &
RUNNER=$!
# run-client.sh writes the client's pid to $R/pid once it is up, and removes it when the client has gone
while kill -0 $RUNNER 2>/dev/null; do
    PID=$(cat "$R/pid" 2>/dev/null || true)
    [ -z "$PID" ] || ps -o rss= -p "$PID" >> "$R/rss.txt" 2>/dev/null || true
    sleep 1
done
wait $RUNNER || true
PEAK=$(sort -n "$R/rss.txt" | tail -1)
echo "WASM-BASELINE peak resident memory: $((${PEAK:-0} / 1024)) MB" | tee -a "$R/client.log"
grep "WASM-BASELINE" "$R/client.log" || { echo "no summary in $R/client.log" >&2; exit 1; }
