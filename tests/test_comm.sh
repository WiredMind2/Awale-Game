#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

echo "[test] Building project..."
make -j4

PORT=4001
SERVER_LOG="$(mktemp)"
OUTPUT="$(mktemp)"

# Start server in background
"$ROOT_DIR"/build/awale_server "$PORT" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true; rm -f "$SERVER_LOG" "$OUTPUT"' EXIT

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

# Run client: list players (1) then quit (6)
printf "1\n6\n" | "$ROOT_DIR"/build/awale_client 127.0.0.1 "$PORT" testbot >"$OUTPUT" 2>&1 || true

# Give server a moment to log
sleep 0.2

# Check client output for expected lines
CLIENT_OK=true
if ! grep -q "✓ Connected to server" "$OUTPUT"; then
    echo "[test][fail] Client did not report successful connection" >&2
    CLIENT_OK=false
fi
if ! grep -q "Connected players" "$OUTPUT"; then
    echo "[test][warn] Client did not receive a player list response" >&2
    CLIENT_OK=false
fi

# Check server log
if ! grep -q "Client thread started for testbot" "$SERVER_LOG"; then
    echo "[test][fail] Server did not start client handler thread" >&2
    CLIENT_OK=false
fi

# Stop server
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

if [ "$CLIENT_OK" = true ]; then
    echo "[test][pass] Communication test passed"
    exit 0
else
    echo "\n--- Client output ---" >&2
    sed -n '1,200p' "$OUTPUT" >&2 || true
    echo "\n--- Server log ---" >&2
    sed -n '1,400p' "$SERVER_LOG" >&2 || true
    exit 1
fi
