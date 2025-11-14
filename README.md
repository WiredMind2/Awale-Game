# Awale Game - Modular Client/Server Implementation

Awale is a modular C implementation of the Awale (Oware/Mancala) game. The
project separates pure game logic from networking and server/client code so
the game can be unit-tested independently from the network layer.

## Features

- **Modular Architecture**: Clean separation between game logic, networking, and UI
- **Single-Socket Communication**: Simplified TCP connections (discovery + single TCP connection)
- **Automatic Server Discovery**: UDP broadcast for easy local network setup
- **Cross-Platform**: Works on Linux, macOS, and Windows (WSL)
- **Thread-Safe**: Concurrent game support with proper synchronization
- **Comprehensive Testing**: Automated unit tests for game logic and networking
- **Internationalization**: Support for English and French languages

## Language Support

The client interface supports both English and French languages. To switch languages:

1. Open `include/client/client_logging.h`
2. Change the line `#define CURRENT_LANGUAGE LANGUAGE_EN` to `#define CURRENT_LANGUAGE LANGUAGE_FR` for French
3. Rebuild the project: `make clean && make client`
4. Run the client as usual

Note: The language setting is compile-time and affects all client UI strings.

## Architecture

### Core Modules
- **Common**: Shared types, protocol definitions, and message structures
- **Game**: Pure game logic (board operations, rules, player management)
- **Network**: Single-socket TCP connections and message serialization
- **Server**: Game management, matchmaking, and client handling
- **Client**: Text-based UI and command processing

### Network Design
- **UDP Discovery (broadcast)**: Clients broadcast a discovery packet on UDP port
	`12346` and servers respond with their TCP discovery port and IP.
- **TCP Discovery Port**: Servers listen on a configurable TCP discovery port
	(default `12345`) for incoming client connections; that TCP connection is
	used for all bidirectional communication.
- **Single TCP Socket**: After discovery the client connects to the server's
	TCP discovery port and uses that single socket for messaging (reads + writes).
- **Message Protocol**: Typed messages with a fixed 16-byte header and a
	variable-length payload (network byte order used for integers).

## Quick Start

### Prerequisites
- C11 compiler (gcc/clang)
- POSIX environment (Linux, macOS, WSL)
- Make build system

### Build
```bash
make          # Build both client and server
make clean        # Clean build artifacts
```

### Run

Start the server (discovery port default: `12345`) and clients which
auto-discover the server via UDP broadcast:

Server:
```bash
make server
./build/awale_server     # default discovery port 12345 (configurable)
```

Client (auto-discover):
```bash
make client
./build/awale_client Alice      # client broadcasts discovery over UDP:12346
```

Client (direct connect to IP):
```bash
./build/awale_client -s 192.168.1.100 Alice
```

### Gameplay
- Use the text menu to list players, send challenges, and play moves
- Games are automatically managed by the server
- Multiple games can run concurrently

## Client Commands

1. **List Players** - See all connected players
2. **Challenge Player** - Send a game challenge
3. **View Challenges** - Check incoming challenges
4. **Play Mode** - Enter active gameplay
5. **Spectator Mode** - Watch ongoing games
6. **Disconnect** - Exit the client

## Network Ports

- **UDP 12346**: Fixed broadcast discovery port used by clients to find
	servers on the LAN (server listens and responds on this UDP port).
- **TCP 12345**: Default TCP discovery port where servers accept client
	connections after discovery (configurable via server command-line).
- **Ephemeral TCP ports**: The implementation uses a single TCP connection for
	gameplay; additional ephemeral ports are not required by default.

## Testing

The repository includes unit and integration tests. Use the included Makefile
targets to build and run tests:

```bash
make test          # Run integration and unit tests
make test-game     # Game logic unit tests only
make test-network  # Network layer unit tests only
```

## Project Structure

```
├── include/           # Header files (public interfaces)
│   ├── common/        # Shared protocol/types/messages
│   ├── game/          # Game logic interfaces (board, rules, player)
│   ├── network/       # Network interfaces (connection, serialization)
│   ├── server/        # Server interfaces (game manager, matchmaking)
│   └── client/        # Client interfaces (UI, commands)
├── src/               # Implementation files
│   ├── common/        # Shared implementations
│   ├── game/          # Game logic (pure, no networking)
│   ├── network/       # Networking (discovery + TCP messaging)
│   ├── server/        # Server implementation and main
│   └── client/        # Client implementation and main
├── tests/             # Unit and integration tests
├── build/             # Compiled binaries (output)
├── data/              # Runtime data used by server
└── docs/              # Additional documentation
```

## Game Rules

Awale follows traditional Mancala rules with the feeding rule:
- Players take turns sowing seeds from pits
- Captures occur when last seed lands in opponent's pit with 2-3 seeds
- **Feeding Rule**: Moves must leave opponent with playable seeds
- First to 25+ seeds or with all remaining seeds wins

## Connection Process

1. **UDP Discovery**: Client broadcasts "AWALE_DISCOVERY" on UDP `12346`.
2. **Server Response**: Server replies with `AWALE_SERVER:<port>` where
	`<port>` is the TCP discovery port (default `12345`).
3. **TCP Connect**: Client connects to the server's TCP discovery port.
4. **Handshake**: Client sends `MSG_CONNECT` (pseudo and protocol version);
	server replies with `MSG_CONNECT_ACK` and session information.
5. **Gameplay**: Clients and server exchange typed messages over the single
	TCP connection (message header: 16 bytes: type, length, sequence, reserved).

## Development

The codebase emphasizes:

- **Clean Interfaces**: Game logic is independent of networking and UI.
- **Test Coverage**: Unit tests for game logic and network serialization.
- **Error Handling**: Functions return `error_code_t` values defined in
	`include/common/types.h`.
- **Thread Safety**: Server uses per-game mutexes to allow concurrent games.

## AI assistance

Some parts of the implementation, refactors, tests and documentation were produced 
with the help of AI-assisted developer tools.For exemple, the autocompletion tools were really useful, and we used AI for generating comments. The interface was made more coherent and polished. This also helped us find some bugs. During development we used VS Code
GitHub Copilot and the Kilo Code extension mostly in agent mode, with three models: 
GPT-4o mini, Grok 3, and Claude 4.5 Sonnet.