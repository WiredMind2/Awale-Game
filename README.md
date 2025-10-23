# Awale Game (Client / Server)

Simple TCP client/server implementation of the Awale (Mancala) game used for a systems / networks practical.

This repository contains two small C programs:

- `awale_server.c` — a TCP server that tracks connected players, challenges and game boards using POSIX APIs (fork, mmap, sockets).
- `awale_client.c` — a TCP client that connects to the server and provides a small text UI to list players, challenge, play moves, and display the board.

## Design / contract

- Server listens on a TCP port and accepts multiple clients (each handled with `fork`).
- Communication is done with simple command enums and raw structs (`read` / `write`).
- Board state and client lists are stored in shared memory (anonymous `mmap`).

Data shapes (high-level):
- Commands: CMD_LISTER_JOUEURS, CMD_DEFIER, CMD_JOUER, CMD_GET_BOARD, CMD_QUITTER
- `struct move` — playerA, playerB, pit_index
- `struct board_state` — pits[12], score[2], current_player, pseudoA, pseudoB, game_exists

## Prerequisites

- A POSIX-like environment (Linux, macOS). The code uses `fork`, `mmap`, and POSIX sockets.
- A C compiler (gcc/clang).

Notes for Windows users
- The source uses POSIX-only APIs. On native Windows (cmd / PowerShell) it will not compile/run without porting.
- Recommended: use WSL (Windows Subsystem for Linux) or an MSYS2/Cygwin environment that provides POSIX compatibility.

## Build

Open a terminal in the repository root (bash / WSL / macOS / Linux). Then:

```bash
gcc -o awale_server awale_server.c
gcc -o awale_client awale_client.c
```

If you use WSL from PowerShell, open WSL and cd to the mounted path, for example:

```powershell
# Open WSL, then in WSL:
cd "/mnt/c/Users/<you>/Documents/INSA/PR/TP1-20250930/Awale Game"
gcc -o awale_server awale_server.c
gcc -o awale_client awale_client.c
```

Or start an Ubuntu/WSL shell and run the same `gcc` commands there.

## Run / usage

1. Start the server (choose a port > 1024):

```bash
./awale_server 12345
```

2. Start one or more clients in separate terminals:

```bash
./awale_client 127.0.0.1 12345 Alice
./awale_client 127.0.0.1 12345 Bob
```

Client usage (menu):
- 1: List connected players
- 2: Challenge another player (enter opponent pseudo)
- 3: Play a move (enter playerA, playerB, then pit 0-11)
- 4: Show board for a given pair
- 5: Quit

Example

- In terminal A: ./awale_server 12345
- In terminal B: ./awale_client 127.0.0.1 12345 Alice
- In terminal C: ./awale_client 127.0.0.1 12345 Bob
- Alice selects option 2 and challenges `Bob`. If Bob also challenges Alice, the server starts a game automatically.

## Limitations & notes

- This is a small educational implementation. It is not hardened for production use.
- The server uses anonymous `mmap` and `fork` for inter-process sharing; the approach is Unix-centric.
- The protocol is very simple and fragile: it sends raw struct bytes and expects exact sizes. Both client and server must be built with compatible architectures and compilers.

## Troubleshooting

- "bind" or "socket" errors: make sure the port is free and you have permission to bind (use >1024 if not root).
- On Windows, prefer running inside WSL or MSYS2; native compilation will likely fail because of missing `fork` / `mmap`.

## Files

- `awale_server.c` — server source
- `awale_client.c` — client source

## License

This repository does not include an explicit license. If you plan to reuse or redistribute the code, add a license file (e.g. `LICENSE`) or ask the original author for permission.

---

If you want, I can also:

- add a simple Makefile or PowerShell/WSL build script,
- add a small test script to run a server and two clients automatically in WSL terminals,
- or update the code to be more portable to Windows (remove fork/mmap and use threads + named pipes / TCP-only shared state).

Tell me which you'd like next.