#!/bin/sh
# Measure the flow bones across the network: one client runs flow-metrics.js on the ookatoo avatar, another
# runs flow-observer.js and measures the same chains from the joint data it receives. The domain is a sandbox
# started with domain-sandbox.sh (any build; the avatar mixer is all that matters) and must be up first.
#   tests-manual/flow-bones/run-pair.sh [path/to/Darkride.app] [results dir] [udp port]
# Prints the mover's FLOW-METRICS and the observer's FLOW-OBSERVER summary lines; the samples land in
# <results dir>/mover/flow-metrics.json and <results dir>/observer/flow-observer.json.
# Each client gets its own settings directory (XDG_CONFIG_HOME), so your own settings are untouched.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/flow-pair"}
PORT=${3:-42102}
URL="hifi://localhost:$PORT"
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R/mover/config" "$R/observer/config"
export HIFI_METAVERSE_URL=${HIFI_METAVERSE_URL:-https://vr.darkride.ai}
echo "client: $BIN"
echo "domain: $URL"
echo "results: $R"
# the observer first, so that it is in the domain before the mover's avatar appears
XDG_CONFIG_HOME="$R/observer/config" "$BIN" --allowMultipleInstances --url "$URL" --testScript "$D/flow-observer.js" --testResultsLocation "$R/observer" > "$R/observer.log" 2>&1 &
OBSERVER=$!
sleep 12
XDG_CONFIG_HOME="$R/mover/config" "$BIN" --allowMultipleInstances --url "$URL" --testScript "$D/flow-metrics.js" --testResultsLocation "$R/mover" > "$R/mover.log" 2>&1 || true
wait $OBSERVER || true
grep "FLOW-METRICS" "$R/mover.log" | grep -E "ALL|poses|phase|ready" || echo "no mover summary in $R/mover.log" >&2
grep "FLOW-OBSERVER" "$R/observer.log" | grep -E "ALL|poses|received|resolved|phase|joint data" || echo "no observer summary in $R/observer.log" >&2
