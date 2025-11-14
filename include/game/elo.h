#ifndef ELO_H
#define ELO_H

#include <stdint.h>
#include <stdbool.h>

/* Elo rating constants */
#define ELO_DEFAULT_RATING 1200
#define ELO_K_FACTOR 32

/* Calculate new Elo rating for a player after a game
 * @param current_rating: Current Elo rating of the player
 * @param opponent_rating: Current Elo rating of the opponent
 * @param game_won: True if the player won, false otherwise
 * @return: New Elo rating
 */
int elo_calculate_new_rating(int current_rating, int opponent_rating, bool game_won);

#endif /* ELO_H */