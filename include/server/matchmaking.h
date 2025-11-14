#ifndef MATCHMAKING_H
#define MATCHMAKING_H

#include "../common/types.h"
#include <pthread.h>

#define MAX_CHALLENGES 100
#define MAX_PLAYERS 100

/* Challenge structure */
typedef struct {
    int64_t challenge_id;
    char challenger[MAX_PSEUDO_LEN];
    char opponent[MAX_PSEUDO_LEN];
    time_t created_at;
    bool active;
} challenge_t;

/* Player registry entry */
typedef struct {
    player_info_t info;
    bool connected;
    time_t last_seen;
} player_entry_t;

/* Matchmaking manager */
typedef struct {
    challenge_t challenges[MAX_CHALLENGES];
    int challenge_count;
    player_entry_t players[MAX_PLAYERS];
    int player_count;
    pthread_mutex_t lock;
    /* Rate limiting: last challenge time from challenger[i] to opponent[j] */
    time_t last_challenge_times[MAX_PLAYERS][MAX_PLAYERS];
    /* Decline tracking: count of declines from opponent[j] to challenger[i] */
    int decline_counts[MAX_PLAYERS][MAX_PLAYERS];
    /* Last decline time from opponent[j] to challenger[i] */
    time_t last_decline_times[MAX_PLAYERS][MAX_PLAYERS];
} matchmaking_t;

/* Matchmaking initialization */
error_code_t matchmaking_init(matchmaking_t* mm);
error_code_t matchmaking_destroy(matchmaking_t* mm);

/* Player management */
error_code_t matchmaking_add_player(matchmaking_t* mm, const char* pseudo, const char* ip);
error_code_t matchmaking_remove_player(matchmaking_t* mm, const char* pseudo);
error_code_t matchmaking_get_players(matchmaking_t* mm, player_info_t* players, int max_players, int* count);
bool matchmaking_player_exists(matchmaking_t* mm, const char* pseudo);

/* Challenge management */
error_code_t matchmaking_create_challenge(matchmaking_t* mm, const char* challenger,
                                         const char* opponent, bool* mutual_found);
error_code_t matchmaking_create_challenge_with_id(matchmaking_t* mm, const char* challenger,
                                                  const char* opponent, int64_t* challenge_id, bool* is_new);
error_code_t matchmaking_remove_challenge(matchmaking_t* mm, const char* challenger, const char* opponent);
error_code_t matchmaking_remove_challenge_by_id(matchmaking_t* mm, int64_t challenge_id);
error_code_t matchmaking_find_challenge_by_id(matchmaking_t* mm, int64_t challenge_id, challenge_t** challenge);
error_code_t matchmaking_get_challenges_for(matchmaking_t* mm, const char* player,
                                           char challengers[][MAX_PSEUDO_LEN], int max_count, int* count);
bool matchmaking_has_mutual_challenge(matchmaking_t* mm, const char* player_a, const char* player_b);

/* Challenge queries */
int matchmaking_count_challenges(matchmaking_t* mm);
int matchmaking_count_challenges_for(matchmaking_t* mm, const char* player);

/* Player statistics management */
error_code_t matchmaking_update_player_stats(matchmaking_t* mm, const char* pseudo,
                                             bool game_won, int score_earned);
error_code_t matchmaking_update_player_elo(matchmaking_t* mm, const char* winner_pseudo,
                                          const char* loser_pseudo);
error_code_t matchmaking_get_player_stats(matchmaking_t* mm, const char* pseudo,
                                         player_info_t* info_out);

/* Utility functions */
int matchmaking_get_player_index(matchmaking_t* mm, const char* pseudo);

/* Update player's bio lines */
error_code_t matchmaking_set_player_bio(matchmaking_t* mm, const char* pseudo, const char bio[][256], int lines);

/* Friend management */
error_code_t matchmaking_add_friend(matchmaking_t* mm, const char* pseudo, const char* friend_pseudo);
error_code_t matchmaking_remove_friend(matchmaking_t* mm, const char* pseudo, const char* friend_pseudo);
bool matchmaking_are_friends(matchmaking_t* mm, const char* pseudo1, const char* pseudo2);

/* Cleanup */
void matchmaking_cleanup_old_challenges(matchmaking_t* mm, int max_age_seconds);
void matchmaking_cleanup_expired_challenges(matchmaking_t* mm);

#endif /* MATCHMAKING_H */
