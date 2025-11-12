# Awale Game - Modular Client/Server Implementation

A modern, cross-platform implementation of Awale (Oware/Mancala) using C with a clean modular architecture and single-socket TCP communication.

## Features

- **Modular Architecture**: Clean separation between game logic, networking, and UI
- **Single-Socket Communication**: Simplified TCP connections (no port negotiation)
- **Automatic Server Discovery**: UDP broadcast for easy local network setup
- **Cross-Platform**: Works on Linux, macOS, and Windows (WSL)
- **Thread-Safe**: Concurrent game support with proper synchronization
- **Comprehensive Testing**: Automated unit tests for game logic and networking

## Architecture

### Core Modules
- **Common**: Shared types, protocol definitions, and message structures
- **Game**: Pure game logic (board operations, rules, player management)
- **Network**: Single-socket TCP connections and message serialization
- **Server**: Game management, matchmaking, and client handling
- **Client**: Text-based UI and command processing

### Network Design
- **Single TCP Socket**: Bidirectional communication on one connection
- **UDP Discovery**: Automatic server detection on local networks
- **Message Protocol**: Typed messages with fixed headers and variable payloads
- **No Port Negotiation**: Direct connection to server discovery port

## Quick Start

### Prerequisites
- C11 compiler (gcc/clang)
- POSIX environment (Linux, macOS, WSL)
- Make build system

### Build
```bash
make all          # Build both client and server
make server       # Build server only
make client       # Build client only
make test         # Run automated tests
make clean        # Clean build artifacts
```

### Run

**Terminal 1: Start Server**
```bash
make run-server          # Auto-discovers on port 12345
# OR
./build/awale_server    # Uses default port 12345
```

**Terminal 2: Start Client A**
```bash
make run-client PSEUDO=Alice    # Auto-discovers server via UDP
# OR
./build/awale_client Alice      # Auto-discovers server
# OR
./build/awale_client -s 192.168.1.100 Alice  # Direct IP connection
```

**Terminal 3: Start Client B**
```bash
./build/awale_client Bob
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

- **UDP 12346**: Broadcast discovery (client → server discovery)
- **TCP 12345**: Server discovery port (default, configurable)
- **Dynamic TCP**: Game connections (assigned by system)

## Testing

```bash
make test         # Run all tests (33 total)
make test-game    # Game logic tests only
make test-network # Network layer tests only
```

## Project Structure

```
├── include/           # Header files
│   ├── common/       # Shared definitions
│   ├── game/         # Game logic interfaces
│   ├── network/      # Network layer interfaces
│   ├── server/       # Server component interfaces
│   └── client/       # Client component interfaces
├── src/              # Implementation files
│   ├── common/       # Shared implementations
│   ├── game/         # Game logic
│   ├── network/      # Networking (single-socket)
│   ├── server/       # Server components
│   └── client/       # Client components
├── tests/            # Unit tests
├── build/            # Compiled binaries
└── docs/             # Documentation
```

## Game Rules

Awale follows traditional Mancala rules with the feeding rule:
- Players take turns sowing seeds from pits
- Captures occur when last seed lands in opponent's pit with 2-3 seeds
- **Feeding Rule**: Moves must leave opponent with playable seeds
- First to 25+ seeds or with all remaining seeds wins

## Connection Process

1. **UDP Discovery**: Client broadcasts "find server" request
2. **Server Response**: Server replies with IP and TCP port
3. **Direct Connect**: Client connects to server's TCP port
4. **Authentication**: Client sends MSG_CONNECT, server responds with session
5. **Gameplay**: Bidirectional communication over single TCP socket

## Troubleshooting

- **Connection Issues**: Check firewall settings, ensure UDP/TCP ports are open
- **Build Errors**: Ensure C11 compiler and POSIX headers are available
- **Windows Users**: Use WSL for native POSIX support
- **Port Conflicts**: Server will use default ports, change if needed

## Documentation

- `ARCHITECTURE.md` - Detailed module descriptions and data flows
- `RULES.md` - Complete Awale game rules
- `UDP_BROADCAST_DISCOVERY.md` - Network discovery implementation
- `USER_MANUAL.md` - User guide and examples

## Development

The codebase emphasizes:
- **Clean Interfaces**: Each module has well-defined responsibilities
- **Test Coverage**: Automated tests for critical functionality
- **Error Handling**: Comprehensive error codes and validation
- **Thread Safety**: Proper synchronization for concurrent operations

## License

This educational implementation is provided as-is for learning purposes. See individual files for any licensing information.