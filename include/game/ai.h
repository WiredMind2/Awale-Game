#ifndef AI_H
#define AI_H

#include "../common/types.h"
#include "board.h"

/* AI difficulty levels */
typedef enum {
    AI_DIFFICULTY_EASY = 0,    /* Search depth 2 */
    AI_DIFFICULTY_MEDIUM = 1,  /* Search depth 4 */
    AI_DIFFICULTY_HARD = 2     /* Search depth 6 */
} ai_difficulty_t;

/* AI move result */
typedef struct {
    int pit_index;
    int evaluation_score;
} ai_move_t;

/* AI functions */
error_code_t ai_get_best_move(const board_t* board, player_id_t ai_player,
                             ai_difficulty_t difficulty, ai_move_t* result);

#endif /* AI_H */