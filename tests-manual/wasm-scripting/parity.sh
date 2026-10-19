#!/bin/sh
# Runs parity.js on the QtScript engine and on the WebAssembly engine (WASM-SCRIPTING.md round 3) and diffs
# the PARITY lines:  tests-manual/wasm-scripting/parity.sh [timeout seconds]
# Each client is this checkout's build, started through run-client.sh (one at a time, with a time limit); the
# logs are results/parity-{qtscript,wasm}/client.log, the PARITY lines results/parity/{qtscript,wasm}.txt.
set -e
D=$(cd "$(dirname "$0")" && pwd)
SECONDS_MAX=${1:-120}
R="$D/results/parity"
mkdir -p "$R"
run_one() {
    NAME=$1
    ON=$2
    DARKRIDE_RUN_JS_ON_WASM=$ON "$D/run-client.sh" "parity-$NAME" "$SECONDS_MAX" \
        --url "file://$D/world.json" --testScript "$D/parity.js" --testResultsLocation "$D/results/parity-$NAME"
    grep -o "PARITY .*" "$D/results/parity-$NAME/client.log" > "$R/$NAME.txt" || true
    echo "$NAME: $(wc -l < "$R/$NAME.txt") lines"
}
run_one qtscript 0
run_one wasm 1
echo "--- differences (< qtscript, > wasm):"
diff "$R/qtscript.txt" "$R/wasm.txt" || true
