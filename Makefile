# Makefile for Awale Game - Modular Architecture
# Compatible with POSIX systems (Linux, macOS, WSL)

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Werror -std=c11 -O2 -I./include
LDFLAGS := -pthread

# Directories
SRC_DIR := src
BUILD_DIR := build
INC_DIR := include
TEST_DIR := tests
DATA_DIR := data

# Source files
COMMON_SRC := $(wildcard $(SRC_DIR)/common/*.c)
GAME_SRC := $(wildcard $(SRC_DIR)/game/*.c)
NETWORK_SRC := $(wildcard $(SRC_DIR)/network/*.c)
SERVER_SRC := $(wildcard $(SRC_DIR)/server/*.c)
CLIENT_SRC := $(wildcard $(SRC_DIR)/client/*.c)

# Object files
COMMON_OBJ := $(COMMON_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
GAME_OBJ := $(GAME_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
NETWORK_OBJ := $(NETWORK_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
SERVER_OBJ := $(SERVER_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CLIENT_OBJ := $(CLIENT_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Shared objects (common to both client and server)
SHARED_OBJ := $(COMMON_OBJ) $(GAME_OBJ) $(NETWORK_OBJ)

# Binaries
SERVER_BIN := $(BUILD_DIR)/awale_server
CLIENT_BIN := $(BUILD_DIR)/awale_client

# Test binaries
TEST_BIN := $(BUILD_DIR)/test_game

# Phony targets
.PHONY: all clean server client test dirs help run-server run-client debug

# Default target
all: dirs server client

# Help message
help:
	@echo "Awale Game - Build System"
	@echo "=========================="
	@echo "Targets:"
	@echo "  all          - Build server and client (default)"
	@echo "  server       - Build server only"
	@echo "  client       - Build client only"
	@echo "  test         - Build and run all tests"
	@echo "  test-game    - Run game logic tests only"
	@echo "  test-network - Run network layer tests only"
	@echo "  clean        - Remove build artifacts"
	@echo "  dirs         - Create necessary directories"
	@echo "  run-server   - Build and run server on port 12345"
	@echo "  run-client   - Build and run client (specify PSEUDO=name)"
	@echo "  debug        - Build with debug symbols"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make server"
	@echo "  make run-server"
	@echo "  make run-client PSEUDO=Alice"

# Create directories
dirs:
	@mkdir -p $(BUILD_DIR)/common
	@mkdir -p $(BUILD_DIR)/game
	@mkdir -p $(BUILD_DIR)/network
	@mkdir -p $(BUILD_DIR)/server
	@mkdir -p $(BUILD_DIR)/client
	@mkdir -p $(DATA_DIR)

# Server binary
server: dirs $(SERVER_BIN)

$(SERVER_BIN): $(SHARED_OBJ) $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Server built successfully: $@"

# Client binary
client: dirs $(CLIENT_BIN)

$(CLIENT_BIN): $(SHARED_OBJ) $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Client built successfully: $@"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean all
	@echo "✓ Debug build complete"

# Test binaries
TEST_GAME_LOGIC := $(BUILD_DIR)/test_game_logic
TEST_NETWORK := $(BUILD_DIR)/test_network

# Test targets
test: test-game test-network
	@echo ""
	@echo "═══════════════════════════════════════════════════════"
	@echo "  ✓ All tests completed successfully!"
	@echo "═══════════════════════════════════════════════════════"

test-game: dirs $(TEST_GAME_LOGIC)
	@echo "Running game logic tests..."
	@$(TEST_GAME_LOGIC)

test-network: dirs $(TEST_NETWORK)
	@echo "Running network layer tests..."
	@$(TEST_NETWORK)

$(TEST_GAME_LOGIC): $(COMMON_OBJ) $(GAME_OBJ) tests/test_game_logic.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_NETWORK): $(COMMON_OBJ) $(NETWORK_OBJ) tests/test_network.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Run server (discovery port with UDP broadcast support)
DISCOVERY_PORT ?= 12345

run-server: server
	@echo "Starting Awale server..."
	@echo "  Discovery Port (TCP): $(DISCOVERY_PORT)"
	@echo "  Broadcast Port (UDP): 12346"
	@echo "Clients will discover server automatically via UDP broadcast."
	$(SERVER_BIN) $(DISCOVERY_PORT)

# Run client (use PSEUDO=name to set player name)
PSEUDO ?= Player1

run-client: client
	@echo "Connecting as $(PSEUDO)..."
	@echo "Server will be discovered automatically via UDP broadcast."
	$(CLIENT_BIN) $(PSEUDO)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(DATA_DIR)
	@echo "✓ Clean complete"

# Clean everything including data
clean-all: clean
	rm -rf $(DATA_DIR)
	@echo "✓ Full clean complete"

# Show variables (for debugging Makefile)
show-vars:
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "COMMON_SRC = $(COMMON_SRC)"
	@echo "GAME_SRC = $(GAME_SRC)"
	@echo "NETWORK_SRC = $(NETWORK_SRC)"
	@echo "SERVER_SRC = $(SERVER_SRC)"
	@echo "CLIENT_SRC = $(CLIENT_SRC)"
	@echo "SHARED_OBJ = $(SHARED_OBJ)"

# Dependency management (optional but recommended)
-include $(SHARED_OBJ:.o=.d)
-include $(SERVER_OBJ:.o=.d)
-include $(CLIENT_OBJ:.o=.d)

# Generate dependencies
$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -MM -MT $(BUILD_DIR)/$*.o $< -MF $@
