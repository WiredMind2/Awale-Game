# Awale Game - Architecture Diagrams

## 📐 System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         AWALE GAME SYSTEM                       │
└─────────────────────────────────────────────────────────────────┘

┌──────────────────┐                            ┌──────────────────┐
│                  │                            │                  │
│     CLIENT       │◄──────── Network ─────────►│     SERVER       │
│                  │        (TCP/IP)            │                  │
└──────────────────┘                            └──────────────────┘
        │                                                │
        │                                                │
   ┌────▼─────┐                                    ┌─────▼────┐
   │          │                                    │          │
   │    UI    │                                    │  Game    │
   │  Module  │                                    │ Manager  │
   │          │                                    │          │
   └────┬─────┘                                    └─────┬────┘
        │                                                │
   ┌────▼─────┐                                    ┌─────▼────┐
   │ Commands │                                    │  Match   │
   │  Handler │                                    │  Making  │
   └────┬─────┘                                    └─────┬────┘
        │                                                │
        └────────┬────────────────────────┬──────────────┘
                 │                        │
           ┌─────▼──────┐          ┌──────▼─────┐
           │  Network   │          │   Game     │
           │   Layer    │          │   Logic    │
           │            │          │            │
           ├────────────┤          ├────────────┤
           │ Session    │          │   Board    │
           │ Connection │          │   Rules    │
           │ Serialize  │          │   Player   │
           └────────────┘          └────────────┘
```

## 🔄 Module Dependencies

```
┌────────────────────────────────────────────────────────────┐
│                      DEPENDENCY LAYERS                     │
└────────────────────────────────────────────────────────────┘

Layer 0: Foundation
┌─────────────────────────────────────────────────────────┐
│  COMMON MODULE                                          │
│  ┌─────────┐  ┌──────────┐  ┌──────────┐                │
│  │  types  │  │ protocol │  │ messages │                │
│  └─────────┘  └──────────┘  └──────────┘                │
└─────────────────────────────────────────────────────────┘
           ▲                          ▲
           │                          │
           ├──────────────────────────┤
           │                          │

Layer 1: Core Logic & Network Primitives
┌─────────────────────┐      ┌─────────────────────┐
│   GAME MODULE       │      │   NETWORK MODULE    │
│  ┌─────────┐        │      │  ┌─────────────┐    │
│  │  board  │        │      │  │ connection  │    │
│  ├─────────┤        │      │  ├─────────────┤    │
│  │  rules  │        │      │  │   session   │    │
│  ├─────────┤        │      │  ├─────────────┤    │
│  │ player  │        │      │  │ serializer  │    │
│  └─────────┘        │      │  └─────────────┘    │
└─────────────────────┘      └─────────────────────┘
           ▲                          ▲
           │                          │
           ├──────────────────────────┤
           │                          │

Layer 2: Application Logic
┌─────────────────────┐      ┌─────────────────────┐
│  SERVER MODULE      │      │  CLIENT MODULE      │
│  ┌──────────────┐   │      │  ┌──────────────┐   │
│  │ game_manager │   │      │  │     main     │   │
│  ├──────────────┤   │      │  ├──────────────┤   │
│  │ matchmaking  │   │      │  │      ui      │   │
│  ├──────────────┤   │      │  ├──────────────┤   │
│  │    storage   │   │      │  │   commands   │   │
│  └──────────────┘   │      │  └──────────────┘   │
└─────────────────────┘      └─────────────────────┘
```

## 📨 Message Flow

### Connect & Authenticate
```
Client                                    Server
  │                                         │
  ├──── MSG_CONNECT ────────────────────►   │
  │     {pseudo, version}                   │
  │                                         ├─── Validate
  │                                         ├─── Add to registry
  │  ◄──── MSG_CONNECT_ACK ─────────────────┤
  │        {success, session_id}            │
  │                                         │
```

### List Players
```
Client                                    Server
  │                                         │
  ├──── MSG_LIST_PLAYERS ─────────────────► │
  │                                         ├─── Query registry
  │ ◄───── MSG_PLAYER_LIST ─────────────────┤
  │        {count, players[]}               │
  │                                         │
```

### Challenge & Match
```
Alice                  Server                  Bob
  │                      │                      │
  ├─ MSG_CHALLENGE ────► │                      │
  │  {challenger: A,     │                      │
  │   opponent: B}       ├─ Store challenge     │
  │                      │                      │
  │                      │ ◄─ MSG_CHALLENGE ────┤
  │                      │    {challenger: B,   │
  │                      │     opponent: A}     │
  │                      ├─ Detect mutual!      │
  │                      ├─ Create game         │
  │                      │                      │
  │ ◄─ MSG_GAME_STARTED ─┤─ MSG_GAME_STARTED ─► │
  │  {game_id,           │  {game_id,           │
  │   your_side: A}      │   your_side: B}      │
  │                      │                      │
```

### Play Move
```
Client                                    Server
  │                                         │
  ├──── MSG_PLAY_MOVE ───────────────────►  │
  │     {game_id, player, pit}              │
  │                                         ├─── Find game
  │                                         ├─── Validate move
  │                                         │    ├─ Is your turn?
  │                                         │    ├─ Valid pit?
  │                                         │    ├─ Feeding rule?
  │                                         │    └─ Execute
  │                                         ├─── Sow seeds
  │                                         ├─── Capture
  │                                         ├─── Check win
  │  ◄──── MSG_MOVE_RESULT ─────────────────┤
  │        {success, captured,              │
  │         game_over, winner}              │
  │                                         │
```

### Get Board State
```
Client                                    Server
  │                                         │
  ├──── MSG_GET_BOARD ───────────────────►  │
  │     {game_id}                           │
  │                                         ├─── Find game
  │                                         ├─── Copy board
  │  ◄──── MSG_BOARD_STATE ─────────────────┤
  │        {exists, pits[], scores[],       │
  │         current_player, state, winner}  │
  │                                         │
```

## 🎮 Game State Machine

```
                    ┌──────────────┐
                    │   WAITING    │
                    │  (No games)  │
                    └───────┬──────┘
                            │
                    Mutual Challenge
                            │
                            ▼
                    ┌──────────────┐
              ┌─────┤ IN_PROGRESS  │◄─────┐
              │     │              │      │
              │     └───────┬──────┘      │
              │             │             │
              │      Valid Move           │
              │             │          Board Not
              │             ▼           Game Over
              │        Execute Move       │
              │             │             │
              │        Sow Seeds          │
              │             │             │
              │        Capture?           │
              │             │             │
              │        Check Win          │
              │             │             │
              │             ├─────────────┘
              │             │
              │      Score >= 25 OR
              │      Both Sides Empty
              │             │
              │             ▼
              │     ┌──────────────┐
              │     │   FINISHED   │
              │     │              │
              │     └───────┬──────┘
              │             │
              │       Determine
              │        Winner
              │             │
              │             ▼
              │     ┌──────────────┐
              └────►│  WINNER_A    │
                    │  WINNER_B    │
                    │    DRAW      │
                    └──────────────┘
```

## 🔒 Thread Safety Model

```
┌─────────────────────────────────────────────────────────┐
│              GAME MANAGER (Thread-Safe)                 │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Global Lock: manager->lock                             │
│    └─ Protects: game_count, games[] array               │
│                                                         │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────┐     │
│  │   Game 1     │   │   Game 2     │   │  Game N  │     │
│  ├──────────────┤   ├──────────────┤   ├──────────┤     │  
│  │ Lock: game   │   │ Lock: game   │   │Lock: game│     │
│  │  -> lock     │   │  -> lock     │   │ -> lock  │     │
│  ├──────────────┤   ├──────────────┤   ├──────────┤     │
│  │ Alice vs Bob │   │ Carol vs Dan │   │ ...      │     │
│  │              │   │              │   │          │     │
│  │ Board State  │   │ Board State  │   │Board St. │     │ 
│  └──────────────┘   └──────────────┘   └──────────┘     │
│                                                         │
└─────────────────────────────────────────────────────────┘

Thread 1 (Alice's move)      Thread 2 (Bob's move)
      │                             │
      ├─ Lock game->lock            ├─ Lock game->lock
      │                             │  (blocks, waits)
      ├─ Execute move               │
      ├─ Update board               │
      ├─ Unlock game->lock          │
      │                             ├─ (acquires lock)
      │                             ├─ Execute move
      │                             ├─ Unlock
      ▼                             ▼

= No race conditions, concurrent games work!
```

## 📦 Build Process Flow

```
Source Files                Compilation              Binaries
────────────               ─────────────            ─────────

src/common/*.c ─┐
src/game/*.c   ─┤
src/network/*.c─┼──► [gcc -c] ──► build/**/*.o ──┐
                │                                │
src/server/*.c ─┤                                ├─► [gcc -o] ──► build/awale_server
                │                                │
                │                                │
src/client/*.c ─┘                                └─► [gcc -o] ──► build/awale_client

Include Files
─────────────
include/common/*.h ─┐
include/game/*.h   ─┤
include/network/*.h─┼──► [-I./include]
include/server/*.h ─┘
```

---

## 🎯 Key Design Patterns Used

1. **Layered Architecture**: Clear separation between layers
2. **Module Pattern**: Each module has .h and .c files
3. **Factory Pattern**: board_init(), game_manager_create_game()
4. **Command Pattern**: Message types as commands
5. **State Machine**: Game states and transitions
6. **Lock-per-object**: Individual game locks for concurrency
7. **Error Code Pattern**: Consistent error handling
8. **Serialization Pattern**: Binary protocol with headers

---

**Legends:**
- `├──►` : Depends on / Uses
- `◄───` : Returns / Provides
- `─┬─` : Branches / Multiple paths
- `◄►` : Bidirectional communication
