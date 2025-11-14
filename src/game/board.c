#include "../../include/game/board.h"
#include "../../include/game/rules.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../../include/client/client_ui_strings.h"

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

    winner_t winner;
    if (rules_check_win_condition(board, &winner)) {
        return true;
    }

    return board->state == GAME_STATE_FINISHED || board->state == GAME_STATE_ABANDONED;
}

winner_t board_get_winner(const board_t* board) {
    if (!board) return NO_WINNER;

    if (!board_is_game_over(board)) {
        return NO_WINNER;
    }

    /* When the game ends due to starvation (one side empty), the remaining
     * seeds on the board belong to the side that still has seeds. To decide
     * the winner consistently we compute the final totals as current score
     * plus remaining seeds on each player's side, without mutating the
     * board. */
    int total_a = board->scores[0] + board_get_side_seeds(board, PLAYER_A);
    int total_b = board->scores[1] + board_get_side_seeds(board, PLAYER_B);

    if (total_a > total_b) {
        return WINNER_A;
    } else if (total_b > total_a) {
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
    
    // Note: Do NOT automatically award remaining seeds to the mover when the
    // opponent side becomes empty after this move. Awarding remaining seeds is
    // handled by end-of-game logic (both sides empty or explicit game end).
    
    // Check for game over
    winner_t winner;
    if (rules_check_win_condition(board, &winner)) {
        board->state = GAME_STATE_FINISHED;
        board->winner = winner;
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
