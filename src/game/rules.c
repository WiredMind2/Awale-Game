#include "../../include/game/rules.h"
#include "../../include/game/board.h"
#include <string.h>
#include <stdio.h>

error_code_t rules_validate_move(const board_t* board, player_id_t player, int pit_index) {
    if (!board) return ERR_INVALID_PARAM;
    
    // Check if pit index is valid
    if (!rules_is_valid_pit_index(pit_index)) {
        return ERR_INVALID_MOVE;
    }
    
    // Check if it's the player's turn
    if (!rules_is_player_turn(board, player)) {
        return ERR_NOT_YOUR_TURN;
    }
    
    // Check if the pit is empty
    if (rules_is_pit_empty(board, pit_index)) {
        return ERR_EMPTY_PIT;
    }
    
    // Check if the pit is on the correct side
    if (!rules_is_correct_side(pit_index, player)) {
        return ERR_WRONG_SIDE;
    }
    
    // Check feeding rule: don't starve opponent if alternative exists
    if (rules_would_starve_opponent(board, player, pit_index)) {
        if (rules_has_feeding_alternative(board, player, pit_index)) {
            return ERR_STARVE_VIOLATION;
        }
    }
    
    return SUCCESS;
}

bool rules_is_valid_pit_index(int pit_index) {
    return pit_index >= 0 && pit_index < NUM_PITS;
}

bool rules_is_player_turn(const board_t* board, player_id_t player) {
    if (!board) return false;
    return board->current_player == player;
}

bool rules_is_pit_empty(const board_t* board, int pit_index) {
    if (!board || !rules_is_valid_pit_index(pit_index)) return true;
    return board->pits[pit_index] == 0;
}

bool rules_is_correct_side(int pit_index, player_id_t player) {
    return board_is_pit_on_player_side(pit_index, player);
}

bool rules_would_starve_opponent(const board_t* board, player_id_t player, int pit_index) {
    if (!board) return false;
    
    // Simulate the move
    board_t sim_board;
    int seeds_captured;
    error_code_t result = rules_simulate_move(board, player, pit_index, &sim_board, &seeds_captured);
    if (result != SUCCESS) return false;
    
    /* (no-op) */

    // Check if opponent side is empty after the move
    player_id_t opponent = (player == PLAYER_A) ? PLAYER_B : PLAYER_A;
    return board_is_side_empty(&sim_board, opponent);
}

bool rules_has_feeding_alternative(const board_t* board, player_id_t player, int pit_index) {
    if (!board) return false;
    
    int start = board_get_pit_start(player);
    int end = board_get_pit_end(player);
    
    // Check all other pits on player's side
    for (int i = start; i <= end; i++) {
        if (i == pit_index) continue;  // Skip the current pit
        if (board->pits[i] == 0) continue;  // Skip empty pits
        
        // Simulate this alternative move
        if (!rules_would_starve_opponent(board, player, i)) {
            return true;  // Found a feeding alternative
        }
    }
    
    return false;  // No feeding alternative found
}

error_code_t rules_simulate_move(const board_t* board, player_id_t player, int pit_index, 
                                 board_t* result_board, int* seeds_captured) {
    if (!board || !result_board || !seeds_captured) return ERR_INVALID_PARAM;
    
    // Copy the board
    board_copy(board, result_board);
    
    // Pick up seeds
    int seeds = result_board->pits[pit_index];
    result_board->pits[pit_index] = 0;
    
    // Sow seeds
    int last_pit = rules_sow_seeds(result_board, pit_index, seeds, true);
    
    // Capture seeds
    *seeds_captured = rules_capture_seeds(result_board, last_pit, player);
    
    return SUCCESS;
}

bool rules_check_win_condition(const board_t* board, winner_t* winner) {
    if (!board || !winner) return false;
    
    *winner = NO_WINNER;
    
    // Check if either player has won by score
    if (board->scores[0] >= WIN_SCORE) {
        *winner = WINNER_A;
        return true;
    }
    if (board->scores[1] >= WIN_SCORE) {
        *winner = WINNER_B;
        return true;
    }
    
    // Check if both sides are empty
    bool a_empty = board_is_side_empty(board, PLAYER_A);
    bool b_empty = board_is_side_empty(board, PLAYER_B);
    
    if (a_empty && b_empty) {
        *winner = board_get_winner(board);
        return true;
    }
    
    return false;
}

bool rules_can_player_move(const board_t* board, player_id_t player) {
    if (!board) return false;
    
    int start = board_get_pit_start(player);
    int end = board_get_pit_end(player);
    
    for (int i = start; i <= end; i++) {
        if (board->pits[i] > 0) {
            return true;
        }
    }
    
    return false;
}

int rules_sow_seeds(board_t* board, int start_pit, int seeds, bool skip_origin) {
    if (!board || seeds <= 0) return start_pit;
    
    int current_pit = start_pit;
    
    while (seeds > 0) {
        // Move to next pit (counterclockwise)
        current_pit = (current_pit + 1) % NUM_PITS;
        
        // Skip the origin pit on subsequent laps if requested
        if (skip_origin && current_pit == start_pit) {
            continue;
        }
        
        // Place one seed
        board->pits[current_pit]++;
        seeds--;
    }
    
    return current_pit;
}

int rules_capture_seeds(board_t* board, int last_pit, player_id_t player) {
    if (!board) return 0;
    
    int total_captured = 0;
    
    // Only capture if last seed landed in opponent's pit
    if (!board_is_opponent_pit(last_pit, player)) {
        return 0;
    }
    
    // Only capture if the pit now has 2 or 3 seeds
    if (board->pits[last_pit] != 2 && board->pits[last_pit] != 3) {
        return 0;
    }
    
    // Capture backwards from last pit while conditions are met
    int current_pit = last_pit;
    
    while (board_is_opponent_pit(current_pit, player) && 
           (board->pits[current_pit] == 2 || board->pits[current_pit] == 3)) {
        
        // Capture seeds from this pit
        total_captured += board->pits[current_pit];
        board->pits[current_pit] = 0;
        
        // Move backwards
        current_pit = (current_pit - 1 + NUM_PITS) % NUM_PITS;
    }
    
    return total_captured;
}
