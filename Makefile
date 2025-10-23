# Makefile for Awale Game
# POSIX shell required (bash, sh). On Windows use WSL / MSYS2.

CC := gcc
CFLAGS := -Wall -Wextra -O2

SERVER_SRC := awale_server.c
CLIENT_SRC := awale_client.c

BUILD_DIR := build
SERVER_BIN := $(BUILD_DIR)/awale_server
CLIENT_BIN := $(BUILD_DIR)/awale_client

.PHONY: all server client clean run-server run-client run-example

all: server client

server: $(SERVER_BIN)

client: $(CLIENT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SERVER_BIN): $(SERVER_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(CLIENT_BIN): $(CLIENT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)
	rm -rf demo_logs

# Run server in foreground on chosen port
run-server:
	@echo "Usage: make run-server PORT=12345"
	@if [ -z "$(PORT)" ]; then echo "Please set PORT, e.g. make run-server PORT=12345"; exit 1; fi
	$(SERVER_BIN) $(PORT)

# Run a client connecting to localhost
run-client:
	@echo "Usage: make run-client PSEUDO=Alice PORT=12345"
	@if [ -z "$(PSEUDO)" -o -z "$(PORT)" ]; then echo "Please set PSEUDO and PORT, e.g. make run-client PSEUDO=Alice PORT=12345"; exit 1; fi
	$(CLIENT_BIN) 127.0.0.1 $(PORT) $(PSEUDO)

# Run example: build and print commands for starting server and two clients
run-example: all
	@echo "To run locally in separate terminals:"
	@echo "  $(SERVER_BIN) 12345"
	@echo "  $(CLIENT_BIN) 127.0.0.1 12345 Alice"
	@echo "  $(CLIENT_BIN) 127.0.0.1 12345 Bob"
	@echo "\nOn Windows use WSL: open an Ubuntu shell, cd to the project path and run the same commands."

# Start a demo: build, launch server and two clients in background and save PIDs/logs
# Usage: make start-demo PORT=12345 PSEUDO_A=Alice PSEUDO_B=Bob
.PHONY: start-demo stop-demo

start-demo: all
	@if [ -z "$(PORT)" ]; then echo "Please set PORT, e.g. make start-demo PORT=12345"; exit 1; fi
	@if [ -z "$(PSEUDO_A)" ]; then PSEUDO_A=Alice; fi
	@if [ -z "$(PSEUDO_B)" ]; then PSEUDO_B=Bob; fi
	mkdir -p demo_logs
	# Start server in background
	$(SERVER_BIN) $(PORT) > demo_logs/server.out 2> demo_logs/server.err & echo $$! > demo_logs/server.pid
	# Give server a moment to start
	sleep 0.5
	# Start two clients in background
	$(CLIENT_BIN) 127.0.0.1 $(PORT) $(PSEUDO_A) > demo_logs/client_A.out 2> demo_logs/client_A.err & echo $$! > demo_logs/client_A.pid
	$(CLIENT_BIN) 127.0.0.1 $(PORT) $(PSEUDO_B) > demo_logs/client_B.out 2> demo_logs/client_B.err & echo $$! > demo_logs/client_B.pid
	@echo "Started demo: server pid=$(shell cat demo_logs/server.pid) clientA=$(shell cat demo_logs/client_A.pid) clientB=$(shell cat demo_logs/client_B.pid)"

stop-demo:
	@echo "Stopping demo processes if running..."
	-if [ -f demo_logs/client_A.pid ]; then kill `cat demo_logs/client_A.pid` 2>/dev/null || true; fi
	-if [ -f demo_logs/client_B.pid ]; then kill `cat demo_logs/client_B.pid` 2>/dev/null || true; fi
	-if [ -f demo_logs/server.pid ]; then kill `cat demo_logs/server.pid` 2>/dev/null || true; fi
	@echo "Stopped demo processes (if any)."
