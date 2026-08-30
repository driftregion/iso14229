#!/bin/bash

set -euo pipefail

cleanup() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 $SERVER_PID 2>/dev/null; then
        echo "Cleaning up server (PID $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
}

trap cleanup EXIT

make

# 1) Start doip_server in bg and store server PID
./doip_server &
SERVER_PID=$!
echo "Started server with PID $SERVER_PID"

# Give server time to start
sleep 1

# 2) Run doip_client in bg
./doip_client &
CLIENT_PID=$!
echo "Started client with PID $CLIENT_PID"

# 3) Wait 5s
sleep 5

# 4) Verify that client exited normally (return code 0)
if kill -0 $CLIENT_PID 2>/dev/null; then
    echo "ERROR: Client is still running after 10 seconds"
    kill $CLIENT_PID 2>/dev/null || true
    exit 1
fi

wait $CLIENT_PID 2>/dev/null
CLIENT_EXIT=$?

if [ $CLIENT_EXIT -ne 0 ]; then
    echo "ERROR: Client exited with code $CLIENT_EXIT (expected 0)"
    exit 1
fi

echo "Client exited normally with code 0"

# 5) Verify that server is alive
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server is not running"
    exit 1
fi

echo "Server is still running (PID $SERVER_PID)"

# 6) Kill the server
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true
echo "Server stopped"

echo "Test passed successfully"

echo "Both client and server exited successfully"
