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
#include "../../include/client/client_logging.h"
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
        client_log_error(CLIENT_LOG_FAILED_SEND_BOARD_REQUEST);
    } else if (err == ERR_TIMEOUT) {
        client_log_warning(CLIENT_LOG_NETWORK_SLOW);
    } else if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_BOARD_REQUEST, error_to_string(err));
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
                client_log_warning(CLIENT_LOG_SERVER_RESPONSE_TIMEOUT, retries, MAX_RETRIES + 1);
                continue;
            }
            client_log_error(CLIENT_LOG_SERVER_NOT_RESPONDING, MAX_RETRIES + 1);
            return err;
        }
        
        if (err == ERR_NETWORK_ERROR) {
            client_log_error(CLIENT_LOG_CONNECTION_LOST);
            return err;
        }
        
        if (err != SUCCESS) {
            client_log_error(CLIENT_LOG_ERROR_RECEIVING_BOARD, error_to_string(err));
            return err;
        }
        
        if (type != MSG_BOARD_STATE) {
            /* Unexpected message type - might be out of order */
            retries++;
            if (retries <= MAX_RETRIES) {
                client_log_warning(CLIENT_LOG_UNEXPECTED_MESSAGE_TYPE, type, MSG_BOARD_STATE);
                /* Request board again */
                return ERR_UNKNOWN;
            }
            client_log_error(CLIENT_LOG_PROTOCOL_ERROR);
            return ERR_UNKNOWN;
        }

        /* Success - log board reception */
        client_log_info(CLIENT_LOG_BOARD_RECEIVED);
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
        
        int legal_moves[6];
        int count = 0;
        for (int i = start_pit; i <= end_pit; i++) {
            if (board->pits[i] > 0) {
                legal_moves[count++] = i;
            }
        }
        
        ui_display_turn_info(1, legal_moves, count);
    } else {
        ui_display_turn_info(0, NULL, 0);
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
            client_log_info(CLIENT_LOG_ENTER_PIT_OR_MENU);
            fflush(stdout);
        } else if (*state == STATE_IDLE) {
            client_log_info(CLIENT_LOG_PROMPT);
            fflush(stdout);
        }
        return;
    }
    
    /* Check for menu command (works in any state) */
    if (input[0] == 'm' || input[0] == 'M') {
        client_log_info(CLIENT_LOG_RETURNING_TO_MENU);
        *state = STATE_EXIT;
        return;
    }
    
    /* State-specific input handling */
    switch (*state) {
        case STATE_IDLE:
            if (board->current_player != my_side) {
                /* Not our turn - ignore pit numbers */
                client_log_info(CLIENT_LOG_NOT_YOUR_TURN);
                client_log_info(CLIENT_LOG_PROMPT);
                fflush(stdout);
                return;
            }
            
            /* Our turn - try to parse pit number */
            int pit = atoi(input);
            int start_pit = (my_side == PLAYER_A) ? 0 : 6;
            int end_pit = (my_side == PLAYER_A) ? 5 : 11;
            
            /* Validate pit */
            if (pit < start_pit || pit > end_pit) {
                client_log_error(CLIENT_LOG_INVALID_PIT, start_pit, end_pit);
                client_log_info(CLIENT_LOG_ENTER_PIT_OR_MENU);
                fflush(stdout);
                return;
            }
            
            if (board->pits[pit] == 0) {
                client_log_error(CLIENT_LOG_PIT_EMPTY);
                client_log_info(CLIENT_LOG_ENTER_PIT_OR_MENU);
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
                client_log_error(CLIENT_LOG_ERROR_SENDING_MOVE, error_to_string(err));
                client_log_info(CLIENT_LOG_ENTER_PIT_OR_MENU);
                fflush(stdout);
                return;
            }
            
            client_log_info(CLIENT_LOG_MOVE_SENT);
            *state = STATE_MOVE_SENT;
            break;
            
        case STATE_MOVE_SENT:
            /* Waiting for move result - ignore input except 'm' (handled above) */
            client_log_info(CLIENT_LOG_WAIT_FOR_MOVE_PROCESS);
            break;
            
        case STATE_WAITING_BOARD:
            /* Waiting for board - ignore input except 'm' (handled above) */
            client_log_info(CLIENT_LOG_LOADING_BOARD_STATE);
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
                client_log_info(CLIENT_LOG_CHALLENGE_SENT_TO, opponent);
            }
            *state = STATE_EXIT;
            break;
            
        default:
            break;
    }
}

void cmd_play_mode(void) {
    client_log_info(CLIENT_LOG_ENTERING_PLAY_MODE);

    /* Get active games count */
    int game_count = active_games_count();

    /* If no local active games, query server for player's games */
    if (game_count == 0) {
        session_t* session = client_state_get_session();

        client_log_info(CLIENT_LOG_LOADING_BOARD_STATE);  /* Reuse loading message */

        error_code_t err = session_send_message(session, MSG_LIST_MY_GAMES, NULL, 0);
        if (err != SUCCESS) {
            client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
            return;
        }

        message_type_t type;
        msg_my_game_list_t list;
        size_t size;

        err = session_recv_message_timeout(session, &type, &list, sizeof(list), &size, 5000);
        if (err == ERR_TIMEOUT) {
            client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
            return;
        }
        if (err != SUCCESS || type != MSG_MY_GAME_LIST) {
            client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
            return;
        }

        game_count = list.count;

        /* Add server games to local active_games if not already present */
        for (int i = 0; i < game_count; i++) {
            bool already_exists = false;
            for (int j = 0; j < active_games_count(); j++) {
                active_game_t* existing = active_games_get(j);
                if (existing && strcmp(existing->game_id, list.games[i].game_id) == 0) {
                    already_exists = true;
                    break;
                }
            }
            if (!already_exists) {
                /* Determine my side */
                player_id_t my_side;
                const char* my_pseudo = client_state_get_pseudo();
                if (strcmp(list.games[i].player_a, my_pseudo) == 0) {
                    my_side = PLAYER_A;
                } else if (strcmp(list.games[i].player_b, my_pseudo) == 0) {
                    my_side = PLAYER_B;
                } else {
                    continue;  /* Not my game */
                }
                active_games_add(list.games[i].game_id, list.games[i].player_a,
                               list.games[i].player_b, my_side);
            }
        }

        game_count = active_games_count();  /* Update count after adding */
    }

    if (game_count == 0) {
        client_log_error(CLIENT_LOG_NO_ACTIVE_GAMES);
        client_log_info(CLIENT_LOG_USE_OPTION_2_FIRST);
        return;
    }
    
    /* Select game */
    active_game_t* selected_game = NULL;
    
    if (game_count == 1) {
        /* Auto-select single game */
        selected_game = active_games_get(0);
        client_log_info(CLIENT_LOG_SELECTED_ACTIVE_GAME);
        client_log_info(CLIENT_LOG_GAME_VS_FORMAT, selected_game->player_a, selected_game->player_b);
    } else {
        /* Show game selection menu */
        client_log_info(CLIENT_LOG_ACTIVE_GAMES_COUNT, game_count);
        client_log_info(CLIENT_LOG_GAME_LIST_SEPARATOR);
        for (int i = 0; i < game_count; i++) {
            active_game_t* game = active_games_get(i);
            if (game) {
                client_log_info(CLIENT_LOG_GAME_LIST_ITEM, i + 1, game->player_a, game->player_b);
            }
        }
        client_log_info(CLIENT_LOG_GAME_LIST_SEPARATOR);
        client_log_info(CLIENT_LOG_SELECT_GAME_PROMPT, game_count);
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            clear_input();
            client_log_error(CLIENT_LOG_INVALID_INPUT);
            return;
        }
        clear_input();
        
        if (choice == 0) {
            client_log_info(CLIENT_LOG_CANCELLED);
            return;
        }
        
        if (choice < 1 || choice > game_count) {
            client_log_error(CLIENT_LOG_INVALID_CHOICE);
            return;
        }
        
        selected_game = active_games_get(choice - 1);
    }
    
    if (!selected_game) {
        client_log_error(CLIENT_LOG_FAILED_SELECT_GAME);
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
    
    client_log_info(CLIENT_LOG_PLAY_MODE_HEADER);
    client_log_info(CLIENT_LOG_PLAY_MODE_ACTIVE);
    client_log_info(CLIENT_LOG_PLAY_MODE_RETURN_HINT);
    client_log_info(CLIENT_LOG_PLAY_MODE_FOOTER);
    
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
                client_log_error(CLIENT_LOG_ERROR_REQUESTING_INITIAL_BOARD, error_to_string(err));
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
                client_log_error(CLIENT_LOG_ERROR_REQUESTING_BOARD, error_to_string(err));
                if (err == ERR_NETWORK_ERROR) {
                    client_log_error(CLIENT_LOG_CONNECTION_LOST_SHORT);
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
                client_log_error(CLIENT_LOG_CONNECTION_LOST_SHORT);
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
                client_log_error(CLIENT_LOG_GAME_NO_LONGER_EXISTS);
                active_games_remove(game_id_copy);
                break;
            }
            
            /* Check if game is over */
            if (board.state == GAME_STATE_FINISHED) {
                client_log_info(CLIENT_LOG_GAME_OVER_HEADER);
                client_log_info(CLIENT_LOG_GAME_FINISHED);
                if (board.winner == WINNER_A) {
                    client_log_info(CLIENT_LOG_GAME_WINNER, board.player_a, board.score_a);
                    client_log_info(CLIENT_LOG_GAME_PLAYER_B, board.player_b, board.score_b);
                } else if (board.winner == WINNER_B) {
                    client_log_info(CLIENT_LOG_GAME_WINNER, board.player_b, board.score_b);
                    client_log_info(CLIENT_LOG_GAME_PLAYER_A, board.player_a, board.score_a);
                } else {
                    client_log_info(CLIENT_LOG_GAME_DRAW, board.score_a, board.score_b);
                }
                client_log_info(CLIENT_LOG_GAME_OVER_SEPARATOR);
                client_log_info(CLIENT_LOG_GAME_REMATCH_PROMPT);
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
                client_log_info(CLIENT_LOG_MOVE_TIMEOUT);
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

    /* Cleanup - user explicitly exited, so remove from active games */
    client_log_info(CLIENT_LOG_GAME_REMOVED_ON_EXIT, game_id_copy);
    active_games_remove(game_id_copy);

    client_log_info(CLIENT_LOG_EXITING_PLAY_MODE);
}
