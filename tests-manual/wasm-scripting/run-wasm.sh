#!/bin/sh
# Runs a WebAssembly script module in a serverless client (WASM-SCRIPTING.md round 2's acceptance test):
#   tests-manual/wasm-scripting/run-wasm.sh <module.wasm> [path/to/Darkride.app] [timeout seconds]
# The module is the client's --testScript, so the client quits when the script stops; the log is in
# results/wasm-run/client.log and the script's own lines are printed. The client starts through run-client.sh
# (release's client_start, the machine lock, its own settings and data under results/wasm-run), small and behind the
# other windows: CLIENT_FOREGROUND=1 opens it in front, for a module whose dialog a person closes (alert.wasm).
set -e
D=$(cd "$(dirname "$0")" && pwd)
[ -n "$1" ] || { echo "usage: $0 <module.wasm> [Darkride.app] [seconds]" >&2; exit 1; }
MOD=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
APP=${2:-"$D/../../build/interface/Darkride.app"}
SECONDS_MAX=${3:-90}
echo "client: $APP"
echo "module: $MOD"
CLIENT_APP=$APP "$D/run-client.sh" wasm-run "$SECONDS_MAX" --url "file://$D/world.json" \
    --testScript "$MOD" --testResultsLocation "$D/results/wasm-run"
grep -E 'WebAssembly|\.wasm|\[script\]|orbit:|hang:|alert:|wasm' "$D/results/wasm-run/client.log" | grep -v 'qml\|BaseWebView\|Registering a packet' || true
