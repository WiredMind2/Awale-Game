#ifndef PLAYER_H
#define PLAYER_H

#include "../common/types.h"
#include <time.h>

/* Player structure (extended player info) */
typedef struct {
    player_info_t info;
    bool connected;
    time_t connect_time;
    time_t last_activity;
    int games_played;
    int games_won;
} player_t;

/* Player management */
error_code_t player_init(player_t* player, const char* pseudo, const char* ip);
error_code_t player_copy(const player_t* src, player_t* dest);
bool player_equals(const player_t* p1, const player_t* p2);

/* Player validation */
bool player_is_valid_pseudo(const char* pseudo);
bool player_is_connected(const player_t* player);

/* Player statistics */
void player_update_stats(player_t* player, bool won);
void player_touch_activity(player_t* player);

#endif /* PLAYER_H */
