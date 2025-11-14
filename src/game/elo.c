#include "../../include/game/elo.h"
#include <math.h>

int elo_calculate_new_rating(int current_rating, int opponent_rating, bool game_won) {
    /* Calculate expected score */
    double rating_diff = (double)(opponent_rating - current_rating) / 400.0;
    double expected_score = 1.0 / (1.0 + pow(10.0, rating_diff));

    /* Actual score: 1 for win, 0 for loss */
    double actual_score = game_won ? 1.0 : 0.0;

    /* Calculate rating change */
    double rating_change = ELO_K_FACTOR * (actual_score - expected_score);

    /* Return new rating (rounded to nearest integer) */
    return current_rating + (int)(rating_change + 0.5);
}