/* Client Play Mode - Implementation
 * Event-driven state machine for interactive play mode
 * 
 * Architecture:
 * - Single event loop using select() on stdin + notification pipe
 * - State machine transitions based on server responses
 * - Always drain user input to prevent buffering issues
 * - Process all user input before handling server events
 * - Server is authoritative - client adjusts to server state
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_play_mode.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_ui.h"
#include "../../include/common/messages.h"
#include "../../include/common/protocol.h"
#include "../../include/network/session.h"
#include "../../include/game/board.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <ctype.h>

/* Play mode states */
typedef enum {
    STATE_INIT,              /* Initial state - request board */
    STATE_IDLE,              /* Board displayed, waiting for events */
    STATE_WAITING_BOARD,     /* Requested board, waiting for response */
    STATE_MOVE_SENT,         /* Move sent, waiting for result */
    STATE_GAME_OVER,         /* Game finished, handle rematch */
    STATE_EXIT               /* User wants to leave */
} play_state_t;

/* Event types */
typedef enum {
    EVENT_NONE = 0,
    EVENT_USER_INPUT,        /* User typed something on stdin */
    EVENT_NOTIFICATION,      /* Server notification received */
    EVENT_TIMEOUT            /* Select timeout (for debugging) */
} event_type_t;

/* Helper: Poll for events from user or server */
static event_type_t poll_events(int notification_fd, int timeout_ms) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    int max_fd = STDIN_FILENO;
    if (notification_fd != -1) {
        FD_SET(notification_fd, &readfds);
        if (notification_fd > max_fd) {
            max_fd = notification_fd;
        }
    }
    
    struct timeval tv;
    struct timeval* tv_ptr = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }
    
    int ready = select(max_fd + 1, &readfds, NULL, NULL, tv_ptr);
    
    if (ready < 0) {
        perror("select error");
        return EVENT_NONE;
    }
    
    if (ready == 0) {
        return EVENT_TIMEOUT;
    }
    
    /* Priority: Process ALL user input before handling server notifications */
    if (FD_ISSET(STDIN_FILENO, &readfds)) {
        return EVENT_USER_INPUT;
    }
    
    if (notification_fd != -1 && FD_ISSET(notification_fd, &readfds)) {
        return EVENT_NOTIFICATION;
    }
    
    return EVENT_NONE;
}

/* Helper: Request board state from server */
static error_code_t request_board(const char* player_a, const char* player_b) {
    msg_get_board_t board_req;
    memset(&board_req, 0, sizeof(board_req));
    snprintf(board_req.player_a, MAX_PSEUDO_LEN, "%s", player_a);
    snprintf(board_req.player_b, MAX_PSEUDO_LEN, "%s", player_b);
    
    error_code_t err = session_send_message(client_state_get_session(), MSG_GET_BOARD, &board_req, sizeof(board_req));
    
    if (err == ERR_NETWORK_ERROR) {
        printf("❌ Failed to send board request - connection lost\n");
    } else if (err == ERR_TIMEOUT) {
        printf("⚠️  Network slow - request timeout\n");
    } else if (err != SUCCESS) {
        printf("❌ Error sending board request: %s\n", error_to_string(err));
    }
    
    return err;
}

/* Helper: Receive board state from server with retry logic */
static error_code_t receive_board(msg_board_state_t* board) {
    message_type_t type;
    size_t size;
    int retries = 0;
    const int MAX_RETRIES = 2;
    
    while (retries <= MAX_RETRIES) {
        error_code_t err = session_recv_message_timeout(client_state_get_session(), &type, board, 
                                                         sizeof(*board), &size, 5000);
        
        if (err == ERR_TIMEOUT) {
            retries++;
            if (retries <= MAX_RETRIES) {
                printf("⚠️  Server response timeout (attempt %d/%d) - retrying...\n", retries, MAX_RETRIES + 1);
                continue;
            }
            printf("❌ Server not responding after %d attempts\n", MAX_RETRIES + 1);
            return err;
        }
        
        if (err == ERR_NETWORK_ERROR) {
            printf("❌ Connection lost - please restart client\n");
            return err;
        }
        
        if (err != SUCCESS) {
            printf("❌ Error receiving board: %s\n", error_to_string(err));
            return err;
        }
        
        if (type != MSG_BOARD_STATE) {
            /* Unexpected message type - might be out of order */
            retries++;
            if (retries <= MAX_RETRIES) {
                printf("⚠️  Unexpected message type %d (expected %d) - retrying...\n", type, MSG_BOARD_STATE);
                /* Request board again */
                return ERR_UNKNOWN;
            }
            printf("❌ Protocol error - unexpected message type\n");
            return ERR_UNKNOWN;
        }
        
        /* Success */
        return SUCCESS;
    }
    
    return ERR_TIMEOUT;
}

/* Helper: Display board and current state */
static void display_board(const msg_board_state_t* board, player_id_t my_side) {
    print_board(board);
    
    if (board->current_player == my_side) {
        int start_pit = (my_side == PLAYER_A) ? 0 : 6;
        int end_pit = (my_side == PLAYER_A) ? 5 : 11;
        
        printf("\n🎯 YOUR TURN!\n");
        printf("   Legal moves: ");
        
        bool has_legal_move = false;
        for (int i = start_pit; i <= end_pit; i++) {
            if (board->pits[i] > 0) {
                printf("%d ", i);
                has_legal_move = true;
            }
        }
        
        if (!has_legal_move) {
            printf("(none)");
        }
        printf("\n");
        printf("\nEnter pit number or 'm' for menu: ");
    } else {
        printf("\n⏳ Waiting for opponent...\n");
        printf("   Type 'm' + Enter to return to main menu\n");
        printf("> ");
    }
    fflush(stdout);
}

/* Helper: Handle user input based on current state */
static void handle_user_input(play_state_t* state, const msg_board_state_t* board, 
                              player_id_t my_side, const char* game_id, 
                              char input[32], bool* should_request_board) {
    (void)should_request_board;  /* Reserved for future use */
    
    /* Always read and drain input to prevent buffering across states */
    if (read_line(input, 32) == NULL || strlen(input) == 0) {
        /* Empty input - in IDLE state with our turn, just re-display prompt */
        if (*state == STATE_IDLE && board->current_player == my_side) {
            printf("\nEnter pit number or 'm' for menu: ");
            fflush(stdout);
        } else if (*state == STATE_IDLE) {
            printf("> ");
            fflush(stdout);
        }
        return;
    }
    
    /* Check for menu command (works in any state) */
    if (input[0] == 'm' || input[0] == 'M') {
        printf("Returning to main menu...\n");
        *state = STATE_EXIT;
        return;
    }
    
    /* State-specific input handling */
    switch (*state) {
        case STATE_IDLE:
            if (board->current_player != my_side) {
                /* Not our turn - ignore pit numbers */
                printf("⚠️  Not your turn - waiting for opponent\n");
                printf("> ");
                fflush(stdout);
                return;
            }
            
            /* Our turn - try to parse pit number */
            int pit = atoi(input);
            int start_pit = (my_side == PLAYER_A) ? 0 : 6;
            int end_pit = (my_side == PLAYER_A) ? 5 : 11;
            
            /* Validate pit */
            if (pit < start_pit || pit > end_pit) {
                printf("❌ Invalid pit! Choose from %d-%d\n", start_pit, end_pit);
                printf("\nEnter pit number or 'm' for menu: ");
                fflush(stdout);
                return;
            }
            
            if (board->pits[pit] == 0) {
                printf("❌ That pit is empty!\n");
                printf("\nEnter pit number or 'm' for menu: ");
                fflush(stdout);
                return;
            }
            
            /* Valid move - send it */
            msg_play_move_t move;
            memset(&move, 0, sizeof(move));
            snprintf(move.game_id, MAX_GAME_ID_LEN, "%s", game_id);
            snprintf(move.player, MAX_PSEUDO_LEN, "%s", client_state_get_pseudo());
            move.pit_index = pit;
            
            error_code_t err = session_send_message(client_state_get_session(), MSG_PLAY_MOVE, 
                                                    &move, sizeof(move));
            if (err != SUCCESS) {
                printf("❌ Error sending move: %s\n", error_to_string(err));
                printf("\nEnter pit number or 'm' for menu: ");
                fflush(stdout);
                return;
            }
            
            printf("⏳ Move sent...\n");
            *state = STATE_MOVE_SENT;
            break;
            
        case STATE_MOVE_SENT:
            /* Waiting for move result - ignore input except 'm' (handled above) */
            printf("⏳ Please wait for move to be processed...\n");
            break;
            
        case STATE_WAITING_BOARD:
            /* Waiting for board - ignore input except 'm' (handled above) */
            printf("⏳ Loading board state...\n");
            break;
            
        case STATE_GAME_OVER:
            /* Game over - handle rematch */
            if (input[0] == 'y' || input[0] == 'Y') {
                const char* opponent = (my_side == PLAYER_A) ? board->player_b : board->player_a;
                
                msg_challenge_t challenge;
                memset(&challenge, 0, sizeof(challenge));
                snprintf(challenge.challenger, MAX_PSEUDO_LEN, "%s", client_state_get_pseudo());
                snprintf(challenge.opponent, MAX_PSEUDO_LEN, "%s", opponent);
                
                session_send_message(client_state_get_session(), MSG_CHALLENGE, 
                                   &challenge, sizeof(challenge));
                printf("✓ Challenge sent to %s!\n", opponent);
            }
            *state = STATE_EXIT;
            break;
            
        default:
            break;
    }
}

void cmd_play_mode(void) {
    printf("\n🎮 ENTERING PLAY MODE\n");
    
    /* Get active games count */
    int game_count = active_games_count();
    
    if (game_count == 0) {
        printf("❌ No active games found\n");
        printf("💡 Use option 2 to challenge a player first!\n");
        return;
    }
    
    /* Select game */
    active_game_t* selected_game = NULL;
    
    if (game_count == 1) {
        /* Auto-select single game */
        selected_game = active_games_get(0);
        printf("✓ Selected your active game:\n");
        printf("   %s vs %s\n", selected_game->player_a, selected_game->player_b);
    } else {
        /* Show game selection menu */
        printf("\nYou have %d active games:\n", game_count);
        printf("═══════════════════════════════════════════════════\n");
        for (int i = 0; i < game_count; i++) {
            active_game_t* game = active_games_get(i);
            if (game) {
                printf("  %d. %s vs %s\n", i + 1, game->player_a, game->player_b);
            }
        }
        printf("═══════════════════════════════════════════════════\n");
        printf("Select game (1-%d) or 0 to cancel: ", game_count);
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            clear_input();
            printf("❌ Invalid input\n");
            return;
        }
        clear_input();
        
        if (choice == 0) {
            printf("Cancelled.\n");
            return;
        }
        
        if (choice < 1 || choice > game_count) {
            printf("❌ Invalid choice\n");
            return;
        }
        
        selected_game = active_games_get(choice - 1);
    }
    
    if (!selected_game) {
        printf("❌ Failed to select game\n");
        return;
    }
    
    /* Initialize state machine */
    play_state_t state = STATE_INIT;
    char game_id_copy[MAX_GAME_ID_LEN];
    char player_a_copy[MAX_PSEUDO_LEN];
    char player_b_copy[MAX_PSEUDO_LEN];
    player_id_t my_side = selected_game->my_side;
    msg_board_state_t board;
    memset(&board, 0, sizeof(board));
    
    snprintf(game_id_copy, MAX_GAME_ID_LEN, "%s", selected_game->game_id);
    snprintf(player_a_copy, MAX_PSEUDO_LEN, "%s", selected_game->player_a);
    snprintf(player_b_copy, MAX_PSEUDO_LEN, "%s", selected_game->player_b);
    
    /* Clear stale notifications and input */
    active_games_clear_notifications();
    clear_input();
    
    printf("\n═══════════════════════════════════════════════════\n");
    printf("    PLAY MODE ACTIVE (Event-Driven)\n");
    printf("    Press 'm' + Enter to return to main menu\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    int notification_fd = active_games_get_notification_fd();
    bool should_request_board = true;
    
    /* ═══════════════════════════════════════════════════════════════
     * EVENT LOOP - Single polling point for all events
     * ═══════════════════════════════════════════════════════════════ */
    while (state != STATE_EXIT && client_state_is_running()) {
        
        /* STATE: INIT - Initial board request */
        if (state == STATE_INIT) {
            error_code_t err = request_board(player_a_copy, player_b_copy);
            if (err != SUCCESS) {
                printf("❌ Error requesting initial board: %s\n", error_to_string(err));
                break;
            }
            state = STATE_WAITING_BOARD;
            should_request_board = false;
            continue;
        }
        
        /* Request board if needed (after notification or state transition) */
        if (should_request_board) {
            error_code_t err = request_board(player_a_copy, player_b_copy);
            if (err != SUCCESS) {
                printf("❌ Error requesting board: %s\n", error_to_string(err));
                if (err == ERR_NETWORK_ERROR) {
                    printf("❌ Connection lost\n");
                    break;
                }
                continue;
            }
            state = STATE_WAITING_BOARD;
            should_request_board = false;
            continue;
        }
        
        /* STATE: WAITING_BOARD - Expecting board response from server */
        if (state == STATE_WAITING_BOARD) {
            error_code_t err = receive_board(&board);
            
            if (err == ERR_TIMEOUT) {
                /* Timeout - try requesting again */
                should_request_board = true;
                state = STATE_INIT;
                continue;
            }
            
            if (err == ERR_NETWORK_ERROR) {
                /* Connection lost */
                printf("❌ Connection lost\n");
                break;
            }
            
            if (err != SUCCESS) {
                /* Other error - retry */
                should_request_board = true;
                state = STATE_INIT;
                continue;
            }
            
            /* Board received successfully */
            if (!board.exists) {
                printf("❌ Game no longer exists\n");
                active_games_remove(game_id_copy);
                break;
            }
            
            /* Check if game is over */
            if (board.state == GAME_STATE_FINISHED) {
                printf("\n🏁 ═══════════════════════════════════════════════════\n");
                printf("   GAME FINISHED!\n");
                if (board.winner == WINNER_A) {
                    printf("   🏆 Winner: %s (Score: %d)\n", board.player_a, board.score_a);
                    printf("   Player B: %s (Score: %d)\n", board.player_b, board.score_b);
                } else if (board.winner == WINNER_B) {
                    printf("   🏆 Winner: %s (Score: %d)\n", board.player_b, board.score_b);
                    printf("   Player A: %s (Score: %d)\n", board.player_a, board.score_a);
                } else {
                    printf("   🤝 Draw! (%d - %d)\n", board.score_a, board.score_b);
                }
                printf("═══════════════════════════════════════════════════\n");
                printf("\nWould you like to challenge them again? (y/n): ");
                fflush(stdout);
                
                state = STATE_GAME_OVER;
                continue;
            }
            
            /* Display board and transition to IDLE */
            display_board(&board, my_side);
            state = STATE_IDLE;
            continue;
        }
        
        /* STATE: MOVE_SENT - Waiting for move result notification */
        if (state == STATE_MOVE_SENT) {
            /* Wait for notification that move was processed */
            event_type_t event = poll_events(notification_fd, 5000);  /* 5 second timeout */
            
            if (event == EVENT_NOTIFICATION) {
                /* Move processed - request fresh board */
                active_games_clear_notifications();
                should_request_board = true;
                state = STATE_INIT;
                continue;
            }
            
            if (event == EVENT_TIMEOUT) {
                /* Timeout waiting for move result - request board to re-sync */
                printf("⚠️  Move response timeout - refreshing board\n");
                should_request_board = true;
                state = STATE_INIT;
                continue;
            }
            
            if (event == EVENT_USER_INPUT) {
                /* User typed something while waiting - drain it */
                char input[32];
                handle_user_input(&state, &board, my_side, game_id_copy, input, &should_request_board);
                continue;
            }
            
            continue;
        }
        
        /* STATE: IDLE - Board displayed, waiting for events */
        if (state == STATE_IDLE) {
            /* Poll for events - no timeout, wait indefinitely */
            event_type_t event = poll_events(notification_fd, -1);
            
            if (event == EVENT_USER_INPUT) {
                /* Process ALL user input before handling server events */
                char input[32];
                handle_user_input(&state, &board, my_side, game_id_copy, input, &should_request_board);
                continue;
            }
            
            if (event == EVENT_NOTIFICATION) {
                /* Server notification (opponent moved, game state changed) */
                active_games_clear_notifications();
                should_request_board = true;
                continue;
            }
            
            continue;
        }
        
        /* STATE: GAME_OVER - Handle rematch decision */
        if (state == STATE_GAME_OVER) {
            char input[32];
            handle_user_input(&state, &board, my_side, game_id_copy, input, &should_request_board);
            continue;
        }
    }
    
    /* Cleanup */
    active_games_remove(game_id_copy);
    
    printf("\nExiting play mode...\n");
}
