# Awale Game - User Manual

## Table of Contents
1. [Installation](#installation)
2. [Starting the Server](#starting-the-server)
3. [Connecting as a Client](#connecting-as-a-client)
4. [Game Features](#game-features)
5. [Client Commands](#client-commands)
6. [Game Rules](#game-rules)
7. [Troubleshooting](#troubleshooting)

## Installation

### Prerequisites
- Linux/WSL environment
- GCC compiler
- Make build tool
- Network connectivity (same local network for automatic discovery)

### Compilation
```bash
# Clone or extract the project
cd "Awale Game"

# Build everything
make clean
make

# Run tests (optional, recommended)
make test
```

This will create:
- `build/awale_server` - The game server
- `build/awale_client` - The game client

## Starting the Server

### Basic Usage
```bash
# Start server with default settings
./build/awale_server

# Or use Make
make run-server
```

### Custom Discovery Port
```bash
# Use a custom discovery port (default: 12345)
./build/awale_server 8080
```

### Server Output
```
╔══════════════════════════════════════════════════════╗
║         AWALE SERVER (New Architecture)              ║
╚══════════════════════════════════════════════════════╝
Discovery Port: 12345 (TCP)
Broadcast Port: 12346 (UDP)
Game manager initialized
Matchmaking initialized
UDP broadcast discovery listening on port 12346
Discovery server listening on port 12345

Server ready! Waiting for connections...
```

## Connecting as a Client

### Automatic Discovery (Recommended)
The client **automatically discovers servers** on the local network using UDP broadcast:

```bash
# Just provide your pseudo!
./build/awale_client Alice

# Or use Make
make run-client PSEUDO=Bob
```

### What Happens
1. Client broadcasts "find server" request on local network
2. Server responds with its IP and port
3. Client automatically connects
4. You can start playing!

### Client Output
```
Player: Alice
Broadcasting discovery request on local network...
Server discovered at 192.168.1.100:12345
Connected to server
   Client will listen on port: 54321
   Server listening on port: 54322
Bidirectional connection established

╔══════════════════════════════════════════════════════╗
║              AWALE GAME CLIENT                       ║
╚══════════════════════════════════════════════════════╝
Connected as: Alice

Available Commands:
  1. List connected players
  2. Challenge a player
  3. View pending challenges
  4. Play a move
  5. View game board
  6. Quit

Enter your choice:
```

## Game Features

### Implemented Features

#### Core Gameplay
- **Full Awale Rules**: Complete implementation with feeding rule, capture mechanics
- **Move Validation**: Server verifies all moves for legality
- **Game State**: Real-time board state with scores
- **Win Detection**: Automatic winner determination at 25+ seeds or starvation

#### Multiplayer
- **Multiple Simultaneous Games**: Server handles many games at once
- **Player Registration**: Each client registers with unique username
- **Challenge System**: Players challenge each other to start games
- **Mutual Challenge**: When both players challenge each other, game auto-starts

#### Network Features
- **Automatic Server Discovery**: UDP broadcast finds servers automatically
- **Bidirectional Communication**: Full-duplex sockets for real-time updates
- **Dynamic Port Allocation**: No port conflicts, each connection uses unique ports
- **Thread Safety**: Mutex-protected game state for concurrent access

#### ✅ Spectator Mode
- **View Any Game**: Watch ongoing games without playing
- **Real-time Board State**: Get current game state any time

## Client Commands

### 1. List Connected Players
Shows all currently connected players on the server.

**Example:**
```
Enter your choice: 1

Connected players (3):
─────────────────────────────
  1. Alice (192.168.1.100)
  2. Bob (192.168.1.101)
  3. Charlie (192.168.1.102)
─────────────────────────────
```

### 2. Challenge a Player
Send a challenge to another player to start a game.

**How it works:**
- You challenge them: `Alice challenges Bob`
- They challenge you back: `Bob challenges Alice`
- Game automatically starts!

**Example:**
```
Enter your choice: 2

⚔️  Challenge a player
Enter opponent's pseudo: Bob

Challenge sent to Bob! Waiting for them to challenge you back...
```

**When both challenge each other:**
```
🎮 GAME STARTED!
Game ID: Alice_Bob
Players: Alice vs Bob
You are: Player A
```

### 3. View Pending Challenges
See who has challenged you.

**Example:**
```
Enter your choice: 3

📨 Viewing pending challenges...

Pending challenges (2):
─────────────────────────────
  1. Charlie
  2. David
─────────────────────────────

💡 Challenge them back (option 2) to start a game!
```

### 4. Play a Move
Make a move in an ongoing game.

**Example:**
```
Enter your choice: 4

🎲 Play a move
Enter Player A pseudo: Alice
Enter Player B pseudo: Bob

Current Board:
╔═══════════════════════════════════════════════════════╗
║                    AWALE BOARD                        ║
╠═══════════════════════════════════════════════════════╣
║  Player A: Alice                 Score: 12            ║
║  Player B: Bob                   Score: 8             ║
╠═══════════════════════════════════════════════════════╣
║       11   10    9    8    7    6    (Player A)      ║
║      ┌───┬───┬───┬───┬───┬───┐                       ║
║      │ 4 │ 4 │ 4 │ 4 │ 4 │ 4 │                       ║
║      └───┴───┴───┴───┴───┴───┘                       ║
║      ┌───┬───┬───┬───┬───┬───┐                       ║
║      │ 4 │ 4 │ 4 │ 4 │ 4 │ 4 │                       ║
║      └───┴───┴───┴───┴───┴───┘                       ║
║        0    1    2    3    4    5    (Player B)      ║
╚═══════════════════════════════════════════════════════╝

Current turn: Player A (Alice)

Enter pit number (0-11): 7

Move successful!
  Seeds captured: 2
```

### 5. View Game Board (Spectator Mode)
Watch any ongoing game without playing.

**Example:**
```
Enter your choice: 5

👁️  View game board
Enter Player A pseudo: Alice
Enter Player B pseudo: Bob

[Board display appears here]
```

### 6. Quit
Disconnect from the server.

```
Enter your choice: 6
Goodbye!
```

## Game Rules

### Objective
Capture more than 24 seeds (out of 48 total) to win.

### Board Layout
```
    11  10   9   8   7   6    ← Player A's pits
   ┌──┬──┬──┬──┬──┬──┐
   │4 │4 │4 │4 │4 │4 │
   └──┴──┴──┴──┴──┴──┘
   ┌──┬──┬──┬──┬──┬──┐
   │4 │4 │4 │4 │4 │4 │
   └──┴──┴──┴──┴──┴──┘
     0   1   2   3   4   5    ← Player B's pits
```

### How to Play

#### 1. Taking a Turn
- Choose a pit from your side (contains seeds)
- Pick up all seeds from that pit
- Sow them one-by-one counter-clockwise
- Skip the origin pit if you lap the board

#### 2. Capturing Seeds
After sowing, capture from opponent's pits if:
- Landing pit is on opponent's side
- Landing pit now has 2 or 3 seeds
- Walk backwards capturing consecutive 2-or-3 pits

**Example:**
```
Before move:
Opponent:  3  2  1  4  2  1
You:       0  5  3  2  1  4

You play pit with 5 seeds → lands in opponent's pit with 1 → becomes 2
Capture that pit (2 seeds)!

Check previous pit: has 2 seeds → capture!
Check previous pit: has 3 seeds → capture!
Check previous pit: has 4 seeds → STOP (not 2 or 3)

Total captured: 2 + 2 + 3 = 7 seeds
```

#### 3. Feeding Rule (IMPORTANT!)
You **must** leave opponent with seeds if:
- They have no seeds
- You have a move that gives them seeds

**Violation:** If you starve them when you could feed them, **move is illegal**!

#### 4. Winning
- **Normal Win**: First to capture 25+ seeds
- **Starvation Win**: Opponent has no seeds and you can't feed them
- **Endless Loop**: Rare - remaining seeds split by side

### Move Validation
The server automatically checks:
- ✅ Pit is on your side (0-5 for Player B, 6-11 for Player A)
- ✅ Pit contains seeds (not empty)
- ✅ It's your turn
- ✅ Feeding rule compliance
- ✅ No invalid captures

## Troubleshooting

### "No server found on local network"
**Problem:** Client can't discover server via UDP broadcast.

**Solutions:**
1. Ensure server is running
2. Both must be on same local network (same subnet)
3. Check firewall allows UDP port 12346
4. Try custom discovery port if default is blocked

### "Failed to connect to server"
**Problem:** TCP connection failed after discovery.

**Solutions:**
1. Check firewall allows TCP discovery port (default 12345)
2. Verify server is running and not crashed
3. Check server logs for errors

### "Error sending challenge"
**Problem:** Challenge request failed.

**Solutions:**
1. Verify opponent's pseudo is correct (case-sensitive)
2. Check if opponent is still connected
3. Try listing players first (option 1)

### "Move is illegal"
**Problem:** Server rejected your move.

**Solutions:**
1. **Empty pit**: Choose a pit with seeds
2. **Wrong side**: Use pits 6-11 if Player A, 0-5 if Player B
3. **Not your turn**: Wait for opponent's move
4. **Feeding rule**: Must feed opponent if they're starving

### "Failed to accept connection from server"
**Problem:** Client-side port negotiation failed.

**Solutions:**
1. Check firewall allows ephemeral ports (32768-60999)
2. Ensure client has permission to bind to ports
3. Try restarting client

### Server Crashes
**Problem:** Server terminates unexpectedly.

**Solutions:**
1. Check terminal output for error messages
2. Verify compilation was clean (`make clean && make`)
3. Run tests to ensure stability (`make test`)
4. Report bugs with server output

## Advanced Usage

### Running Multiple Clients
Open multiple terminals and run different clients:

```bash
# Terminal 1: Server
make run-server

# Terminal 2: Client Alice
./build/awale_client Alice

# Terminal 3: Client Bob
./build/awale_client Bob

# Terminal 4: Spectator Charlie
./build/awale_client Charlie
```

Alice and Bob can play, Charlie can spectate!

### Custom Ports
If default ports conflict:

```bash
# Server with custom discovery port
./build/awale_server 9000

# Note: UDP broadcast port is fixed at 12346
```

### Debugging
Enable verbose output by checking server terminal for:
- Connection messages
- Challenge notifications
- Game start/end events
- Error messages

## Network Architecture

### Ports Used
- **UDP 12346**: Broadcast discovery (fixed)
- **TCP 12345**: Discovery port (configurable)
- **TCP Ephemeral**: Dynamic per-connection ports (auto-allocated)

### Connection Flow
1. Client broadcasts discovery request (UDP)
2. Server responds with IP + discovery port (UDP)
3. Client connects to discovery port (TCP)
4. Port negotiation: both exchange listening ports
5. Bidirectional connection established
6. Game communication begins!

## Performance & Limits

### Server Capacity
- **Max Players**: 100 concurrent connections
- **Max Games**: 100 simultaneous games
- **Max Challenges**: 100 pending challenges
- **Port Range**: ~28,000 available ephemeral ports

### Recommended Setup
- **LAN**: Best performance, <1ms latency
- **WiFi**: Good, <10ms latency
- **Same Subnet**: Required for UDP broadcast discovery

## Getting Help

### Check Status
```bash
# Server status
ps aux | grep awale_server

# Network connections
netstat -tuln | grep 12345
```

### Test Connectivity
```bash
# Test UDP broadcast port
nc -u -l 12346

# Test TCP discovery port
nc -l 12345
```

### Report Issues
Include in bug reports:
1. Server terminal output
2. Client terminal output
3. Steps to reproduce
4. Expected vs actual behavior

## Summary

**Starting a Game:**
1. Run server: `make run-server`
2. Connect clients: `./build/awale_client Alice`, `./build/awale_client Bob`
3. Both challenge each other (option 2)
4. Game auto-starts!
5. Play moves (option 4)

**Spectating:**
- Connect as any client
- Use option 5 to view any ongoing game
- No need to play yourself!

**Key Points:**
- Server discovery is automatic
- No IP addresses needed
- Challenge system is mutual (both must challenge)
- Feeding rule is strictly enforced
- Multiple games run simultaneously

Enjoy playing Awale! 🎮
