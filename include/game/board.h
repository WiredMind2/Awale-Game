#ifndef BOARD_H
#define BOARD_H

#include "../common/types.h"
#include <time.h>

/* Board structure */
typedef struct {
    int pits[NUM_PITS];           /* 12 pits */
    int scores[2];                /* Scores for player A and B */
    player_id_t current_player;   /* Whose turn it is */
    game_state_t state;           /* Current game state */
    winner_t winner;              /* Winner (if game is over) */
    time_t created_at;            /* When the game was created */
    time_t last_move_at;          /* Last move timestamp */
} board_t;

/* Board initialization and management */
error_code_t board_init(board_t* board);
error_code_t board_reset(board_t* board);
error_code_t board_copy(const board_t* src, board_t* dest);

/* Board queries */
bool board_is_game_over(const board_t* board);
winner_t board_get_winner(const board_t* board);
int board_get_total_seeds(const board_t* board);
bool board_is_side_empty(const board_t* board, player_id_t player);
int board_get_side_seeds(const board_t* board, player_id_t player);

/* Move execution */
error_code_t board_execute_move(board_t* board, player_id_t player, int pit_index, int* seeds_captured);

/* Helper functions */
bool board_is_pit_on_player_side(int pit_index, player_id_t player);
bool board_is_opponent_pit(int pit_index, player_id_t player);
int board_get_pit_start(player_id_t player);
int board_get_pit_end(player_id_t player);

#endif /* BOARD_H */
