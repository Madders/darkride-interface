#!/bin/sh
# Dump the API a script sees, serverless (WASM-SCRIPTING.md round 0): tests-manual/wasm-scripting/dump-api.sh
# Writes results/api-client.json and results/api-entity_client.json beside this script. The client is this checkout's
# build, started through run-client.sh (a time limit, its own settings and data); its log is results/dump-api/client.log.
set -e
D=$(cd "$(dirname "$0")" && pwd)
"$D/run-client.sh" dump-api 90 --url "file://$D/world.json" --testScript "$D/dump-api.js" --testResultsLocation "$D/results/dump-api"
grep "WASM-API" "$D/results/dump-api/client.log" || { echo "no WASM-API lines in results/dump-api/client.log" >&2; exit 1; }
ls -la "$D"/results/api-*.json
