# Awale Game - New Architecture Documentation

## 🎯 Overview

This document describes the new modular architecture for the Awale (Oware/Mancala) game, designed to replace the monolithic client/server implementation with a clean, maintainable, and extensible codebase.

## 🏗️ Architecture Principles

### **Separation of Concerns**
The codebase is organized into distinct modules:
- **Common**: Shared types, protocol definitions, and messages
- **Game**: Pure game logic (board, rules, players)
- **Network**: Connection handling and serialization
- **Server**: Game management and matchmaking
- **Client**: User interface and command handling

### **Layer Independence**
- Game logic is completely independent of network code
- Network layer doesn't know about game rules
- Clear interfaces between all layers

### **Thread Safety**
- Mutex-protected shared resources
- Lock-per-game design for concurrent gameplay
- No global mutable state

## 📁 Directory Structure

```
awale-game/
├── include/                    # Header files
│   ├── common/                # Shared definitions
│   │   ├── types.h           # Basic types and constants
│   │   ├── protocol.h        # Network protocol
│   │   └── messages.h        # Message structures
│   ├── game/                 # Game logic
│   │   ├── board.h           # Board operations
│   │   ├── rules.h           # Game rules
│   │   └── player.h          # Player management
│   ├── network/              # Network layer
│   │   ├── connection.h      # TCP connections
│   │   ├── session.h         # Session handling
│   │   └── serialization.h   # Message serialization
│   └── server/               # Server components
│       ├── game_manager.h    # Multi-game management
│       ├── matchmaking.h     # Challenge system
│       └── storage.h         # Persistence
│
├── src/                       # Implementation files
│   ├── common/               # (mirrors include/common)
│   ├── game/                 # (mirrors include/game)
│   ├── network/              # (mirrors include/network)
│   ├── server/               # Server implementation
│   │   ├── main.c           # Server entry point
│   │   ├── game_manager.c   # Game management
│   │   └── matchmaking.c    # Matchmaking logic
│   └── client/               # Client implementation
│       ├── main.c           # Client entry point
│       ├── ui.c             # User interface
│       └── commands.c       # Command handlers
│
├── tests/                     # Unit tests
├── build/                     # Build artifacts
├── data/                      # Runtime data
├── Makefile.new              # New build system
└── ARCHITECTURE.md           # This file
```

## 🔌 Module Descriptions

### **Common Module** (`include/common/`, `src/common/`)

#### `types.h` / `types.c`
Defines fundamental types used throughout the system:
- **Error codes**: `SUCCESS`, `ERR_INVALID_MOVE`, etc.
- **Player IDs**: `PLAYER_A`, `PLAYER_B`
- **Game states**: `GAME_STATE_IN_PROGRESS`, `GAME_STATE_FINISHED`
- **Winner types**: `WINNER_A`, `WINNER_B`, `DRAW`, `NO_WINNER`
- **Constants**: `MAX_PSEUDO_LEN`, `NUM_PITS`, `WIN_SCORE`

#### `protocol.h` / `protocol.c`
Network protocol definition:
- **Message types**: `MSG_CONNECT`, `MSG_PLAY_MOVE`, `MSG_BOARD_STATE`, etc.
- **Message header**: Fixed-size header with type, length, sequence number
- **Protocol version**: Versioning for compatibility

#### `messages.h`
Payload structures for each message type:
- `msg_connect_t`: Connection request
- `msg_play_move_t`: Move command
- `msg_board_state_t`: Board state response
- `msg_challenge_t`: Challenge request
- And many more...

### **Game Module** (`include/game/`, `src/game/`)

#### `board.h` / `board.c`
Board state and operations:
```c
typedef struct {
    int pits[12];              // 12 pits
    int scores[2];             // Scores for each player
    player_id_t current_player;
    game_state_t state;
    winner_t winner;
} board_t;

// Key functions
error_code_t board_init(board_t* board);
error_code_t board_execute_move(board_t* board, player_id_t player, 
                                int pit_index, int* seeds_captured);
bool board_is_game_over(const board_t* board);
winner_t board_get_winner(const board_t* board);
```

#### `rules.h` / `rules.c`
Game rules validation and enforcement:
```c
// Move validation
error_code_t rules_validate_move(const board_t* board, player_id_t player, int pit_index);

// Feeding rule enforcement
bool rules_would_starve_opponent(const board_t* board, player_id_t player, int pit_index);
bool rules_has_feeding_alternative(const board_t* board, player_id_t player, int pit_index);

// Move mechanics
int rules_sow_seeds(board_t* board, int start_pit, int seeds, bool skip_origin);
int rules_capture_seeds(board_t* board, int last_pit, player_id_t player);
```

**Key Features:**
- ✅ Proper pit ownership checking
- ✅ Feeding rule (don't starve opponent if alternative exists)
- ✅ Capture logic (2-3 seeds in opponent pits)
- ✅ Win condition checking (25+ seeds or both sides empty)
- ✅ Move simulation for validation

#### `player.h` / `player.c`
Player information and statistics:
```c
typedef struct {
    player_info_t info;        // Pseudo and IP
    bool connected;
    time_t connect_time;
    int games_played;
    int games_won;
} player_t;
```

### **Network Module** (`include/network/`, `src/network/`)

#### `connection.h` / `connection.c`
Low-level TCP connection handling:
```c
typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    bool connected;
    uint32_t sequence;
} connection_t;

error_code_t connection_create_server(connection_t* conn, int port);
error_code_t connection_connect(connection_t* conn, const char* host, int port);
error_code_t connection_send_raw(connection_t* conn, const void* data, size_t size);
```

#### `serialization.h` / `serialization.c`
Message serialization/deserialization:
```c
// Binary protocol with network byte order
error_code_t serialize_int32(serialize_buffer_t* buffer, int32_t value);
error_code_t serialize_string(serialize_buffer_t* buffer, const char* str, size_t max_len);
error_code_t serialize_message(message_type_t type, const void* payload, ...);

// Message-specific serializers
error_code_t serialize_board_state(serialize_buffer_t* buffer, const msg_board_state_t* msg);
error_code_t serialize_move(serialize_buffer_t* buffer, const msg_play_move_t* msg);
```

**Protocol Format:**
```
[Header: 16 bytes]
  - type: 4 bytes (uint32_t)
  - length: 4 bytes (uint32_t)
  - sequence: 4 bytes (uint32_t)
  - reserved: 4 bytes (uint32_t)
[Payload: variable length, up to MAX_PAYLOAD_SIZE]
```

### **Server Module** (`include/server/`, `src/server/`)

#### `game_manager.h` / `game_manager.c`
Manages multiple concurrent games:
```c
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char player_a[MAX_PSEUDO_LEN];
    char player_b[MAX_PSEUDO_LEN];
    board_t board;
    bool active;
    pthread_mutex_t lock;        // Per-game lock
} game_instance_t;

typedef struct {
    game_instance_t games[MAX_GAMES];
    int game_count;
    pthread_mutex_t lock;        // Manager lock
} game_manager_t;

// Thread-safe operations
error_code_t game_manager_create_game(game_manager_t* manager, ...);
error_code_t game_manager_play_move(game_manager_t* manager, ...);
```

**Features:**
- ✅ Thread-safe game creation and access
- ✅ Lock-per-game for concurrent gameplay
- ✅ Game ID generation and lookup
- ✅ Player-based game search

#### `matchmaking.h` / `matchmaking.c`
Challenge system and player registry:
```c
typedef struct {
    char challenger[MAX_PSEUDO_LEN];
    char opponent[MAX_PSEUDO_LEN];
    time_t created_at;
    bool active;
} challenge_t;

typedef struct {
    challenge_t challenges[MAX_CHALLENGES];
    player_entry_t players[MAX_PLAYERS];
    pthread_mutex_t lock;
} matchmaking_t;

// Mutual challenge detection
error_code_t matchmaking_create_challenge(matchmaking_t* mm, 
                                         const char* challenger,
                                         const char* opponent,
                                         bool* mutual_found);
```

**Features:**
- ✅ Player registry
- ✅ Challenge tracking
- ✅ Mutual challenge detection (auto-start games)
- ✅ Challenge expiration

## 🔄 Protocol Flow

### **Connection Sequence**
```
Client                          Server
  |                               |
  |------- MSG_CONNECT --------->|
  |    (pseudo, version)          |
  |                               |
  |<----- MSG_CONNECT_ACK -------|
  |    (success, session_id)      |
  |                               |
```

### **Challenge & Game Start**
```
Alice                Server                Bob
  |                     |                    |
  |-- MSG_CHALLENGE --->|                    |
  | (challenger: Alice, |                    |
  |  opponent: Bob)     |                    |
  |                     |<-- MSG_CHALLENGE --|
  |                     | (challenger: Bob,  |
  |                     |  opponent: Alice)  |
  |                     |                    |
  |<- MSG_GAME_STARTED -|-> MSG_GAME_STARTED-|
  | (game_id, you=A)    |  (game_id, you=B) |
  |                     |                    |
```

### **Playing Moves**
```
Client                          Server
  |                               |
  |------ MSG_PLAY_MOVE -------->|
  | (game_id, player, pit)        |
  |                               | [Validate move]
  |                               | [Execute move]
  |                               | [Check win condition]
  |<---- MSG_MOVE_RESULT --------|
  | (success, seeds_captured,     |
  |  game_over, winner)           |
  |                               |
```

## 🛠️ Building

### **Using the New Makefile**
```bash
# Build everything
make -f Makefile.new

# Build server only
make -f Makefile.new server

# Build client only
make -f Makefile.new client

# Clean
make -f Makefile.new clean

# Help
make -f Makefile.new help
```

### **Build Targets**
- `all`: Build server and client
- `server`: Build server only
- `client`: Build client only
- `clean`: Remove build artifacts
- `debug`: Build with debug symbols
- `run-server`: Build and run server
- `run-client PSEUDO=name`: Build and run client

## 🧪 Testing

### **Unit Tests** (To be implemented)
```bash
make -f Makefile.new test
```

Test coverage should include:
- ✅ Board operations (init, copy, queries)
- ✅ Rules validation (all error cases)
- ✅ Sowing mechanics
- ✅ Capture logic
- ✅ Feeding rule enforcement
- ✅ Serialization/deserialization
- ✅ Game manager operations
- ✅ Matchmaking logic

## 📈 Improvements Over Original

| Aspect | Original | New Architecture |
|--------|----------|------------------|
| **Structure** | Monolithic (2 files) | Modular (20+ files) |
| **Concurrency** | `fork()` + `mmap` | `pthread` + mutex |
| **Protocol** | Raw structs | Typed messages + serialization |
| **Testability** | Difficult | Easy (unit testable) |
| **Error Handling** | Minimal | Comprehensive error codes |
| **Code Reuse** | Low | High |
| **Maintainability** | Low | High |
| **Extensibility** | Difficult | Easy |

## 🔮 Future Enhancements

### **Planned Features**
1. **Persistence**: Save/load games to disk
2. **Replay System**: Record and replay games
3. **AI Player**: Computer opponent
4. **Web Interface**: HTTP/WebSocket support
5. **Statistics**: Player rankings and history
6. **Spectator Mode**: Watch ongoing games
7. **Tournament Mode**: Bracket-style tournaments

### **Technical Improvements**
1. **JSON Protocol**: Replace binary with JSON for debugging
2. **Event System**: Pub/sub for game events
3. **Connection Pooling**: Reusable connections
4. **Graceful Shutdown**: Clean resource cleanup
5. **Logging System**: Structured logging
6. **Configuration**: Config file support

## 📝 Migration Guide

### **From Original to New**

**Original Code:**
```c
// Monolithic, mixed concerns
handle_play_command(int scomm, const char* pseudo) {
    struct move player_move;
    read(scomm, &player_move, sizeof(player_move));
    // Game logic mixed with network code
    ...
}
```

**New Architecture:**
```c
// Separated concerns
// 1. Network layer receives message
session_recv_message(&session, &type, &payload, ...);

// 2. Deserialize
msg_play_move_t move;
deserialize_move(&buffer, &move);

// 3. Game logic (independent)
game_manager_play_move(&g_game_manager, move.game_id, 
                       move.player, move.pit_index, &captured);

// 4. Send response
msg_move_result_t result;
session_send_move_result(&session, &result);
```

## 🤝 Contributing

When adding new features:
1. Follow the modular structure
2. Keep game logic separate from network code
3. Add unit tests
4. Update this documentation
5. Use the defined error codes
6. Maintain thread safety

## 📚 References

- [Oware Rules](../RULES.md)
- [Original Implementation](../awale_server.c)
- [Build System](../Makefile.new)

---

**Status**: ✅ Core architecture implemented  
**Next Steps**: Complete server/client main implementations, add tests, add persistence
