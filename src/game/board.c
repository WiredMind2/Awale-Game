#include "../../include/game/board.h"
#include "../../include/game/rules.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

error_code_t board_init(board_t* board) {
    if (!board) return ERR_INVALID_PARAM;
    
    // Initialize all pits with 4 seeds
    for (int i = 0; i < NUM_PITS; i++) {
        board->pits[i] = INITIAL_SEEDS_PER_PIT;
    }
    
    // Initialize scores
    board->scores[0] = 0;
    board->scores[1] = 0;
    
    // Player A starts
    board->current_player = PLAYER_A;
    board->state = GAME_STATE_IN_PROGRESS;
    board->winner = NO_WINNER;
    
    // Set timestamps
    board->created_at = time(NULL);
    board->last_move_at = time(NULL);
    
    return SUCCESS;
}

error_code_t board_reset(board_t* board) {
    return board_init(board);
}

error_code_t board_copy(const board_t* src, board_t* dest) {
    if (!src || !dest) return ERR_INVALID_PARAM;
    memcpy(dest, src, sizeof(board_t));
    return SUCCESS;
}

bool board_is_game_over(const board_t* board) {
    if (!board) return false;
    
    // Check if either player has won
    if (board->scores[0] >= WIN_SCORE || board->scores[1] >= WIN_SCORE) {
        return true;
    }
    
    // Check if either side is completely empty
    bool side_a_empty = board_is_side_empty(board, PLAYER_A);
    bool side_b_empty = board_is_side_empty(board, PLAYER_B);
    
    if (side_a_empty || side_b_empty) {
        return true;
    }
    
    return board->state == GAME_STATE_FINISHED || board->state == GAME_STATE_ABANDONED;
}

winner_t board_get_winner(const board_t* board) {
    if (!board) return NO_WINNER;
    
    if (!board_is_game_over(board)) {
        return NO_WINNER;
    }
    
    if (board->scores[0] > board->scores[1]) {
        return WINNER_A;
    } else if (board->scores[1] > board->scores[0]) {
        return WINNER_B;
    } else {
        return DRAW;
    }
}

int board_get_total_seeds(const board_t* board) {
    if (!board) return 0;
    
    int total = board->scores[0] + board->scores[1];
    for (int i = 0; i < NUM_PITS; i++) {
        total += board->pits[i];
    }
    return total;
}

bool board_is_side_empty(const board_t* board, player_id_t player) {
    if (!board) return true;
    
    int start = board_get_pit_start(player);
    int end = board_get_pit_end(player);
    
    for (int i = start; i <= end; i++) {
        if (board->pits[i] > 0) {
            return false;
        }
    }
    return true;
}

int board_get_side_seeds(const board_t* board, player_id_t player) {
    if (!board) return 0;
    
    int start = board_get_pit_start(player);
    int end = board_get_pit_end(player);
    int total = 0;
    
    for (int i = start; i <= end; i++) {
        total += board->pits[i];
    }
    return total;
}

error_code_t board_execute_move(board_t* board, player_id_t player, int pit_index, int* seeds_captured) {
    if (!board || !seeds_captured) return ERR_INVALID_PARAM;
    
    *seeds_captured = 0;
    
    // Validate the move first
    error_code_t result = rules_validate_move(board, player, pit_index);
    if (result != SUCCESS) {
        return result;
    }
    
    // Pick up seeds from the pit
    int seeds = board->pits[pit_index];
    board->pits[pit_index] = 0;
    
    // Sow seeds
    int last_pit = rules_sow_seeds(board, pit_index, seeds, true);
    
    // Capture seeds if applicable
    *seeds_captured = rules_capture_seeds(board, last_pit, player);
    board->scores[player] += *seeds_captured;
    
    // Check if opponent is starved and award remaining seeds to current player
    player_id_t opponent = (player == PLAYER_A) ? PLAYER_B : PLAYER_A;
    if (board_is_side_empty(board, opponent)) {
        int remaining = board_get_side_seeds(board, player);
        if (remaining > 0) {
            board->scores[player] += remaining;
            // Clear the player's side
            int start = board_get_pit_start(player);
            int end = board_get_pit_end(player);
            for (int i = start; i <= end; i++) {
                board->pits[i] = 0;
            }
        }
    }
    
    // Check for game over
    if (board->scores[0] >= WIN_SCORE || board->scores[1] >= WIN_SCORE) {
        board->state = GAME_STATE_FINISHED;
        board->winner = board_get_winner(board);
    } else if (board_is_side_empty(board, PLAYER_A) && board_is_side_empty(board, PLAYER_B)) {
        board->state = GAME_STATE_FINISHED;
        board->winner = board_get_winner(board);
    }
    
    // Switch turn
    if (board->state == GAME_STATE_IN_PROGRESS) {
        board->current_player = (board->current_player == PLAYER_A) ? PLAYER_B : PLAYER_A;
    }
    
    board->last_move_at = time(NULL);
    
    return SUCCESS;
}

bool board_is_pit_on_player_side(int pit_index, player_id_t player) {
    if (pit_index < 0 || pit_index >= NUM_PITS) return false;
    
    if (player == PLAYER_A) {
        return pit_index >= 0 && pit_index <= 5;
    } else {
        return pit_index >= 6 && pit_index <= 11;
    }
}

bool board_is_opponent_pit(int pit_index, player_id_t player) {
    if (pit_index < 0 || pit_index >= NUM_PITS) return false;
    return !board_is_pit_on_player_side(pit_index, player);
}

int board_get_pit_start(player_id_t player) {
    return (player == PLAYER_A) ? 0 : 6;
}

int board_get_pit_end(player_id_t player) {
    return (player == PLAYER_A) ? 5 : 11;
}

void board_print(const board_t* board) {
    if (!board) return;
    
    printf("\n");
    printf("  [%2d][%2d][%2d][%2d][%2d][%2d]  <- Player B (Score: %d)\n",
           board->pits[11], board->pits[10], board->pits[9],
           board->pits[8], board->pits[7], board->pits[6],
           board->scores[1]);
    printf("  [%2d][%2d][%2d][%2d][%2d][%2d]  <- Player A (Score: %d)\n",
           board->pits[0], board->pits[1], board->pits[2],
           board->pits[3], board->pits[4], board->pits[5],
           board->scores[0]);
    printf("  Current turn: Player %c\n", (board->current_player == PLAYER_A) ? 'A' : 'B');
    printf("\n");
}

void board_print_detailed(const board_t* board, const char* player_a_name, const char* player_b_name) {
    if (!board) return;
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("                    PLATEAU AWALE                          \n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Joueur B: %s (Score: %d)                    %s\n", 
           player_b_name ? player_b_name : "Player B", 
           board->scores[1],
           (board->current_player == PLAYER_B) ? "← À TOI!" : "");
    printf("\n");
    
    printf("   ┌────┬────┬────┬────┬────┬────┐\n");
    printf("   │%2d  │%2d  │%2d  │%2d  │%2d  │%2d  │\n",
           board->pits[11], board->pits[10], board->pits[9], 
           board->pits[8], board->pits[7], board->pits[6]);
    printf("   │ 11 │ 10 │ 9  │ 8  │ 7  │ 6  │\n");
    printf("   ├────┼────┼────┼────┼────┼────┤\n");
    printf("   │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │\n");
    printf("   │%2d  │%2d  │%2d  │%2d  │%2d  │%2d  │\n",
           board->pits[0], board->pits[1], board->pits[2], 
           board->pits[3], board->pits[4], board->pits[5]);
    printf("   └────┴────┴────┴────┴────┴────┘\n");
    printf("\n");
    printf("%s Joueur A: %s (Score: %d)\n", 
           (board->current_player == PLAYER_A) ? "À TOI! →" : "",
           player_a_name ? player_a_name : "Player A",
           board->scores[0]);
    printf("═══════════════════════════════════════════════════════════\n");
    
    if (board->state == GAME_STATE_FINISHED) {
        printf("🏁 PARTIE TERMINÉE - ");
        if (board->winner == WINNER_A) {
            printf("%s gagne!\n", player_a_name ? player_a_name : "Player A");
        } else if (board->winner == WINNER_B) {
            printf("%s gagne!\n", player_b_name ? player_b_name : "Player B");
        } else {
            printf("Match nul!\n");
        }
    } else {
        printf("Tour du joueur: %s\n", 
               (board->current_player == PLAYER_A) ? 
               (player_a_name ? player_a_name : "Player A") : 
               (player_b_name ? player_b_name : "Player B"));
    }
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
}
