# Awale Game - User Action Sequences

This document outlines the step-by-step sequences for performing the most common actions in the Awale game application. Each section describes a specific functionality with clear, numbered steps.

## Table of Contents
1. [Server Startup](#server-startup)
2. [Client Connection](#client-connection)
3. [Main Menu Actions](#main-menu-actions)
4. [Play Mode](#play-mode)
5. [Spectator Mode](#spectator-mode)
6. [Chat Functionality](#chat-functionality)

## Server Startup

### Basic Server Startup
1. Open a terminal in the project directory
2. Run `make run-server` to build and start the server with default settings
3. Server will display startup banner and begin listening on default port 12345
4. Server is ready when you see "Server ready! Waiting for connections..."

### Custom Port Server Startup
1. Open a terminal in the project directory
2. Run `./build/awale_server 8080` (replace 8080 with desired port)
3. Server will display startup banner with custom port
4. Server begins listening on the specified port

## Client Connection

### Automatic Server Discovery (Recommended)
1. Open a new terminal in the project directory
2. Run `make run-client PSEUDO=YourName` (replace YourName with your chosen username)
3. Client broadcasts discovery request on local network
4. Client automatically connects when server is found
5. Connection established message appears
6. Main menu is displayed

### Manual IP Connection
1. Open a new terminal in the project directory
2. Run `./build/awale_client -s 192.168.1.100 YourName` (replace IP with server address)
3. Client attempts direct connection to specified IP
4. Connection established message appears
5. Main menu is displayed

## Main Menu Actions

### List Connected Players
1. From main menu, enter `1`
2. System displays all currently connected players with their IP addresses
3. Press Enter to return to main menu

### Challenge a Player
1. From main menu, enter `2`
2. Enter opponent's username when prompted
3. Challenge is sent to the specified player
4. Wait for opponent to accept challenge (game starts automatically when both have challenged each other)
5. If opponent challenges you back, game begins immediately

### View Pending Challenges
1. From main menu, enter `3`
2. System displays any incoming challenges from other players
3. Note: Challenges are mutual - respond by challenging them back (option 2)

### Set Your Bio
1. From main menu, enter `4`
2. Enter your bio text when prompted (up to 255 characters)
3. Bio is saved and can be viewed by other players

### View Player Bio
1. From main menu, enter `5`
2. Enter the username of the player whose bio you want to view
3. System displays the player's bio if it exists

### View Player Statistics
1. From main menu, enter `6`
2. Enter the username of the player whose stats you want to view
3. System displays games played, wins, losses, and win rate

### Enter Play Mode
1. From main menu, enter `8`
2. If you have active games, select which one to play (or it auto-selects if only one)
3. Enter play mode interface for making moves

### Start Chat
1. From main menu, enter `9`
2. Choose recipient: enter "all" for global chat or a username for private chat
3. Type messages to send (one per line)
4. Type "exit" or "quit" to return to main menu

### Enter Spectator Mode
1. From main menu, enter `10`
2. System displays list of available games to spectate
3. Select a game by number
4. Enter spectator interface for watching the game

### Quit Application
1. From main menu, enter `7`
2. Client disconnects from server
3. Application terminates

## Play Mode

### Single Active Game
1. Enter play mode (option 8 from main menu)
2. System automatically selects your active game
3. Current board state is displayed
4. If it's your turn:
   - Enter pit number (0-5 for Player A bottom row, 6-11 for Player B top row)
   - Move is validated and sent to server
5. If it's opponent's turn, wait for their move
6. Board updates automatically after each move

### Multiple Active Games
1. Enter play mode (option 8 from main menu)
2. System displays list of your active games
3. Enter the number of the game you want to play
4. Continue with single game sequence above

### Making a Move
1. When it's your turn, system shows legal moves
2. Enter the pit number you want to play (displayed as legal moves)
3. System validates the move (checks ownership, seeds available, feeding rule)
4. If valid, move is sent and board updates
5. If invalid, error message appears and you can try again

### Game Completion
1. When game ends, final scores are displayed
2. System asks if you want to challenge opponent again
3. Enter 'y' to send new challenge, 'n' to return to main menu

### Exiting Play Mode
1. Type 'm' + Enter at any time during play mode
2. Return to main menu

## Spectator Mode

### Selecting a Game to Spectate
1. Enter spectator mode (option 10 from main menu)
2. System requests and displays list of active games
3. Each game shows players, game ID, spectator count, and status
4. Enter the number of the game you want to watch
5. System sends spectate request to server

### Watching a Game
1. Initial board state is displayed
2. Score and current player information is shown
3. Available commands are displayed:
   - 'r' to manually refresh the board
   - 'q' to stop spectating

### Automatic Updates
1. Board automatically refreshes when moves are made
2. Notification appears when board updates
3. New board state is displayed immediately

### Manual Refresh
1. Type 'r' + Enter to manually refresh board
2. System requests current board state from server
3. Updated board is displayed

### Stopping Spectation
1. Type 'q' + Enter to stop watching
2. System sends stop spectating message to server
3. Return to main menu

## Chat Functionality

### Starting Global Chat
1. Enter chat mode (option 9 from main menu)
2. Type "all" when prompted for recipient
3. System confirms global chat mode
4. Type your message and press Enter to send
5. Messages are broadcast to all connected players

### Starting Private Chat
1. Enter chat mode (option 9 from main menu)
2. Type the recipient's username when prompted
3. System confirms private chat mode
4. Type your message and press Enter to send
5. Message is sent only to the specified player

### Receiving Messages
1. Chat messages appear automatically in any mode
2. Global messages show sender and message
3. Private messages show sender and indicate private message
4. Messages include timestamp

### Exiting Chat Mode
1. Type "exit" or "quit" and press Enter
2. Return to main menu
3. Chat mode terminates

## Common Error Scenarios

### No Server Found
- **Symptom**: "No server found on local network"
- **Solutions**:
  1. Ensure server is running
  2. Check that client and server are on same network
  3. Verify firewall allows UDP port 12346
  4. Try manual IP connection instead

### Connection Failed
- **Symptom**: "Failed to connect to server"
- **Solutions**:
  1. Check server is running and not crashed
  2. Verify firewall allows TCP port (default 12345)
  3. Check server logs for errors
  4. Try different port if server uses custom port

### Move Rejected
- **Symptom**: "Move is illegal"
- **Common causes**:
  1. Wrong pit (not your side of board)
  2. Empty pit selected
  3. Not your turn
  4. Feeding rule violation (starving opponent when you could feed them)

### Challenge Failed
- **Symptom**: "Error sending challenge"
- **Solutions**:
  1. Verify opponent username is spelled correctly
  2. Check that opponent is still connected
## Optimized User Sequences

This section outlines proposed optimizations to improve user comfort and experience. These changes would require code modifications but would significantly enhance usability.

### Optimized Challenge System

#### Standard Challenge Flow
1. From main menu, enter `2` to challenge a player
2. Enter opponent's username when prompted
3. Challenge notification is sent to the opponent
4. Opponent receives real-time notification with options to:
   - Accept challenge (game starts immediately)
   - Decline challenge (you receive notification)
   - Challenge times out after 60 seconds if no response
5. Game begins automatically upon acceptance

#### Receiving Challenges
1. Incoming challenge appears as notification in any mode
2. Quick accept/decline options available:
   - Press 'y' to accept immediately
   - Press 'n' to decline
   - Press 'l' to view challenge details later
3. Accepted challenges start game instantly

### Optimized Play Mode

#### Smart Game Selection
1. Enter play mode (option 8 from main menu)
2. System automatically prioritizes:
   - Game where it's your turn (highest priority)
   - Game with recent activity
   - Most recently started game
3. If multiple games need attention, dashboard shows:
   - Game status (your turn/opponent's turn)
   - Time since last move
   - Quick switch options (press game number)

#### Enhanced Move Input
1. Board displays with intuitive labeling:
   ```
   Your pits (A1-A6): 4 4 4 4 4 4
   Opponent pits (B1-B6): 4 4 4 4 4 4
   ```
2. Legal moves highlighted with arrows (→)
3. Enter move using friendly notation:
   - Type "A3" or "a3" for your third pit
   - Type "B2" or "b2" for opponent's second pit (if legal)
4. Move preview shows expected outcome before confirmation

#### Multi-Game Dashboard
1. Press 'd' from play mode to view all games
2. Dashboard shows:
   - Game ID and opponents
   - Current status (your turn/waiting)
   - Last move timestamp
   - Quick actions: switch to game, view board, resign
3. Keyboard shortcuts for instant switching (F1-F9 for games 1-9)

### Optimized Spectator Mode

#### Real-Time Spectating
1. Enter spectator mode (option 10 from main menu)
2. Games list shows live status and spectator counts
3. Select game to watch - joins automatically
4. Live updates stream automatically:
   - Move-by-move updates without manual refresh
   - Player action notifications ("Alice played A4")
   - Game event announcements ("Game started!", "Game finished!")

#### Enhanced Spectator Features
1. Spectator chat: communicate with other spectators
2. Multiple game following: watch up to 3 games simultaneously
3. Game statistics: view move history, average move time
4. Follow players: get notified when followed players start new games

### Optimized Chat System

#### Persistent Chat Interface
1. Chat available as sidebar or overlay in all modes
2. Quick commands:
   - `/global message` or `/g message` for global chat
   - `/private username message` or `/p username message` for private
   - `/exit` or `/quit` to hide chat
   - `/help` for chat commands

#### Enhanced Chat Features
1. Message history persists across sessions
2. Timestamps and read receipts
3. Emoji support and text formatting
4. Chat notifications with sound alerts (optional)
5. Auto-complete for usernames and commands

### Optimized Client Connection

#### Smart Connection Management
1. Run `./build/awale_client YourName`
2. Client automatically:
   - Scans for servers on local network
   - Shows list of available servers with friendly names
   - Connects to last used server if available
   - Falls back to manual IP input if auto-discovery fails

#### Connection Recovery
1. If connection lost, client automatically attempts reconnection
2. Shows clear status: "Reconnecting..." with progress
3. Preserves game state and chat history
4. Graceful degradation: offline mode for viewing local stats

### Optimized Error Handling

#### Contextual Error Messages
Instead of generic "Move is illegal", provide specific guidance:

- **Wrong pit selected**: "You can only play pits A1-A6 (your side). Try A3 instead."
- **Feeding rule violation**: "This move would starve your opponent. Legal alternatives: A1, A4, A5"
- **Not your turn**: "Waiting for Alice to move. Press 'r' to refresh board."
- **Connection issues**: "Server unreachable. Trying to reconnect... (attempt 2/5)"

#### Progressive Help System
1. First error: Specific suggestion
2. Repeated errors: Link to help section
3. Critical errors: Step-by-step troubleshooting wizard
4. Prevention: Highlight potential issues before they occur

### Optimized User Interface

#### Visual Enhancements
- Color-coded board with clear pit ownership
- Animated move previews
- Progress bars for long operations
- Status icons for players (online, in-game, away)

#### Keyboard Shortcuts
- F1-F9: Quick game switching
- Ctrl+C: Copy board state
- Ctrl+V: Paste move notation
- Tab: Auto-complete usernames
- Arrow keys: Navigate menus

#### Accessibility Features
- High contrast mode
- Screen reader support
- Keyboard-only navigation
- Customizable font sizes and colors

These optimizations would transform the user experience from functional but clunky to modern and intuitive, while maintaining the core Awale gameplay mechanics.
  3. Use option 1 to list players first