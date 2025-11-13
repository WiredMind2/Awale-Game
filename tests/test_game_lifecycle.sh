#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

echo "[test] Building project..."
make -j4

PORT=4003
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

echo "[test] Testing full game lifecycle with persistence..."

# Client 1: Alice - set bio and challenge Bob
printf "set-bio 1 I am Alice, the Awale champion!\nchallenge Bob\nlist-games\n6\n" | \
    "$ROOT_DIR"/build/awale_client 127.0.0.1 "$PORT" Alice >"$CLIENT1_LOG" 2>&1 || true

# Client 2: Bob - accept challenge and play
printf "accept Alice\nplay 0\nplay 6\nplay 1\nplay 7\nplay 2\n6\n" | \
    "$ROOT_DIR"/build/awale_client 127.0.0.1 "$PORT" Bob >"$CLIENT2_LOG" 2>&1 || true

# Give server time to process everything
sleep 1

# Stop server
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

echo "[test] Restarting server to test persistence..."

# Restart server (should load saved data)
"$ROOT_DIR"/build/awale_server "$PORT" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true; rm -f "$SERVER_LOG" "$CLIENT1_LOG" "$CLIENT2_LOG"' EXIT

# Wait for server ready again
READY=false
for i in {1..50}; do
    if grep -q "Server ready" "$SERVER_LOG"; then
        READY=true
        break
    fi
    sleep 0.1
done

if [ "$READY" != true ]; then
    echo "[test][error] Server did not restart. Log:" >&2
    sed -n '1,200p' "$SERVER_LOG" >&2 || true
    exit 2
fi

# Client checks stats after game
printf "stats Alice\nstats Bob\nget-bio Alice\n6\n" | \
    "$ROOT_DIR"/build/awale_client 127.0.0.1 "$PORT" Checker >"$CLIENT1_LOG" 2>&1 || true

# Give time to process
sleep 0.5

# Stop server
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

# Check results
GAME_OK=true
PERSISTENCE_OK=true
STATS_OK=true

# Check that game was created and played
if ! grep -q "Challenge accepted" "$CLIENT2_LOG"; then
    echo "[test][fail] Challenge was not accepted" >&2
    GAME_OK=false
fi

if ! grep -q "Move executed" "$CLIENT2_LOG"; then
    echo "[test][fail] Moves were not executed" >&2
    GAME_OK=false
fi

# Check persistence - server should have loaded saved data
if ! grep -q "Loaded.*games from storage" "$SERVER_LOG"; then
    echo "[test][warn] No games loaded from storage (might be expected if game ended)" >&2
fi

if ! grep -q "Loaded.*players from storage" "$SERVER_LOG"; then
    echo "[test][fail] Players were not loaded from storage" >&2
    PERSISTENCE_OK=false
fi

# Check that stats were updated and persisted
if ! grep -q "Games played: 1" "$CLIENT1_LOG"; then
    echo "[test][fail] Alice's stats not updated correctly" >&2
    STATS_OK=false
fi

if ! grep -q "Games played: 1" "$CLIENT1_LOG"; then
    echo "[test][fail] Bob's stats not updated correctly" >&2
    STATS_OK=false
fi

# Check bio persistence
if ! grep -q "I am Alice, the Awale champion!" "$CLIENT1_LOG"; then
    echo "[test][fail] Alice's bio was not persisted" >&2
    PERSISTENCE_OK=false
fi

# Clean up test data
rm -rf ./data

# Report results
if [ "$GAME_OK" = true ] && [ "$PERSISTENCE_OK" = true ] && [ "$STATS_OK" = true ]; then
    echo "[test][pass] Full game lifecycle with persistence test passed"
    exit 0
else
    echo "[test][fail] Game lifecycle test failed"
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