#include "../../include/game/ai.h"
#include "../../include/game/rules.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* Get search depth based on difficulty */
static int get_search_depth(ai_difficulty_t difficulty) {
    switch (difficulty) {
        case AI_DIFFICULTY_EASY:   return 2;
        case AI_DIFFICULTY_MEDIUM: return 4;
        case AI_DIFFICULTY_HARD:   return 6;
        default: return 4; /* Default to medium */
    }
}

/* Evaluate board position for a player */
/* Higher score is better for the evaluating player */
static int evaluate_position(const board_t* board, player_id_t player) {
    if (!board) return 0;

    /* Game over check */
    winner_t winner;
    if (rules_check_win_condition(board, &winner)) {
        if (winner == (winner_t)player) {
            return 10000; /* Win */
        } else if (winner == DRAW) {
            return 0; /* Draw */
        } else {
            return -10000; /* Loss */
        }
    }

    /* Seed count evaluation */
    int my_score = board->scores[player];
    int opponent_score = board->scores[player == PLAYER_A ? PLAYER_B : PLAYER_A];
    int score_diff = my_score - opponent_score;

    /* Positional heuristics */
    int my_side_seeds = board_get_side_seeds(board, player);
    int opponent_side_seeds = board_get_side_seeds(board, player == PLAYER_A ? PLAYER_B : PLAYER_A);

    /* Prefer having more seeds on own side */
    int positional_score = my_side_seeds - opponent_side_seeds;

    /* Bonus for controlling the center pits (pits 3-4 on each side) */
    int center_bonus = 0;
    int start_pit = board_get_pit_start(player);
    for (int i = 0; i < PITS_PER_PLAYER; i++) {
        int pit_idx = start_pit + i;
        if (i >= 2 && i <= 3) { /* Center pits */
            center_bonus += board->pits[pit_idx] * 2;
        }
    }

    /* Penalty for leaving opponent with captures */
    int capture_penalty = 0;
    int opp_start = board_get_pit_start(player == PLAYER_A ? PLAYER_B : PLAYER_A);
    for (int i = 0; i < PITS_PER_PLAYER; i++) {
        int pit_idx = opp_start + i;
        if (board->pits[pit_idx] == 2 || board->pits[pit_idx] == 3) {
            capture_penalty -= board->pits[pit_idx] * 3; /* Strong penalty */
        }
    }

    /* Combine all factors */
    int total_score = score_diff * 10 + positional_score + center_bonus + capture_penalty;

    return total_score;
}

/* Minimax with alpha-beta pruning */
static int minimax(const board_t* board, player_id_t ai_player, int depth,
                   int alpha, int beta, bool maximizing) {
    /* Terminal node or depth limit reached */
    if (depth == 0 || board_is_game_over(board)) {
        return evaluate_position(board, ai_player);
    }

    /* Get possible moves */
    int start_pit = board_get_pit_start(board->current_player);
    int end_pit = board_get_pit_end(board->current_player);

    if (maximizing) {
        int max_eval = INT_MIN;

        for (int pit = start_pit; pit <= end_pit; pit++) {
            /* Check if move is valid */
            if (rules_validate_move(board, board->current_player, pit) != SUCCESS) {
                continue;
            }

            /* Simulate move */
            board_t sim_board;
            int seeds_captured;
            if (rules_simulate_move(board, board->current_player, pit, &sim_board, &seeds_captured) != SUCCESS) {
                continue;
            }

            /* Recursive call */
            int eval = minimax(&sim_board, ai_player, depth - 1, alpha, beta, false);

            if (eval > max_eval) {
                max_eval = eval;
            }

            if (eval > alpha) {
                alpha = eval;
            }

            /* Alpha-beta pruning */
            if (beta <= alpha) {
                break;
            }
        }

        return max_eval;
    } else {
        int min_eval = INT_MAX;

        for (int pit = start_pit; pit <= end_pit; pit++) {
            /* Check if move is valid */
            if (rules_validate_move(board, board->current_player, pit) != SUCCESS) {
                continue;
            }

            /* Simulate move */
            board_t sim_board;
            int seeds_captured;
            if (rules_simulate_move(board, board->current_player, pit, &sim_board, &seeds_captured) != SUCCESS) {
                continue;
            }

            /* Recursive call */
            int eval = minimax(&sim_board, ai_player, depth - 1, alpha, beta, true);

            if (eval < min_eval) {
                min_eval = eval;
            }

            if (eval < beta) {
                beta = eval;
            }

            /* Alpha-beta pruning */
            if (beta <= alpha) {
                break;
            }
        }

        return min_eval;
    }
}

/* Get the best move for AI */
error_code_t ai_get_best_move(const board_t* board, player_id_t ai_player,
                             ai_difficulty_t difficulty, ai_move_t* result) {
    if (!board || !result) return ERR_INVALID_PARAM;

    int depth = get_search_depth(difficulty);
    int best_pit = -1;
    int best_score = INT_MIN;

    /* Get possible moves */
    int start_pit = board_get_pit_start(ai_player);
    int end_pit = board_get_pit_end(ai_player);

    for (int pit = start_pit; pit <= end_pit; pit++) {
        /* Check if move is valid */
        if (rules_validate_move(board, ai_player, pit) != SUCCESS) {
            continue;
        }

        /* Simulate move */
        board_t sim_board;
        int seeds_captured;
        if (rules_simulate_move(board, ai_player, pit, &sim_board, &seeds_captured) != SUCCESS) {
            continue;
        }

        /* Evaluate move using minimax */
        int eval = minimax(&sim_board, ai_player, depth - 1, INT_MIN, INT_MAX, false);

        /* Update best move */
        if (eval > best_score) {
            best_score = eval;
            best_pit = pit;
        }
    }

    if (best_pit == -1) {
        return ERR_INVALID_MOVE; /* No valid moves */
    }

    result->pit_index = best_pit;
    result->evaluation_score = best_score;

    return SUCCESS;
}