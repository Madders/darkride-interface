#!/bin/sh
# Stops every client of this checkout (a client process started from this checkout's build, by the path
# run-client.sh starts it with), and nothing else: the clients of other checkouts and other people are left alone.
# SIGTERM first, SIGKILL after 5 s. Run it before and after every test run.
#   tests-manual/wasm-scripting/stop-clients.sh
CHECKOUT=$(cd "$(dirname "$0")/../.." && pwd)
CLIENT="$CHECKOUT/build/interface/Darkride.app/Contents/MacOS/Darkride"
my_clients() {
    ps -ax -o pid=,command= | grep -F "$CLIENT" | grep -v grep | awk '{print $1}'
}
PIDS=$(my_clients)
if [ -z "$PIDS" ]; then
    echo "no clients of $CHECKOUT are running"
    exit 0
fi
for P in $PIDS; do
    echo "stopping $P: $(ps -o etime= -p $P | tr -d ' ') old, $(ps -o command= -p $P | cut -c1-80)"
    kill $P 2>/dev/null
done
sleep 5
for P in $PIDS; do
    if kill -0 $P 2>/dev/null; then
        echo "$P did not stop, killing it"
        kill -9 $P 2>/dev/null
    fi
done
sleep 1
echo "remaining: $(my_clients | wc -l | tr -d ' ')"
