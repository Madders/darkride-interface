#!/bin/sh
# Runs a WebAssembly script module in a serverless client (WASM-SCRIPTING.md round 2's acceptance test):
#   tests-manual/wasm-scripting/run-wasm.sh <module.wasm> [path/to/Darkride.app] [timeout seconds]
# The module is the client's --testScript, so the client quits when the script stops; the log is in
# results/wasm-run/client.log and the script's own lines are printed. The client gets its own settings and
# data directories under results/wasm-run.
set -e
D=$(cd "$(dirname "$0")" && pwd)
[ -n "$1" ] || { echo "usage: $0 <module.wasm> [Darkride.app] [seconds]" >&2; exit 1; }
MOD=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
APP=${2:-"$D/../../build/interface/Darkride.app"}
SECONDS_MAX=${3:-90}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
R="$D/results/wasm-run"
mkdir -p "$R/config" "$R/data"
echo "client: $BIN"
echo "module: $MOD"
XDG_CONFIG_HOME="$R/config" TIVOLI_APP_DATA_DIR="$R/data" "$BIN" --allowMultipleInstances --url "file://$D/world.json" \
    --testScript "$MOD" --testResultsLocation "$R" > "$R/client.log" 2>&1 &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
    if [ $WAITED -ge "$SECONDS_MAX" ]; then
        echo "timeout after $SECONDS_MAX s, stopping the client"
        kill $PID 2>/dev/null || true
        break
    fi
    sleep 1
    WAITED=$((WAITED + 1))
done
grep -E 'WebAssembly|\.wasm|\[script\]|orbit:|hang:|alert:|wasm' "$R/client.log" | grep -v 'qml\|BaseWebView\|Registering a packet' || true
