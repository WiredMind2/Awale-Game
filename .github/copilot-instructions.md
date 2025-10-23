# Awale Game - AI Agent Instructions

## Project Overview
A client-server implementation of Awale (Oware/Mancala) using C with TCP/UDP sockets, pthreads, and select() multiplexing. The project uses a **fully modular architecture** with automatic UDP broadcast server discovery and peer-to-peer port negotiation for bidirectional connections.

## Critical Architecture Points

### Fully Modular Architecture
- **Legacy files removed**: All old monolithic files (`awale_server.c`, `awale_client.c`, old Makefiles, redundant docs) deleted
- **New architecture only**: Modular design in `src/` and `include/` directories with 30+ files
- All new code goes in modular structure under `src/`/`include/`
- **Clean project root**: Only essential documentation (README.md, RULES.md, ARCHITECTURE.md, USER_MANUAL.md, objectives.txt)

### Module Structure (New Architecture)
```
src/common/     → Protocol, types, messages (shared by client/server)
src/game/       → Pure game logic (board, rules, player) - NO network code
src/network/    → TCP connections, serialization, sessions
src/server/     → Game manager, matchmaking, server main loop
src/client/     → Client UI and command handlers
```

**Golden Rule**: Game logic (`src/game/`) NEVER imports network code. Network layer (`src/network/`) NEVER imports game logic. They communicate only via `src/common/` types.

### Network Communication Evolution

#### UDP Broadcast Discovery
Automatic server discovery eliminates manual IP configuration:
- Client broadcasts "AWALE_DISCOVERY" on UDP port 12346 (255.255.255.255)
- Server responds "AWALE_SERVER:<port>" with IP and TCP discovery port
- Client extracts server IP and connects automatically
- See `UDP_BROADCAST_DISCOVERY.md` for complete flow diagrams

**Key Functions:**
```c
// Client: Find server on local network (5 second timeout)
error_code_t connection_broadcast_discovery(discovery_response_t* response);

// Server: Listen for discovery requests (infinite loop, run in thread)
void connection_listen_for_discovery(uint16_t discovery_port);
```

#### Peer-to-Peer Port Negotiation
Symmetric design where each side allocates its own listening port:
- Client finds free port using `connection_find_free_port()`
- Server finds free port using `connection_find_free_port()`
- Exchange ports via `MSG_PORT_NEGOTIATION` (payload: `{int32_t my_port}`)
- Both create listening socket on own port AND connect to peer's port
- Result: Full-duplex bidirectional connection

#### Bidirectional Communication System
The codebase uses **dual-socket connections** with `select()` multiplexing:
- `connection_t` has TWO file descriptors: `read_sockfd` and `write_sockfd`
- All connection functions now use bidirectional sockets by default
- See `BIDIRECTIONAL_USAGE.md` for full API and examples

**Key Pattern**:
```c
// Server setup (two ports: read_port, write_port)
connection_create_server(&server, 2000, 2001);

// Client connects to both
connection_connect(&client, "host", 2000, 2001);

// Use select() to multiplex
select_context_t ctx;
select_context_init(&ctx, timeout_sec, timeout_usec);
select_context_add_read(&ctx, conn.read_sockfd);
select_wait(&ctx, &ready_count);
if (select_context_is_readable(&ctx, conn.read_sockfd)) { /* handle */ }
```

### Message Protocol (Typed, Not Raw Structs)
Messages have fixed **16-byte header** + variable payload:
```c
message_header_t {
    uint32_t type;      // message_type_t enum (network byte order)
    uint32_t length;    // Payload size in bytes
    uint32_t sequence;  // For ordering/dedup
    uint32_t reserved;
}
```

**Critical Bug Pattern**: When sending `msg_player_list_t`, do NOT send `sizeof(struct)` as it includes the full 100-player array. Calculate actual size:
```c
size_t actual_size = sizeof(list.count) + (count * sizeof(player_info_t));
session_send_message(session, MSG_PLAYER_LIST, &list, actual_size);
```

### Thread Safety Pattern
- Each game instance has its own `pthread_mutex_t` in `game_instance_t`
- Matchmaking uses a single global `pthread_mutex_t` for the registry
- Always lock before modifying shared state, unlock immediately after
- Pattern: `pthread_mutex_lock(&game->lock)` → modify → `pthread_mutex_unlock(&game->lock)`

### Awale Rules Implementation (src/game/rules.c)
The **feeding rule** is the most complex:
- Player MUST leave opponent with seeds if alternative move exists
- Check: `rules_would_starve_opponent()` then `rules_has_feeding_alternative()`
- If starve + has alternative → move is `ERR_STARVE_VIOLATION`
- Capture chain: Walk backwards from landing pit, capture consecutive 2-or-3 seed pits in opponent row

**Game End Conditions**:
1. Player reaches 25+ seeds (immediate win)
2. Player has no seeds on their side and opponent can't feed them
3. Endless loop (rare, remaining seeds split by side)

### Build System Commands

### Primary Build
```bash
make              # Builds server + client in build/
make server       # Build server only → build/awale_server
make client       # Build client only → build/awale_client
make clean        # Remove all build artifacts
make run-server   # Build and run server (discovery port 12345, broadcast port 12346)
make run-client PSEUDO=Alice  # Build and run client (auto-discovers server via UDP broadcast)
make test         # Run all automated tests (33 tests, preferred over manual testing)
```

**Important Network Ports:**
- **UDP 12346**: Broadcast discovery port (fixed, hardcoded)
- **TCP 12345**: Discovery/connection port (default, configurable via command-line)
- **TCP Ephemeral**: Dynamic bidirectional ports negotiated per connection



### WSL Context
This project is developed in WSL (Windows Subsystem for Linux):
- Workspace path: `/mnt/c/Users/willi/Documents/INSA/PR/TP1-20250930/Awale Game/`
- Uses POSIX APIs: pthread, select(), TCP sockets
- Will NOT compile on native Windows without porting

### AI Agent Limitations
- **Cannot manage multiple terminal sessions**: Use automated tests (`make test`) instead of manual multi-terminal testing
- **NEVER run server/client in shell for debugging**: Running the applications in terminal to see logs is extremely inefficient and unreliable
  - Instead: Ask the user to run the application and provide the log output
  - Pattern: "Please run `./build/awale_server` and paste the output here" or "Please run `./build/awale_client William` and share what you see"
  - This is MUCH faster than trying to coordinate background processes, capture output, or manage multiple terminals
- **Avoid interactive prompts**: Tests should be non-interactive and return exit codes
- **Prefer build system**: Use `make` targets rather than raw `gcc` commands
- **For connection/network debugging**: Always ask user to provide both server AND client logs together to see the full picture

## Code Conventions

### Error Handling Pattern
Every function returns `error_code_t` (enum from `types.h`):
```c
error_code_t foo(param_t* param) {
    if (!param) return ERR_INVALID_PARAM;
    // ... do work
    return SUCCESS;
}

// Caller checks:
error_code_t err = foo(&param);
if (err != SUCCESS) {
    // Handle error - often send MSG_ERROR to client
}
```

### Naming Conventions
- Functions: `module_action_target()` → `board_execute_move()`, `session_send_message()`
- Structs: `thing_t` suffix → `connection_t`, `board_t`, `select_context_t`
- Enums: SCREAMING_SNAKE → `MSG_CONNECT`, `PLAYER_A`, `ERR_INVALID_MOVE`
- Message structs: `msg_<type>_t` → `msg_play_move_t`, `msg_board_state_t`

### Memory Management
- No malloc/free in game logic (stack-allocated structs)
- Server uses malloc for `game_instance_t` (long-lived, variable count)
- Always check NULL on pointers before dereferencing
- No memory leaks: every malloc has matching free in cleanup path

## Testing & Debugging

### Automated Testing (PREFERRED)
**Always use automated tests instead of manual command-line testing.** AI agents cannot effectively manage multiple terminal sessions simultaneously.

```bash
# Run all tests
make test

# Run specific test suites
make test-game      # Game logic tests only
make test-network   # Network layer tests only
```

Test files are in `tests/`:
- `test_game_logic.c` - Board operations, rules validation, feeding rule, capture logic
- `test_network.c` - Message serialization, connection management, protocol handling

### Manual Testing (Avoid if Possible)
**⚠️ Note**: Manual testing requires multiple terminals and is difficult for AI agents to coordinate. Use automated tests instead.

If manual testing is absolutely necessary:
```bash
# Terminal 1: Start server (discovery_port optional, default 12345)
./build/awale_server

# Terminal 2: Start client A (auto-discovers server via UDP broadcast)
./build/awale_client Alice

# Terminal 3: Start client B
./build/awale_client Bob
```

**Port Negotiation Process:**
1. Client broadcasts discovery request on UDP port 12346
2. Server responds with IP and discovery port
3. Client connects to TCP discovery port
4. Each side allocates own listening port via `connection_find_free_port()`
5. Exchange ports via MSG_PORT_NEGOTIATION
6. Both create listening socket on own port AND connect to peer's port

### Common Issues
1. **"list players doesn't work"** → Check message size calculation (see Message Protocol section)
2. **Binding error** → Port already in use, try different port or kill old process
3. **Compilation errors** → Missing include path: always use `-I./include` flag
4. **Segfault on move** → Likely mutex not locked or out-of-bounds pit index (0-11 valid)

## Key Files to Understand First
1. `ARCHITECTURE.md` - Full module descriptions, data flows, protocol details
2. `RULES.md` - Official Awale game rules (feeding rule is critical)
3. `include/common/types.h` - Error codes, constants, enums used everywhere
4. `include/common/messages.h` - All message payload structures
5. `src/game/rules.c` - Core game logic with feeding rule enforcement
6. `BIDIRECTIONAL_USAGE.md` - New dual-socket communication system
7. `UDP_BROADCAST_DISCOVERY.md` - Automatic server discovery system



## Future Work (In Progress)
- Chat system (send messages during/outside games)
- Player bio (10 ASCII lines per player)
- Private mode / friends list for spectators
- Persistent storage (saving game state to disk)
- Save and replay games functionality
- ELO rating system
- Reconnection support (client crash recovery)
- Tournament mode with rankings

---

When making changes, always maintain module boundaries, use typed error returns, and calculate exact message sizes. The architecture is designed for testability - write unit tests for game logic without needing network code.
