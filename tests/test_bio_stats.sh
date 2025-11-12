#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

echo "[test] Building project..."
make -j4

PORT=4002
SERVER_LOG="$(mktemp)"
CLIENT1_LOG="$(mktemp)"
CLIENT2_LOG="$(mktemp)"
OUTPUT="$(mktemp)"

# Start server in background
"$ROOT_DIR"/build/awale_server "$PORT" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true; rm -f "$SERVER_LOG" "$CLIENT1_LOG" "$CLIENT2_LOG" "$OUTPUT"' EXIT

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

echo "[test] Testing bio and stats functionality..."

# Test 1: Client sets bio and views it
printf "set-bio 1 Hello, this is my first bio line\nset-bio 2 And this is my second line\nget-bio TestPlayer\nstats TestPlayer\n6\n" | \
    "$ROOT_DIR"/build/awale_client 127.0.0.1 "$PORT" TestPlayer >"$CLIENT1_LOG" 2>&1 || true

# Test 2: Another client views the first client's bio and stats
printf "get-bio TestPlayer\nstats TestPlayer\n6\n" | \
    "$ROOT_DIR"/build/awale_client 127.0.0.1 "$PORT" ViewerPlayer >"$CLIENT2_LOG" 2>&1 || true

# Give server a moment to process
sleep 0.5

# Check results
BIO_OK=true
STATS_OK=true

# Check that bio was set and retrieved
if ! grep -q "Bio set successfully" "$CLIENT1_LOG"; then
    echo "[test][fail] Bio was not set successfully" >&2
    BIO_OK=false
fi

if ! grep -q "Hello, this is my first bio line" "$CLIENT1_LOG"; then
    echo "[test][fail] Bio line 1 not found in own view" >&2
    BIO_OK=false
fi

if ! grep -q "And this is my second line" "$CLIENT1_LOG"; then
    echo "[test][fail] Bio line 2 not found in own view" >&2
    BIO_OK=false
fi

if ! grep -q "Hello, this is my first bio line" "$CLIENT2_LOG"; then
    echo "[test][fail] Bio line 1 not found in other client's view" >&2
    BIO_OK=false
fi

# Check stats functionality
if ! grep -q "Games played:" "$CLIENT1_LOG"; then
    echo "[test][fail] Stats not displayed for own player" >&2
    STATS_OK=false
fi

if ! grep -q "Games played:" "$CLIENT2_LOG"; then
    echo "[test][fail] Stats not displayed for other player" >&2
    STATS_OK=false
fi

# Stop server
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

# Report results
if [ "$BIO_OK" = true ] && [ "$STATS_OK" = true ]; then
    echo "[test][pass] Bio and stats integration test passed"
    exit 0
else
    echo "[test][fail] Bio and/or stats test failed"
    echo ""
    echo "--- Client 1 (TestPlayer) output ---" >&2
    sed -n '1,100p' "$CLIENT1_LOG" >&2 || true
    echo ""
    echo "--- Client 2 (ViewerPlayer) output ---" >&2
    sed -n '1,100p' "$CLIENT2_LOG" >&2 || true
    echo ""
    echo "--- Server log ---" >&2
    sed -n '1,200p' "$SERVER_LOG" >&2 || true
    exit 1
fi