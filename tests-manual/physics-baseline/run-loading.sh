#!/bin/zsh
# PHYSICS.md round 10: how long the loading screen takes on the sandbox's world (udt's reliable path carries the
# initial scene), and how long the physics server takes to its first full scene.
#   tests-manual/physics-baseline/run-loading.sh [path/to/Darkride.app] [results dir] [udp port] [loss] [delay_ms] [world dir]
# Restarts the physics sandbox from the client's build (the earlier runs' test entities taken out of its world), then
# runs one walled-off client twice with loading.js: cold (a fresh home, so empty caches) and warm (the same home again,
# its caches kept between the two), each under TIVOLI_PACKET_LOSS=<loss> and TIVOLI_PACKET_DELAY_MS=<delay> when
# given. A world dir in the layout of ~/tivoli-test-worlds/<name> (models.json.gz, config.json, assets/) is put into
# the sandbox for the run and the sandbox's own world, assets and config are restored afterwards. Prints, per client,
# the time from its first log line to "Physics enabled", the script's own measure, the physics server's time to its
# first full scene, and the entity server's udt lines for the physics server and the client. TRACE=1 puts
# TIVOLI_ARRIVAL_TRACE=1 in the clients' environment (a line per octree packet while the safe landing tracks). Round 11:
# loading.js holds the arrival to the safe landing itself from its start, as the loading screen does while it is up, so
# both runs measure the gate; the gate line, the completions the entity server reported and the last tracking line are
# printed per run, and the entity server's first traversals and completions for the clients.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/physics-loading"}
PORT=${3:-42102}
LOSS=${4:-0}
DELAY=${5:-0}
WORLD=${6:-}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R"
. "$D/harness.sh"
BUILD=$(cd "$APP/../.." && pwd)

# The domain-server keeps its own copy of the world (appdata/entities/models.json.gz) and hands it to the entity
# server at start whenever the entity server's file has another id or an older data version (DomainServer::
# processOctreeDataRequestMessage), so a world put only into entities/ is replaced by the sandbox's own world before
# the first client arrives: round 10's and round 11's first "Tivoli Town" runs served the sandbox's world at Tivoli
# Town's spawn (found 2026-10-19 by the entity count the headless viewers held, 1,828 against 2,045). Both copies are
# swapped and restored.
DS_WORLD="$SANDBOX/appdata/entities/models.json.gz"

restore_world() {
    if [ -d "$R/sandbox-backup" ]; then
        rm -rf "$SANDBOX/entities/models.json.gz" "$SANDBOX/assets"
        cp "$R/sandbox-backup/models.json.gz" "$SANDBOX/entities/models.json.gz"
        if [ -f "$R/sandbox-backup/ds-models.json.gz" ]; then
            cp "$R/sandbox-backup/ds-models.json.gz" "$DS_WORLD"
        else
            rm -f "$DS_WORLD"
        fi
        [ -d "$R/sandbox-backup/assets" ] && cp -R "$R/sandbox-backup/assets" "$SANDBOX/assets" || mkdir -p "$SANDBOX/assets"
        cp "$R/sandbox-backup/domain-config.json" "$SANDBOX/domain-config.json"
        rm -rf "$R/sandbox-backup"
        echo "sandbox world, assets and config restored"
    fi
}

sandbox_stop
if [ -n "$WORLD" ]; then
    [ -f "$WORLD/models.json.gz" ] || { echo "no world at $WORLD" >&2; exit 1; }
    rm -rf "$R/sandbox-backup"; mkdir -p "$R/sandbox-backup"
    cp "$SANDBOX/entities/models.json.gz" "$R/sandbox-backup/models.json.gz"
    [ -f "$DS_WORLD" ] && cp "$DS_WORLD" "$R/sandbox-backup/ds-models.json.gz"
    [ -d "$SANDBOX/assets" ] && cp -R "$SANDBOX/assets" "$R/sandbox-backup/assets"
    cp "$SANDBOX/domain-config.json" "$R/sandbox-backup/domain-config.json"
    trap 'sandbox_stop; restore_world' EXIT
    cp "$WORLD/models.json.gz" "$SANDBOX/entities/models.json.gz"
    mkdir -p "$(dirname "$DS_WORLD")"
    cp "$WORLD/models.json.gz" "$DS_WORLD"
    rm -rf "$SANDBOX/assets"; mkdir -p "$SANDBOX/assets"
    [ -d "$WORLD/assets" ] && cp -R "$WORLD/assets/." "$SANDBOX/assets/"
    python3 - "$SANDBOX/domain-config.json" "$WORLD/config.json" <<'PY'
import json, os, sys
path, world_config = sys.argv[1:3]
config = json.load(open(path)) if os.path.exists(path) else {}
world = json.load(open(world_config)) if os.path.exists(world_config) else {}
if world.get("paths"):
    config["paths"] = world["paths"]
json.dump(config, open(path, "w"), indent=2)
PY
    echo "world: $WORLD ($(python3 -c 'import gzip,json,sys; print(len(json.load(gzip.open(sys.argv[1])).get("Entities",[])))' "$WORLD/models.json.gz") entities) put into the sandbox (the entity server's file and the domain-server's copy) for this run"
else
    sandbox_clean_world rover- airship- aircraft-
    trap 'sandbox_stop' EXIT
fi
sandbox_start "$BUILD" "$PORT"

URL="hifi://localhost:$PORT"
echo "client: $BIN"
echo "domain: $URL; the client under packet loss $LOSS and a delay of $DELAY ms; results: $R; arrival trace ${TRACE:-off}"
rm -rf "$R/arrival-home" "$R/arrival-cold.log" "$R/arrival-warm.log"
cp "$D/loading.js" "$R/loading.js"
for PASS in cold warm; do
    KEEP=1; [ "$PASS" = "warm" ] && KEEP=""
    sleep 2
    KEEP_KTX=$KEEP TIVOLI_PACKET_LOSS="$LOSS" TIVOLI_PACKET_DELAY_MS="$DELAY" TIVOLI_ARRIVAL_TRACE="${TRACE:-}" \
        client arrival --url "$URL" --testScript "$R/loading.js" || true
    cp "$R/arrival.log" "$R/arrival-$PASS.log"
done
sandbox_copy_logs
sandbox_stop
trap - EXIT
restore_world

secs() { python3 -c 'import sys,datetime; t=datetime.datetime.strptime(sys.argv[1], "%m/%d %H:%M:%S"); print(t.hour*3600+t.minute*60+t.second)' "$1"; }
for PASS in cold warm; do
    LOG="$R/arrival-$PASS.log"
    FIRST=$(grep -m1 -o '^\[[0-9/]* [0-9:]*\]' "$LOG" | tr -d '[]')
    ENABLED=$(grep -m1 "Physics enabled" "$LOG" | grep -o '^\[[0-9/]* [0-9:]*\]' | tr -d '[]')
    if [ -n "$FIRST" ] && [ -n "$ENABLED" ]; then
        echo "== $PASS: launch to Physics enabled $(( $(secs "$ENABLED") - $(secs "$FIRST") )) s (first line $FIRST, enabled $ENABLED)"
    else
        echo "== $PASS: no Physics enabled line in $LOG"
    fi
    grep "PHYSICS-LOADING" "$LOG" | sed 's/^/   /' || true
    grep "Physics enabled" "$LOG" | sed 's/.*Physics enabled/   gate: Physics enabled/' | head -1 || true
    grep "safe landing: the entity server reports\|safe landing: complete" "$LOG" | sed 's/.*safe landing:/   safe landing:/' | head -8 || true
    grep "safe landing: tracking" "$LOG" | tail -1 | sed 's/.*safe landing:/   last: safe landing:/' || true
done
echo "== the world the entity server served: $(grep -o 'known [0-9]* entities' "$R/sandbox-assignment-client.log" | sort | uniq -c | sort -rn | head -2 | tr '\n' ';')"
echo "== entity server's traversals for the clients (first traversals and completions reported)"
grep "first traversal for\|initial scene complete for" "$R/sandbox-assignment-client.log" | grep -v "Physics Server" | sed 's/.*\[entity-server\]/   /' | head -16 || true
echo "== physics server"
grep "reported its first full scene" "$R/sandbox-assignment-client.log" | grep "physics-server" | sed 's/.*reported/   reported/' || echo "   no first-scene line"
echo "== entity server's send threads (ten-second lines)"
grep "send thread for" "$R/sandbox-assignment-client.log" | grep -v "Entity Script Server\|Avatar Mixer\|Audio Mixer\|Asset Server\|Messages Mixer" | sed 's/.*send thread for/   /' | head -12
