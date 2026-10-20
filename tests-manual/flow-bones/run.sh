#!/bin/sh
# Measure the flow bones (spring bones) on the ookatoo avatar with a built client, serverless.
#   tests-manual/flow-bones/run.sh [path/to/Darkride.app] [results dir]
# Prints the FLOW-METRICS lines; the samples land in <results dir>/flow-metrics.json.
# The client gets its own settings directory (XDG_CONFIG_HOME) so the avatar it loads for the test does not
# replace the avatar saved in your own settings.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/flow-metrics"}
WORLD="$D/../physics-baseline/world.json"
mkdir -p "$R/config"
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
echo "client: $BIN"
echo "results: $R"
# --allowMultipleInstances: without it a second client hands its --url to the client already running and exits
XDG_CONFIG_HOME="$R/config" "$BIN" --allowMultipleInstances --url "file://$WORLD" --testScript "$D/flow-metrics.js" --testResultsLocation "$R" > "$R/client.log" 2>&1 || true
grep "FLOW-METRICS" "$R/client.log" || { echo "no summary in $R/client.log" >&2; exit 1; }
