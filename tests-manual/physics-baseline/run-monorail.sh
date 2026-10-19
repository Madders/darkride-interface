#!/bin/zsh
# PHYSICS.md round 5's acceptance test: the monorail with the physics server driving, twenty bot riders aboard and
# twenty bots walking below, two real clients measuring, one of them under packet loss.
#   tests-manual/physics-baseline/run-monorail.sh [path/to/Darkride.app] [results dir] [udp port] [riders] [walkers] [loss]
# The sandbox domain is restarted from the app's build (domain-sandbox.sh, which starts the physics server with the other
# services) with the bots as the domain's persistent scripts; then observer.js runs with TIVOLI_PACKET_LOSS=<loss>
# (0.05 by default: five per cent of its datagrams dropped, in and out) and track.js makes the track and the cabin, boards
# and rides a lap. At the end the bots are taken out of the sandbox's settings and the sandbox is stopped. The two
# clients' summaries (PHYSICS-TRACK, PHYSICS-OBSERVER) are printed and their samples land under <results dir>.
# Clients walled off, see harness.sh. Bots only while Auxi's mind is off (Caity's rule).
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/physics-monorail"}
PORT=${3:-42102}
RIDERS=${4:-20}
WALKERS=${5:-20}
LOSS=${6:-0.05}
BUILD=$(cd "$APP/../.." && pwd)
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
[ -x "$BUILD/assignment-client/assignment-client" ] || { echo "no assignment-client in $BUILD" >&2; exit 1; }
mkdir -p "$R/mover" "$R/observer"
. "$D/harness.sh"
CONFIG="$SANDBOX/domain-config.json"
# an agent refuses a script from the local filesystem, so the bots are served over HTTP from this folder
BOT_PORT=$((PORT + 78))
BOT="http://127.0.0.1:$BOT_PORT/monorail-bot.js"

set_bots() {   # set_bots <riders> <walkers>: the domain's persistent scripts, in the sandbox's config
    python3 - "$CONFIG" "$BOT" "$1" "$2" <<'PY'
import json, os, sys
path, bot, riders, walkers = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
config = {}
if os.path.exists(path):
    with open(path) as f:
        config = json.load(f)
scripts = []
if riders > 0:
    scripts.append({"url": bot + "?role=rider", "num_instances": riders, "pool": ""})
if walkers > 0:
    scripts.append({"url": bot + "?role=walker", "num_instances": walkers, "pool": ""})
config.setdefault("scripts", {})["persistent_scripts"] = scripts
with open(path, "w") as f:
    json.dump(config, f, indent=2)
print("persistent scripts:", len(scripts), "entries,", riders, "riders,", walkers, "walkers")
PY
}

count_agents() {   # the agents the domain lists (its nodes page is open to localhost)
    curl -s "http://localhost:$((PORT - 2))/nodes.json" 2>/dev/null | python3 -c '
import json, sys
try:
    nodes = json.load(sys.stdin)["nodes"]
    print(sum(1 for n in nodes if n.get("type") == "agent"))
except Exception:
    print(-1)'
}

echo "client: $BIN"
echo "build: $BUILD"
echo "domain: hifi://localhost:$PORT (restarting the physics sandbox from this build, with the physics server)"
echo "bots: $RIDERS riders, $WALKERS walkers; observer packet loss $LOSS"
echo "results: $R"
set_bots "$RIDERS" "$WALKERS"
(cd "$D/bots" && python3 -m http.server "$BOT_PORT" --bind 127.0.0.1 > "$R/bot-http.log" 2>&1) &
HTTP_PID=$!
# an earlier run's track and cabin (lifetime 600 s) would still be in the world, and a train on them
sandbox_stop
sandbox_clean_world baseline-
sandbox_start "$BUILD" "$PORT"

# the sandbox's own monitor adds a child only every 30 s, so the bots get a monitor of their own with a child each;
# they ask the domain for work and take the scripted assignments. Wait for them, up to two minutes. (The physics server
# is up first, sandbox_start waits for it: a bot monitor started before that would hand one of its children to it.)
WANT=$((RIDERS + WALKERS))
BOTS_PID=""
if [ "$WANT" -gt 0 ]; then
    sleep 3
    nohup "$BUILD/assignment-client/assignment-client" -n "$WANT" --max "$WANT" -a 127.0.0.1 --server-port "$PORT" \
        --log-directory "$SANDBOX/logs" > "$SANDBOX/logs/bots-monitor.log" 2>&1 &
    BOTS_PID=$!
    echo "bot monitor pid $BOTS_PID"
fi
HAVE=0
for i in $(seq 1 120); do
    HAVE=$(count_agents)
    [ "$HAVE" -ge "$WANT" ] 2>/dev/null && break
    sleep 1
done
echo "agents in the domain: $HAVE of $WANT after ${i}s"
sleep 5

# the observer first, under loss, so it is in the domain before the cabin appears (the mover's boot phase is 8 s)
TIVOLI_PACKET_LOSS="$LOSS" client observer --url "hifi://localhost:$PORT" --testScript "$D/observer.js" --testResultsLocation "$R/observer" &
OBSERVER=$!
wait_for_client observer
client mover --url "hifi://localhost:$PORT" --testScript "$D/track.js" --testResultsLocation "$R/mover" || true
wait $OBSERVER || true

grep "PHYSICS-TRACK" "$R/mover.log" || echo "no mover summary in $R/mover.log" >&2
grep "PHYSICS-OBSERVER" "$R/observer.log" || echo "no observer summary in $R/observer.log" >&2
grep -h "MONORAIL-BOT" $SANDBOX/logs/*.log 2>/dev/null | grep -c "aboard" | sed 's/^/bots that boarded (log lines): /'

# the sandbox's logs of this run go with the results
sandbox_copy_logs
cp "$SANDBOX/logs/bots-monitor.log" "$R/sandbox-bots-monitor.log" 2>/dev/null || true
grep "\[physics-server\]" "$R/sandbox-assignment-client.log" | grep -v "jitter\|Kbps\|Ping:\|Connection class" | grep -i "physics-server:\|late\|error\|crash\|fatal" | tail -30

# the bots go, their monitor with them (its children follow it, --parent-pid), the bots leave the config, the sandbox stops
if [ -n "$BOTS_PID" ]; then
    kill "$BOTS_PID" 2>/dev/null || true
fi
kill "$HTTP_PID" 2>/dev/null || true
set_bots 0 0
sandbox_stop
