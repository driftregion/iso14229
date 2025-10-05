#!/bin/bash

set -euxo pipefail

cleanup() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 $SERVER_PID 2>/dev/null; then
        kill $SERVER_PID 2>/dev/null || true
    fi
    if [ -n "${CLIENT_PID:-}" ] && kill -0 $CLIENT_PID 2>/dev/null; then
        kill $CLIENT_PID 2>/dev/null || true
    fi
}

trap cleanup EXIT

make

(sleep 5; echo "Test timed out after 5 seconds"; kill -TERM $$ 2>/dev/null) &
TIMEOUT_PID=$!

./server &
SERVER_PID=$!
# Give server time to start
sleep 1

./client &
CLIENT_PID=$!

wait $CLIENT_PID
CLIENT_EXIT=$?

if ! kill -0 $SERVER_PID 2>/dev/null; then
    wait $SERVER_PID
    SERVER_EXIT=$?
    echo "Server exited prematurely with code $SERVER_EXIT"
    kill $TIMEOUT_PID 2>/dev/null || true
    exit 1
fi

kill $SERVER_PID
wait $SERVER_PID || true

if [ $CLIENT_EXIT -ne 0 ]; then
    echo "Client exited with code $CLIENT_EXIT"
    kill $TIMEOUT_PID 2>/dev/null || true
    exit 1
fi

kill $TIMEOUT_PID 2>/dev/null || true

echo "Both client and server exited successfully"
