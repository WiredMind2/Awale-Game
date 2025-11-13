#ifndef RULES_H
#define RULES_H

#include "../common/types.h"
#include "board.h"

/* Move validation */
error_code_t rules_validate_move(const board_t* board, player_id_t player, int pit_index);

/* Check if a move is legal */
bool rules_is_valid_pit_index(int pit_index);
bool rules_is_player_turn(const board_t* board, player_id_t player);
bool rules_is_pit_empty(const board_t* board, int pit_index);
bool rules_is_correct_side(int pit_index, player_id_t player);

/* Feeding rule: check if a move would starve the opponent */
bool rules_would_starve_opponent(const board_t* board, player_id_t player, int pit_index);
bool rules_has_feeding_alternative(const board_t* board, player_id_t player, int pit_index);

/* Simulate a move without modifying the board */
error_code_t rules_simulate_move(const board_t* board, player_id_t player, int pit_index, 
                                 board_t* result_board, int* seeds_captured);

/* Check for game ending conditions */
bool rules_check_win_condition(const board_t* board, winner_t* winner);
bool rules_can_player_move(const board_t* board, player_id_t player);

/* Check if a player can feed another player */
bool rules_can_feed(const board_t* board, player_id_t feeder, player_id_t feedee);

/* Helper for sowing mechanics */
int rules_sow_seeds(board_t* board, int start_pit, int seeds, bool skip_origin);

/* Capture mechanics */
int rules_capture_seeds(board_t* board, int last_pit, player_id_t player);

#endif /* RULES_H */
