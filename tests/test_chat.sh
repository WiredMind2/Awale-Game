#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

echo "[test] Building project..."
make -j4

PORT=4004
SERVER_LOG="$(mktemp)"
CLIENT1_LOG="$(mktemp)"
CLIENT2_LOG="$(mktemp)"

# Clean any existing data
rm -rf ./data

# Start server in background
"$ROOT_DIR"/build/awale_server "$PORT" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true; rm -f "$SERVER_LOG" "$CLIENT1_LOG" "$CLIENT2_LOG"' EXIT

# Wait for server ready
READY=false
for i in {1..50}; do
    if grep -q "Server ready" "$SERVER_LOG"; then
        READY=true
        break
    fi
    sleep 0.1
done

if [ "$READY" != true ]; then
    echo "[test][error] Server did not start. Log:" >&2
    sed -n '1,200p' "$SERVER_LOG" >&2 || true
    exit 2
fi

echo "[test] Testing chat functionality between multiple clients..."

# Client 1: Alice - connect and wait
printf "9\nall\nHello everyone from Alice!\nexit\n6\n" | \
    "$ROOT_DIR"/build/awale_client 127.0.0.1 "$PORT" Alice >"$CLIENT1_LOG" 2>&1 || true &

CLIENT1_PID=$!

# Give Alice time to connect
sleep 1

# Client 2: Bob - connect, send global message, then private message to Alice
printf "9\nall\nHi from Bob!\nexit\n9\nAlice\nPrivate message from Bob to Alice\nexit\n6\n" | \
    "$ROOT_DIR"/build/awale_client 127.0.0.1 "$PORT" Bob >"$CLIENT2_LOG" 2>&1 || true &

CLIENT2_PID=$!

# Give time for all messages to be processed
sleep 2

# Stop server
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

# Wait for clients to finish
wait "$CLIENT1_PID" 2>/dev/null || true
wait "$CLIENT2_PID" 2>/dev/null || true

# Check results
CHAT_OK=true
GLOBAL_OK=true
PRIVATE_OK=true

# Check that Alice received Bob's global message
if ! grep -q "GLOBAL CHAT from Bob:" "$CLIENT1_LOG"; then
    echo "[test][fail] Alice did not receive Bob's global message" >&2
    GLOBAL_OK=false
fi

# Check that Bob received Alice's global message
if ! grep -q "GLOBAL CHAT from Alice:" "$CLIENT2_LOG"; then
    echo "[test][fail] Bob did not receive Alice's global message" >&2
    GLOBAL_OK=false
fi

# Check that Alice received Bob's private message
if ! grep -q "PRIVATE MESSAGE from Bob:" "$CLIENT1_LOG"; then
    echo "[test][fail] Alice did not receive Bob's private message" >&2
    PRIVATE_OK=false
fi

# Check that messages were sent successfully (check for "sent" confirmations)
if ! grep -q "Global message sent:" "$CLIENT1_LOG"; then
    echo "[test][fail] Alice's global message was not sent successfully" >&2
    CHAT_OK=false
fi

if ! grep -q "Global message sent:" "$CLIENT2_LOG"; then
    echo "[test][fail] Bob's global message was not sent successfully" >&2
    CHAT_OK=false
fi

if ! grep -q "Private message sent to Alice:" "$CLIENT2_LOG"; then
    echo "[test][fail] Bob's private message was not sent successfully" >&2
    CHAT_OK=false
fi

# Clean up test data
rm -rf ./data

# Report results
if [ "$CHAT_OK" = true ] && [ "$GLOBAL_OK" = true ] && [ "$PRIVATE_OK" = true ]; then
    echo "[test][pass] Chat functionality test passed"
    exit 0
else
    echo "[test][fail] Chat functionality test failed"
    echo ""
    echo "--- Alice output ---" >&2
    sed -n '1,50p' "$CLIENT1_LOG" >&2 || true
    echo ""
    echo "--- Bob output ---" >&2
    sed -n '1,50p' "$CLIENT2_LOG" >&2 || true
    echo ""
    echo "--- Server log ---" >&2
    sed -n '1,100p' "$SERVER_LOG" >&2 || true
    exit 1
fi