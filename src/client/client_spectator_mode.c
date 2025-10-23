/* Client Spectator Mode - Implementation
 * Interactive spectator mode for watching games
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_spectator_mode.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_ui.h"
#include "../../include/common/messages.h"
#include "../../include/common/protocol.h"
#include "../../include/network/session.h"
#include "../../include/game/board.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

void cmd_spectator_mode(void) {
    printf("\n👁️ ═══════════════════════════════════════════════════\n");
    printf("   SPECTATOR MODE\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    
    /* Request list of active games */
    error_code_t err = session_send_message(client_state_get_session(), MSG_LIST_GAMES, NULL, 0);
    if (err != SUCCESS) {
        printf("❌ Failed to request game list\n");
        return;
    }
    
    /* Wait for game list response */
    message_type_t type;
    msg_game_list_t game_list;
    size_t size;
    
    err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&game_list, 
                                       sizeof(game_list), &size, 5000);
    
    if (err == ERR_TIMEOUT) {
        printf("❌ Timeout waiting for game list\n");
        return;
    }
    
    if (err != SUCCESS || type != MSG_GAME_LIST) {
        printf("❌ Failed to receive game list\n");
        return;
    }
    
    if (game_list.count == 0) {
        printf("No games currently in progress.\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    /* Display available games */
    printf("Available games to spectate:\n\n");
    for (int i = 0; i < game_list.count; i++) {
        game_info_t* game = &game_list.games[i];
        printf("  %d. %s vs %s\n", i + 1, game->player_a, game->player_b);
        printf("     Game ID: %s\n", game->game_id);
        printf("     Spectators: %d\n", game->spectator_count);
        printf("     State: ");
        if (game->state == GAME_STATE_WAITING) printf("Waiting\n");
        else if (game->state == GAME_STATE_IN_PROGRESS) printf("In Progress\n");
        else if (game->state == GAME_STATE_FINISHED) printf("Finished\n");
        printf("\n");
    }
    
    /* Get user selection */
    printf("Select game to spectate (1-%d, 0 to cancel): ", game_list.count);
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
    
    if (choice < 1 || choice > game_list.count) {
        printf("❌ Invalid game selection\n");
        return;
    }
    
    game_info_t* selected_game = &game_list.games[choice - 1];
    
    /* Send spectate request */
    msg_spectate_game_t spectate_req;
    snprintf(spectate_req.game_id, sizeof(spectate_req.game_id), "%s", selected_game->game_id);
    
    err = session_send_message(client_state_get_session(), MSG_SPECTATE_GAME, &spectate_req, sizeof(spectate_req));
    if (err != SUCCESS) {
        printf("❌ Failed to send spectate request\n");
        return;
    }
    
    /* Wait for acknowledgment */
    msg_spectate_ack_t ack;
    err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&ack, 
                                       sizeof(ack), &size, 5000);
    
    if (err == ERR_TIMEOUT) {
        printf("❌ Timeout waiting for spectate acknowledgment\n");
        return;
    }
    
    if (err != SUCCESS || type != MSG_SPECTATE_ACK) {
        printf("❌ Failed to receive spectate acknowledgment\n");
        return;
    }
    
    if (!ack.success) {
        printf("❌ Spectate request denied: %s\n", ack.message);
        return;
    }
    
    printf("✓ %s\n", ack.message);
    
    /* Set spectator state */
    spectator_state_set(selected_game->game_id, selected_game->player_a, selected_game->player_b);
    
    /* Spectator loop */
    printf("\n🎮 ═══════════════════════════════════════════════════\n");
    printf("   NOW SPECTATING: %s vs %s\n", selected_game->player_a, selected_game->player_b);
    printf("═══════════════════════════════════════════════════════\n");
    printf("\nCommands:\n");
    printf("  'r' - Refresh board\n");
    printf("  'q' - Stop spectating\n\n");
    
    /* Initial board request */
    msg_get_board_t board_req;
    snprintf(board_req.game_id, sizeof(board_req.game_id), "%s", selected_game->game_id);
    session_send_message(client_state_get_session(), MSG_GET_BOARD, &board_req, sizeof(board_req));
    
    /* Display initial board */
    msg_board_state_t board_state;
    err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&board_state, 
                                       sizeof(board_state), &size, 5000);
    if (err == SUCCESS && type == MSG_BOARD_STATE) {
        /* Create board_t from board_state for printing */
        board_t board;
        memcpy(board.pits, board_state.pits, sizeof(board.pits));
        board.scores[0] = board_state.score_a;  /* Player A */
        board.scores[1] = board_state.score_b;  /* Player B */
        board.current_player = board_state.current_player;
        
        printf("\n");
        board_print(&board);
        printf("\n");
        printf("Score - %s: %d  |  %s: %d\n", 
               board_state.player_a, board_state.score_a,
               board_state.player_b, board_state.score_b);
        const char* current_str = (board_state.current_player == PLAYER_A) ? board_state.player_a : board_state.player_b;
        printf("Current turn: %s\n", current_str);
        printf("\n");
    }
    
    bool spectating = true;
    while (spectating) {
        printf("Spectator> ");
        fflush(stdout);
        
        /* Wait for user input or board update with 30 second timeout */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 1;  /* Check every second */
        tv.tv_usec = 0;
        
        int ready = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv);
        
        if (ready > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
            char cmd[10];
            if (fgets(cmd, sizeof(cmd), stdin) == NULL) {
                break;
            }
            
            if (cmd[0] == 'q') {
                spectating = false;
                break;
            } else if (cmd[0] == 'r') {
                /* Refresh board */
                session_send_message(client_state_get_session(), MSG_GET_BOARD, &board_req, sizeof(board_req));
                
                err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&board_state, 
                                                   sizeof(board_state), &size, 5000);
                if (err == SUCCESS && type == MSG_BOARD_STATE) {
                    /* Create board_t from board_state for printing */
                    board_t board;
                    memcpy(board.pits, board_state.pits, sizeof(board.pits));
                    board.scores[0] = board_state.score_a;  /* Player A */
                    board.scores[1] = board_state.score_b;  /* Player B */
                    board.current_player = board_state.current_player;
                    
                    printf("\n");
                    board_print(&board);
                    printf("\n");
                    printf("Score - %s: %d  |  %s: %d\n", 
                           board_state.player_a, board_state.score_a,
                           board_state.player_b, board_state.score_b);
                    const char* current_str = (board_state.current_player == PLAYER_A) ? board_state.player_a : board_state.player_b;
                    printf("Current turn: %s\n", current_str);
                    printf("\n");
                } else {
                    printf("❌ Failed to refresh board\n");
                }
            }
        }
        
        /* Check if board was updated by notification */
        if (spectator_state_check_and_clear_updated()) {
            
            /* Auto-refresh board */
            printf("\n🔔 Board updated! Refreshing...\n");
            session_send_message(client_state_get_session(), MSG_GET_BOARD, &board_req, sizeof(board_req));
            
            err = session_recv_message_timeout(client_state_get_session(), &type, (char*)&board_state, 
                                               sizeof(board_state), &size, 5000);
            if (err == SUCCESS && type == MSG_BOARD_STATE) {
                /* Create board_t from board_state for printing */
                board_t board;
                memcpy(board.pits, board_state.pits, sizeof(board.pits));
                board.scores[0] = board_state.score_a;  /* Player A */
                board.scores[1] = board_state.score_b;  /* Player B */
                board.current_player = board_state.current_player;
                
                printf("\n");
                board_print(&board);
                printf("\n");
                printf("Score - %s: %d  |  %s: %d\n", 
                       board_state.player_a, board_state.score_a,
                       board_state.player_b, board_state.score_b);
                const char* current_str = (board_state.current_player == PLAYER_A) ? board_state.player_a : board_state.player_b;
                printf("Current turn: %s\n", current_str);
                printf("\n");
            }
        }
    }
    
    /* Send stop spectating message */
    msg_spectate_game_t stop_req;
    snprintf(stop_req.game_id, sizeof(stop_req.game_id), "%s", selected_game->game_id);
    session_send_message(client_state_get_session(), MSG_STOP_SPECTATE, &stop_req, sizeof(stop_req));
    
    /* Clear spectator state */
    spectator_state_clear();
    
    printf("\n👋 Stopped spectating.\n");
}
