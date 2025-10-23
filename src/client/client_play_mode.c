/* Client Play Mode - Implementation
 * Interactive play mode for active games
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
    
    /* Play mode loop */
    bool playing = true;
    char game_id_copy[MAX_GAME_ID_LEN];
    char player_a_copy[MAX_PSEUDO_LEN];
    char player_b_copy[MAX_PSEUDO_LEN];
    player_id_t my_side = selected_game->my_side;
    
    snprintf(game_id_copy, MAX_GAME_ID_LEN, "%s", selected_game->game_id);
    snprintf(player_a_copy, MAX_PSEUDO_LEN, "%s", selected_game->player_a);
    snprintf(player_b_copy, MAX_PSEUDO_LEN, "%s", selected_game->player_b);
    
    printf("\n═══════════════════════════════════════════════════\n");
    printf("    PLAY MODE ACTIVE\n");
    printf("    Press 'm' + Enter to return to main menu\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    while (playing && client_state_is_running()) {
        /* Fetch current board state */
        msg_get_board_t board_req;
        memset(&board_req, 0, sizeof(board_req));
        snprintf(board_req.player_a, MAX_PSEUDO_LEN, "%s", player_a_copy);
        snprintf(board_req.player_b, MAX_PSEUDO_LEN, "%s", player_b_copy);
        
        error_code_t err = session_send_message(client_state_get_session(), MSG_GET_BOARD, &board_req, sizeof(board_req));
        if (err != SUCCESS) {
            printf("❌ Error fetching board: %s\n", error_to_string(err));
            break;
        }
        
        message_type_t type;
        msg_board_state_t board;
        size_t size;
        
        err = session_recv_message(client_state_get_session(), &type, &board, sizeof(board), &size);
        if (err != SUCCESS || type != MSG_BOARD_STATE) {
            printf("❌ Error receiving board state\n");
            break;
        }
        
        if (!board.exists) {
            printf("❌ Game no longer exists\n");
            active_games_remove(game_id_copy);
            break;
        }
        
        /* Display board */
        print_board(&board);
        
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
            
            /* Ask for rematch */
            printf("\nWould you like to challenge them again? (y/n): ");
            char rematch[10];
            if (read_line(rematch, 10) != NULL && (rematch[0] == 'y' || rematch[0] == 'Y')) {
                /* Determine opponent */
                const char* opponent = (my_side == PLAYER_A) ? player_b_copy : player_a_copy;
                
                msg_challenge_t challenge;
                memset(&challenge, 0, sizeof(challenge));
                snprintf(challenge.challenger, MAX_PSEUDO_LEN, "%s", client_state_get_pseudo());
                snprintf(challenge.opponent, MAX_PSEUDO_LEN, "%s", opponent);
                
                session_send_message(client_state_get_session(), MSG_CHALLENGE, &challenge, sizeof(challenge));
                printf("✓ Challenge sent to %s!\n", opponent);
            }
            
            active_games_remove(game_id_copy);
            break;
        }
        
        /* Check whose turn it is */
        if (board.current_player != my_side) {
            printf("\n⏳ Waiting for opponent's move...\n");
            printf("   (or type 'm' + Enter to return to main menu)\n");
            printf("> ");
            fflush(stdout);
            
            /* Wait for turn notification with timeout */
            if (!active_games_wait_for_turn(30)) {
                /* Timeout or user input - check for menu request */
                char input[10];
                fd_set rfds;
                struct timeval tv;
                FD_ZERO(&rfds);
                FD_SET(STDIN_FILENO, &rfds);
                tv.tv_sec = 0;
                tv.tv_usec = 100000; /* 100ms */
                
                if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0) {
                    if (fgets(input, sizeof(input), stdin)) {
                        if (input[0] == 'm' || input[0] == 'M') {
                            printf("Returning to main menu...\n");
                            playing = false;
                            break;
                        }
                    }
                }
                /* Continue loop to refresh board */
                continue;
            }
            
            /* Turn notification received, continue to show updated board */
            printf("\n🔔 It's your turn!\n\n");
            continue;
        }
        
        /* It's our turn - get legal moves */
        int start_pit = (my_side == PLAYER_A) ? 0 : 6;
        int end_pit = (my_side == PLAYER_A) ? 5 : 11;
        
        printf("\n🎯 YOUR TURN!\n");
        printf("   You can play from pits %d-%d\n", start_pit, end_pit);
        printf("   Legal pits (non-empty): ");
        
        bool has_legal_move = false;
        for (int i = start_pit; i <= end_pit; i++) {
            if (board.pits[i] > 0) {
                printf("%d ", i);
                has_legal_move = true;
            }
        }
        
        if (!has_legal_move) {
            printf("(none - game may end)");
        }
        printf("\n");
        
        printf("\nEnter pit number (%d-%d) or 'm' for menu: ", start_pit, end_pit);
        
        char input[32];
        if (read_line(input, 32) == NULL || strlen(input) == 0) {
            continue;
        }
        
        if (input[0] == 'm' || input[0] == 'M') {
            printf("Returning to main menu...\n");
            playing = false;
            break;
        }
        
        int pit = atoi(input);
        
        /* Validate move */
        if (pit < start_pit || pit > end_pit) {
            printf("❌ Invalid pit! You must choose from %d-%d\n", start_pit, end_pit);
            printf("Press Enter to continue...");
            getchar();
            continue;
        }
        
        if (board.pits[pit] == 0) {
            printf("❌ That pit is empty! Choose a pit with seeds.\n");
            printf("Press Enter to continue...");
            getchar();
            continue;
        }
        
        /* Send move */
        msg_play_move_t move;
        memset(&move, 0, sizeof(move));
        snprintf(move.game_id, MAX_GAME_ID_LEN, "%s", game_id_copy);
        snprintf(move.player, MAX_PSEUDO_LEN, "%s", client_state_get_pseudo());
        move.pit_index = pit;
        
        err = session_send_message(client_state_get_session(), MSG_PLAY_MOVE, &move, sizeof(move));
        if (err != SUCCESS) {
            printf("❌ Error sending move: %s\n", error_to_string(err));
            printf("Press Enter to continue...");
            getchar();
            continue;
        }
        
        /* Receive move result */
        msg_move_result_t result;
        err = session_recv_message(client_state_get_session(), &type, &result, sizeof(result), &size);
        if (err != SUCCESS || type != MSG_MOVE_RESULT) {
            printf("❌ Error receiving move result\n");
            printf("Press Enter to continue...");
            getchar();
            continue;
        }
        
        if (result.success) {
            printf("\n✅ Move played successfully!\n");
            if (result.seeds_captured > 0) {
                printf("⭐ You captured %d seeds!\n", result.seeds_captured);
            }
            
            if (result.game_over) {
                /* Game over will be shown on next loop iteration */
                printf("🏁 Game finished!\n");
            }
            
            printf("\nPress Enter to continue...");
            getchar();
        } else {
            printf("\n❌ Move failed: %s\n", result.message);
            printf("Press Enter to continue...");
            getchar();
        }
    }
    
    printf("\nExiting play mode...\n");
}
