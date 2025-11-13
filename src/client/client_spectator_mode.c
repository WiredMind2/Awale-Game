/* Client Spectator Mode - Implementation
 * Interactive spectator mode for watching games
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_spectator_mode.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_ui.h"
#include "../../include/client/client_logging.h"
#include "../../include/common/messages.h"
#include "../../include/common/protocol.h"
#include "../../include/network/session.h"
#include "../../include/game/board.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>

void cmd_spectator_mode(void) {
    client_log_info(CLIENT_LOG_SPECTATOR_MODE_HEADER);
    client_log_info(CLIENT_LOG_SPECTATOR_MODE_TITLE);
    client_log_info(CLIENT_LOG_SPECTATOR_MODE_SEPARATOR);
    client_log_info(CLIENT_LOG_SPECTATOR_MODE_INFO);
    
    /* Request list of active games */
    error_code_t err = session_send_message(client_state_get_session(), MSG_LIST_GAMES, NULL, 0);
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_FAILED_REQUEST_GAME_LIST);
        return;
    }
    
    /* Wait for game list response */
    message_type_t type;
    msg_game_list_t game_list;
    size_t size;
    
    err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&game_list, 
                                       sizeof(game_list), &size, 5000);
    
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_GAME_LIST);
        return;
    }
    
    if (err != SUCCESS || type != MSG_GAME_LIST) {
        client_log_error(CLIENT_LOG_FAILED_RECEIVE_GAME_LIST);
        return;
    }
    
    if (game_list.count == 0) {
        client_log_info(CLIENT_LOG_NO_GAMES_IN_PROGRESS);
        client_log_info(CLIENT_LOG_PRESS_ENTER_CONTINUE);
        getchar();
        return;
    }
    
    /* Display available games */
    client_log_info(CLIENT_LOG_AVAILABLE_GAMES);
    for (int i = 0; i < game_list.count; i++) {
        game_info_t* game = &game_list.games[i];
        client_log_info(CLIENT_LOG_GAME_LIST_ITEM, i + 1, game->player_a, game->player_b);
        client_log_info(CLIENT_LOG_GAME_ID, game->game_id);
        client_log_info(CLIENT_LOG_SPECTATORS, game->spectator_count);
        client_log_info(CLIENT_LOG_GAME_STATE);
        if (game->state == GAME_STATE_WAITING) client_log_info(CLIENT_LOG_GAME_STATE_WAITING);
        else if (game->state == GAME_STATE_IN_PROGRESS) client_log_info(CLIENT_LOG_GAME_STATE_IN_PROGRESS);
        else if (game->state == GAME_STATE_FINISHED) client_log_info(CLIENT_LOG_GAME_STATE_FINISHED);
        client_log_info(CLIENT_LOG_GAME_STATE_NEWLINE);
    }
    
    /* Get user selection */
    client_log_info(CLIENT_LOG_SELECT_GAME, game_list.count);
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
    
    if (choice < 1 || choice > game_list.count) {
        client_log_error(CLIENT_LOG_INVALID_GAME_SELECTION);
        return;
    }
    
    game_info_t* selected_game = &game_list.games[choice - 1];
    
    /* Send spectate request */
    msg_spectate_game_t spectate_req;
    snprintf(spectate_req.game_id, sizeof(spectate_req.game_id), "%s", selected_game->game_id);
    
    err = session_send_message(client_state_get_session(), MSG_SPECTATE_GAME, &spectate_req, sizeof(spectate_req));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_SPECTATOR_FAILED_SEND_REQUEST);
        return;
    }
    
    /* Wait for acknowledgment */
    msg_spectate_ack_t ack;
    err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&ack, 
                                       sizeof(ack), &size, 5000);
    
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_SPECTATOR_TIMEOUT_ACK);
        return;
    }
    
    if (err != SUCCESS || type != MSG_SPECTATE_ACK) {
        client_log_error(CLIENT_LOG_SPECTATOR_FAILED_RECEIVE_ACK);
        return;
    }
    
    if (!ack.success) {
        client_log_error(CLIENT_LOG_SPECTATOR_REQUEST_DENIED, ack.message);
        return;
    }
    
    client_log_info(CLIENT_LOG_SPECTATOR_ACK_MESSAGE, ack.message);
    
    /* Set spectator state */
    spectator_state_set(selected_game->game_id, selected_game->player_a, selected_game->player_b);

    /* Spectator loop */
    client_log_info(CLIENT_LOG_SPECTATOR_GAME_HEADER);
    client_log_info(CLIENT_LOG_SPECTATOR_NOW_SPECTATING, selected_game->player_a, selected_game->player_b);
    client_log_info(CLIENT_LOG_SPECTATOR_SEPARATOR);
    client_log_info(CLIENT_LOG_SPECTATOR_COMMANDS);
    client_log_info(CLIENT_LOG_SPECTATOR_COMMAND_REFRESH);
    client_log_info(CLIENT_LOG_SPECTATOR_COMMAND_QUIT);

    /* Flag to prevent concurrent board requests */
    bool board_request_pending = false;

    /* Initial board request */
    msg_get_board_t board_req;
    memset(&board_req, 0, sizeof(board_req));
    snprintf(board_req.player_a, MAX_PSEUDO_LEN, "%s", selected_game->player_a);
    snprintf(board_req.player_b, MAX_PSEUDO_LEN, "%s", selected_game->player_b);
    err = session_send_message(client_state_get_session(), MSG_GET_BOARD, &board_req, sizeof(board_req));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_SPECTATOR_FAILED_SEND_REQUEST);
        return;
    }
    board_request_pending = true;

    /* Display initial board */
    msg_board_state_t board_state;
    err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&board_state,
                                        sizeof(board_state), &size, 5000);

    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_GAME_LIST);  /* Reuse timeout message */
        client_log_error(CLIENT_LOG_SPECTATOR_FAILED_REFRESH);
        return;  /* Exit spectator mode on timeout */
    } else if (err == ERR_NETWORK_ERROR) {
        client_log_error(CLIENT_LOG_CONNECTION_LOST);
        return;  /* Exit spectator mode on network error */
    } else if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_SPECTATOR_FAILED_RECEIVE_ACK);  /* Reuse generic error */
        return;  /* Exit spectator mode on other errors */
    }

    board_request_pending = false;  /* Request completed */

    if (type != MSG_BOARD_STATE) {
        /* Handle unexpected message types */
        if (IS_NOTIFICATION_MESSAGE(type)) {
            /* This is a notification that arrived during board request wait */
            /* Process it here since notification listener might not have caught it */
            client_log_info("Received notification %d during board request, processing...", type);

            /* Handle the notification */
            if (type == MSG_CHALLENGE_RECEIVED) {
                // Handle challenge received
                msg_challenge_received_t* notif = (msg_challenge_received_t*)&board_state;  // Reuse buffer
                pending_challenges_add(notif->from, notif->challenge_id);
                ui_display_challenge_received(notif);
            } else if (type == MSG_GAME_STARTED) {
                // Handle game started
                msg_game_started_t* start = (msg_game_started_t*)&board_state;
                active_games_add(start->game_id, start->player_a, start->player_b, start->your_side);
                ui_display_game_started(start);
            } else if (type == MSG_MOVE_RESULT) {
                /* Move result - trigger board update */
                spectator_state_notify_update();
            } else if (type == MSG_GAME_OVER) {
                // Handle game over
                msg_game_over_t* game_over = (msg_game_over_t*)&board_state;
                active_games_remove(game_over->game_id);
                ui_display_game_over(game_over);
            } else if (type == MSG_CHAT_MESSAGE) {
                // Handle chat message
                msg_chat_message_t* chat = (msg_chat_message_t*)&board_state;
                ui_display_chat_message(chat);
            }

            /* Continue waiting for board state */
            err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&board_state,
                                                sizeof(board_state), &size, 5000);
            if (err != SUCCESS || type != MSG_BOARD_STATE) {
                client_log_error("Still failed to receive board state after processing notification");
                return;
            }
        } else {
            /* Unexpected non-notification message */
            client_log_error(CLIENT_LOG_PROTOCOL_ERROR);
            client_log_error(CLIENT_LOG_SPECTATOR_UNEXPECTED_MSG, type, MSG_BOARD_STATE);
            return;
        }
    }

    /* Create board_t from board_state for printing */
    board_t board;
    memcpy(board.pits, board_state.pits, sizeof(board.pits));
    board.scores[0] = board_state.score_a;  /* Player A */
    board.scores[1] = board_state.score_b;  /* Player B */
    board.current_player = board_state.current_player;

    client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
    ui_display_board_simple(&board);
    client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
    client_log_info(CLIENT_LOG_SPECTATOR_SCORE_FORMAT,
           board_state.player_a, board_state.score_a,
           board_state.player_b, board_state.score_b);
    const char* current_str = (board_state.current_player == PLAYER_A) ? board_state.player_a : board_state.player_b;
    client_log_info(CLIENT_LOG_SPECTATOR_CURRENT_TURN, current_str);
    client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
    
    bool spectating = true;
    bool prompt_printed = false;
    while (spectating && client_state_is_running()) {
        if (!prompt_printed) {
            client_log_info(CLIENT_LOG_SPECTATOR_PROMPT);
            fflush(stdout);
            prompt_printed = true;
        }

        /* Wait for user input or board update with 1 second timeout */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        struct timeval tv;
        tv.tv_sec = 1;  /* Check every second */
        tv.tv_usec = 0;

        int ready = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv);

        if (ready > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
            prompt_printed = false;  /* Reset for next input */
            char cmd[10];
            if (fgets(cmd, sizeof(cmd), stdin) == NULL) {
                break;
            }
            
            if (cmd[0] == 'q') {
                spectating = false;
                break;
            } else if (cmd[0] == 'r') {
                /* Refresh board - only if no request pending */
                if (!board_request_pending) {
                    msg_get_board_t refresh_req;
                    memset(&refresh_req, 0, sizeof(refresh_req));
                    snprintf(refresh_req.player_a, MAX_PSEUDO_LEN, "%s", selected_game->player_a);
                    snprintf(refresh_req.player_b, MAX_PSEUDO_LEN, "%s", selected_game->player_b);
                    err = session_send_message(client_state_get_session(), MSG_GET_BOARD, &refresh_req, sizeof(refresh_req));
                    if (err != SUCCESS) {
                        client_log_error(CLIENT_LOG_SPECTATOR_FAILED_SEND_REQUEST);
                        spectating = false;  /* Exit spectator mode on send failure */
                        break;
                    }
                    board_request_pending = true;

                    err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&board_state,
                                                        sizeof(board_state), &size, 5000);
                    board_request_pending = false;

                    if (err == ERR_TIMEOUT) {
                        client_log_error(CLIENT_LOG_TIMEOUT_GAME_LIST);  /* Reuse timeout message */
                        client_log_error(CLIENT_LOG_SPECTATOR_FAILED_REFRESH);
                        /* Continue spectating on timeout - user can retry */
                    } else if (err == ERR_NETWORK_ERROR) {
                        client_log_error(CLIENT_LOG_CONNECTION_LOST);
                        spectating = false;  /* Exit spectator mode on network error */
                        break;
                    } else if (err != SUCCESS) {
                        client_log_error(CLIENT_LOG_SPECTATOR_FAILED_RECEIVE_ACK);  /* Reuse generic error */
                        spectating = false;  /* Exit spectator mode on other errors */
                        break;
                    } else if (type != MSG_BOARD_STATE) {
                        client_log_error(CLIENT_LOG_PROTOCOL_ERROR);
                        spectating = false;  /* Exit spectator mode on protocol error */
                        break;
                    } else {
                        /* Success - display board */
                        /* Create board_t from board_state for printing */
                        board_t board;
                        memcpy(board.pits, board_state.pits, sizeof(board.pits));
                        board.scores[0] = board_state.score_a;  /* Player A */
                        board.scores[1] = board_state.score_b;  /* Player B */
                        board.current_player = board_state.current_player;

                        client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
                        ui_display_board_simple(&board);
                        client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
                        client_log_info(CLIENT_LOG_SPECTATOR_SCORE_FORMAT,
                            board_state.player_a, board_state.score_a,
                            board_state.player_b, board_state.score_b);
                        const char* current_str = (board_state.current_player == PLAYER_A) ? board_state.player_a : board_state.player_b;
                        client_log_info(CLIENT_LOG_SPECTATOR_CURRENT_TURN, current_str);
                        client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
                    }
                } else {
                    client_log_info("Board refresh already in progress, please wait...");
                }
            }
        }
        
        /* Check if board was updated by notification */
        if (spectator_state_check_and_clear_updated() && !board_request_pending) {
            prompt_printed = false;  /* Reset prompt flag when board updates */

            /* Auto-refresh board */
            client_log_info(CLIENT_LOG_SPECTATOR_BOARD_UPDATED);
            msg_get_board_t update_req;
            memset(&update_req, 0, sizeof(update_req));
            snprintf(update_req.player_a, MAX_PSEUDO_LEN, "%s", selected_game->player_a);
            snprintf(update_req.player_b, MAX_PSEUDO_LEN, "%s", selected_game->player_b);
            err = session_send_message(client_state_get_session(), MSG_GET_BOARD, &update_req, sizeof(update_req));
            if (err != SUCCESS) {
                client_log_error(CLIENT_LOG_SPECTATOR_FAILED_SEND_REQUEST);
                spectating = false;  /* Exit spectator mode on send failure */
                break;
            }
            board_request_pending = true;

            err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&board_state,
                                                sizeof(board_state), &size, 5000);
            board_request_pending = false;

            if (err == ERR_TIMEOUT) {
                client_log_error(CLIENT_LOG_TIMEOUT_GAME_LIST);  /* Reuse timeout message */
                client_log_error(CLIENT_LOG_SPECTATOR_FAILED_REFRESH);
                /* Continue spectating on timeout - notification will trigger again */
            } else if (err == ERR_NETWORK_ERROR) {
                client_log_error(CLIENT_LOG_CONNECTION_LOST);
                spectating = false;  /* Exit spectator mode on network error */
                break;
            } else if (err != SUCCESS) {
                client_log_error(CLIENT_LOG_SPECTATOR_FAILED_RECEIVE_ACK);  /* Reuse generic error */
                spectating = false;  /* Exit spectator mode on other errors */
                break;
            } else if (type != MSG_BOARD_STATE) {
                client_log_error(CLIENT_LOG_PROTOCOL_ERROR);
                spectating = false;  /* Exit spectator mode on protocol error */
                break;
            } else {
                /* Success - display board */
                /* Create board_t from board_state for printing */
                board_t board;
                memcpy(board.pits, board_state.pits, sizeof(board.pits));
                board.scores[0] = board_state.score_a;  /* Player A */
                board.scores[1] = board_state.score_b;  /* Player B */
                board.current_player = board_state.current_player;

                client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
                ui_display_board_simple(&board);
                client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
                client_log_info(CLIENT_LOG_SPECTATOR_SCORE_FORMAT,
                    board_state.player_a, board_state.score_a,
                    board_state.player_b, board_state.score_b);
                const char* current_str = (board_state.current_player == PLAYER_A) ? board_state.player_a : board_state.player_b;
                client_log_info(CLIENT_LOG_SPECTATOR_CURRENT_TURN, current_str);
                client_log_info(CLIENT_LOG_SPECTATOR_NEWLINE);
            }
        }
    }
    
    /* Send stop spectating message */
    msg_spectate_game_t stop_req;
    snprintf(stop_req.game_id, sizeof(stop_req.game_id), "%s", selected_game->game_id);
    session_send_message(client_state_get_session(), MSG_STOP_SPECTATE, &stop_req, sizeof(stop_req));
    
    /* Clear spectator state */
    spectator_state_clear();
    
    client_log_info(CLIENT_LOG_SPECTATOR_STOPPED);
}
