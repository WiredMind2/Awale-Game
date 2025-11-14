# Awale Game - User Action Sequences

This document outlines the step-by-step sequences for performing the most common actions in the Awale game application. Each section describes a specific functionality with clear, numbered steps.

## Table of Contents
1. [Server Startup](#server-startup)
2. [Client Connection](#client-connection)
3. [Main Menu Actions](#main-menu-actions)
4. [Play Mode](#play-mode)
5. [Spectator Mode](#spectator-mode)
6. [Chat Functionality](#chat-functionality)
7. [Profile Management Submenu](#profile-management-submenu)
8. [Friend Management](#friend-management)
9. [Interactive Tutorial](#interactive-tutorial)
10. [List Saved Games for Review](#list-saved-games-for-review)
11. [View Saved Game](#view-saved-game)

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

### Profile Management
1. From main menu, enter `4`
2. Choose from submenu:
   - 1: Set Your Bio
   - 2: View Player Bio
   - 3: View Player Statistics
3. Follow prompts for selected option

### Enter Play Mode
1. From main menu, enter `5`
2. If you have active games, select which one to play (or it auto-selects if only one)
3. Enter play mode interface for making moves

### Start Chat
1. From main menu, enter `6`
2. Choose recipient: enter "all" for global chat or a username for private chat
3. Type messages to send (one per line)
4. Type "exit" or "quit" to return to main menu

### Enter Spectator Mode
1. From main menu, enter `7`
2. System displays list of available games to spectate (requires friend relationship)
3. Select a game by number
4. Enter spectator interface for watching the game

### Friend Management
1. From main menu, enter `8`
2. Choose from submenu:
   - 1: Add Friend
   - 2: Remove Friend
   - 3: List Friends
3. Follow prompts for selected option

### List Saved Games for Review
1. From main menu, enter `9`
2. System displays list of your saved games
3. Select a game to view details or review

### View Saved Game
1. From main menu, enter `10`
2. Enter game ID or select from list
3. View the saved game replay or details

### Quit Application
1. From main menu, enter `11`
2. Client disconnects from server
3. Application terminates

### Interactive Tutorial
1. From main menu, enter `12`
2. Follow the step-by-step tutorial on how to play Awale
3. Learn rules, moves, and strategies

## Play Mode

### Single Active Game
1. Enter play mode (option 5 from main menu)
2. System automatically selects your active game
3. Current board state is displayed
4. If it's your turn:
    - Enter pit number (0-5 for Player A bottom row, 6-11 for Player B top row)
    - Move is validated and sent to server
5. If it's opponent's turn, wait for their move
6. Board updates automatically after each move

### Multiple Active Games
1. Enter play mode (option 5 from main menu)
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
1. Enter spectator mode (option 7 from main menu)
2. System requests and displays list of active games (only games involving friends)
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
1. Enter chat mode (option 6 from main menu)
2. Type "all" when prompted for recipient
3. System confirms global chat mode
4. Type your message and press Enter to send
5. Messages are broadcast to all connected players

### Starting Private Chat
1. Enter chat mode (option 6 from main menu)
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

## Profile Management Submenu

### Setting Your Bio
1. From main menu, enter `4` for Profile Management
2. Choose `1: Set Your Bio` from submenu
3. Enter your bio text when prompted (up to 255 characters)
4. Bio is saved and can be viewed by other players

### Viewing Player Bio
1. From main menu, enter `4` for Profile Management
2. Choose `2: View Player Bio` from submenu
3. Enter the username of the player whose bio you want to view
4. System displays the player's bio if it exists

### Viewing Player Statistics
1. From main menu, enter `4` for Profile Management
2. Choose `3: View Player Statistics` from submenu
3. Enter the username of the player whose stats you want to view
4. System displays games played, wins, losses, and win rate

## Friend Management

### Adding a Friend
1. From main menu, enter `8` for Friend Management
2. Choose `1: Add Friend` from submenu
3. Enter the username of the player to add as friend
4. Friend request is sent if the player exists

### Removing a Friend
1. From main menu, enter `8` for Friend Management
2. Choose `2: Remove Friend` from submenu
3. Enter the username of the friend to remove
4. Friend is removed from your list

### Listing Friends
1. From main menu, enter `8` for Friend Management
2. Choose `3: List Friends` from submenu
3. System displays your current friends list

## Interactive Tutorial

### Starting the Tutorial
1. From main menu, enter `12` for Interactive Tutorial
2. System begins the step-by-step tutorial
3. Follow on-screen instructions to learn Awale rules

### Navigating the Tutorial
1. Tutorial presents rules, board setup, and basic moves
2. Interactive examples allow you to try moves
3. Progress through sections at your own pace
4. Exit tutorial at any time to return to main menu

## List Saved Games for Review

### Accessing Saved Games List
1. From main menu, enter `9` for List Saved Games for Review
2. System displays list of your saved games
3. Each entry shows game ID, opponents, date, and outcome

### Selecting a Game for Review
1. Enter the number of the game you want to review
2. System shows detailed game information
3. Option to proceed to full replay or return to list

## View Saved Game

### Viewing a Saved Game Replay
1. From main menu, enter `10` for View Saved Game
2. Enter game ID or select from recent games
3. System loads and displays the game replay
4. Navigate through moves using controls

### Game Replay Controls
1. Use arrow keys or commands to step through moves
2. View board state at any point in the game
3. See move history and final scores
4. Exit replay to return to main menu

## Background Notification Processing

### Notification Listener Thread
1. Client starts notification listener thread upon connection
2. Thread continuously monitors for server notifications
3. Notifications processed in background without user interaction
4. Supported notification types:
   - Challenge received
   - Game started
   - Move results
   - Game over
   - Chat messages
   - Spectator updates

### Notification Display
1. Notifications appear immediately in any mode
2. Real-time updates for game state changes
3. Automatic board refreshes in spectator mode
4. Challenge notifications with interactive prompts

## Error Recovery and Retry Sequences

### Network Timeout Recovery
1. Operation times out (e.g., board request, message send)
2. Client displays timeout warning
3. Automatic retry with exponential backoff
4. Maximum retry attempts before error
5. User notified of retry status

### Connection Loss Handling
1. Network connection lost during session
2. Client displays connection lost message
3. Automatic reconnection attempts (if implemented)
4. Session state preserved where possible
5. Graceful degradation to offline mode

### Board Request Retry Logic
1. Board request fails or times out
2. Client retries request up to 3 times
3. Displays retry attempt messages
4. Falls back to manual refresh option
5. Preserves game continuity

## Spectator Mode Updates

### Automatic Updates (Current Implementation)
1. Enter spectator mode and select game
2. Initial board displayed
3. Background notifications trigger automatic board refreshes
4. Board updates in real-time when moves are made
5. Manual refresh ('r') still available as fallback

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
### Optimized Profile Management

#### Enhanced Profile Features
1. Rich profile pages with:
   - Customizable avatars or profile pictures
   - Extended bio with formatting (bold, italic, links)
   - Achievement badges based on game performance
   - Favorite strategies or playing style description

#### Quick Profile Access
1. Press 'p' from main menu for instant profile view/edit
2. Inline editing: click on bio to edit directly
3. Auto-save drafts to prevent data loss
4. Profile preview before publishing changes

#### Integrated Statistics Dashboard
1. Comprehensive stats view with:
   - Win/loss ratio with trend graphs
   - Average game duration
   - Most played opponents
   - Peak performance periods
2. Export stats to share with friends

#### Privacy and Social Features
1. Profile visibility settings:
   - Public: visible to all players
   - Friends only: restricted to friend list
   - Private: stats only, no bio
2. Friend recommendations based on similar play styles
3. Profile comments and ratings from friends

#### Error Handling and Validation
1. Real-time bio length counter (e.g., "150/255 characters")
2. Content moderation warnings for inappropriate text
3. Graceful handling of missing profiles: "Player has not set up their profile yet"
4. Backup and restore profile data

### Optimized Friend Management

#### Streamlined Friend Requests
1. Quick-add from player list: hover over username and click "Add Friend"
2. Friend request notifications with one-click accept/decline
3. Mutual friend suggestions based on common connections
4. Friend request history and pending requests dashboard

#### Enhanced Friend List Features
1. Friend status indicators:
   - Online/In-Game/Away/Offline
   - Current activity (playing vs Alice, spectating game #123)
2. Sort and filter options:
   - Online first
   - Recently active
   - Alphabetical
   - By win rate
3. Bulk actions: message all online friends, invite to group game

#### Social Integration
1. Friend-only games and tournaments
2. Automatic spectator access to friends' games
3. Shared game history and statistics with friends
4. Friend activity feed: see when friends start games or achieve milestones

#### Privacy and Communication
1. Friend groups or categories (Close Friends, Casual Players)
2. Selective sharing: choose which stats to share with friends
3. Friend chat shortcuts: /friend message to send to all friends
4. Block/unblock players with automatic friend removal

#### Error Handling
1. Clear feedback for failed friend requests: "Player not found" vs "Player has friend requests disabled"
2. Duplicate request prevention
3. Automatic cleanup of inactive friends (optional)
4. Recovery options for accidentally removed friends

### Optimized Interactive Tutorial

#### Adaptive Learning Path
1. Skill assessment quiz at start to customize tutorial difficulty
2. Dynamic content based on user progress and mistakes
3. Skip sections for experienced players
4. Remedial lessons for common mistakes

#### Interactive Board Experience
1. Click-to-play board with visual feedback
2. Move animations showing seed distribution
3. Undo/redo for experimentation
4. Hint system with progressive reveal (show legal moves, then suggest best move)

#### Multimedia Learning Aids
1. Video demonstrations of complex strategies
2. Audio explanations with voice-over
3. Interactive quizzes after each section
4. Practice mode with AI opponent at various difficulty levels

#### Progress Tracking and Gamification
1. Tutorial completion badges and achievements
2. Progress bar with estimated time remaining
3. Leaderboard for fastest tutorial completion
4. Unlock advanced content after basic tutorial

#### Integration with Main Game
1. Seamless transition: practice moves carry over to real games
2. Tutorial accessible during games for rule reminders
3. Link to community resources and strategy guides
4. Personalized tips based on playing style observed in tutorial

#### Error Handling and Support
1. Gentle correction for wrong moves with explanations
2. Multiple attempt allowance before showing solution
3. Help button linking to relevant documentation
4. Save progress across sessions

### Optimized List Saved Games

#### Advanced Filtering and Search
1. Search by opponent name, game ID, or date range
2. Filter options:
   - Game outcome (wins/losses/draws)
   - Opponent skill level
   - Game duration
   - Specific strategies used (if tagged)
3. Saved filter presets for quick access

#### Enhanced Game List Display
1. Thumbnail previews of final board positions
2. Key statistics at a glance: moves played, duration, Elo change
3. Color coding: wins green, losses red, draws yellow
4. Sort by multiple criteria: date, opponent, game length, Elo gained/lost

#### Bulk Operations
1. Select multiple games for batch actions:
   - Delete old games
   - Export game data
   - Share with friends
   - Analyze for patterns
2. Quick actions per game: view, replay, delete, favorite

#### Integration with Analytics
1. Game history dashboard with trends over time
2. Performance against specific opponents
3. Improvement tracking: Elo progression, win rate changes
4. Export data for external analysis tools

#### Usability Improvements
1. Infinite scroll or pagination for large game lists
2. Keyboard navigation and shortcuts
3. Mobile-responsive design
4. One-click access to replay from list

#### Error Handling
1. Graceful handling of corrupted game files
2. Clear messages for missing games: "Game data not available"
3. Backup and restore functionality for game history
4. Validation of game integrity before display

### Optimized View Saved Game

#### Advanced Replay Controls
1. Playback speed control: 0.5x to 4x speed
2. Frame-by-frame stepping with arrow keys
3. Jump to specific move numbers or timestamps
4. Pause and resume at any point

#### Analysis Mode
1. Move evaluation with AI suggestions for better alternatives
2. Position analysis showing winning probabilities
3. Highlight critical moves and turning points
4. Comment system for personal notes on moves

#### Interactive Features
1. Branching replays: try alternative moves from any position
2. Side-by-side comparison with similar games
3. Export replay as video or GIF
4. Share replay link with friends for review

#### Enhanced Visualization
1. Animated seed movements with trails
2. Heat maps showing most contested pits
3. Timeline view of board state evolution
4. Player thinking time indicators

#### Integration and Sharing
1. Link replays to strategy discussions
2. Embed in tutorials or lessons
3. Compare with live games in spectator mode
4. Save favorite positions for study

#### Error Handling and Performance
1. Smooth playback even for long games
2. Automatic quality adjustment for performance
3. Error recovery for corrupted replay data
4. Offline viewing capability


These optimizations would transform the user experience from functional but clunky to modern and intuitive, while maintaining the core Awale gameplay mechanics.