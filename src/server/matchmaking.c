#include "../../include/server/matchmaking.h"
#include "../../include/server/storage.h"
#include "../../include/game/elo.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Static counter for challenge ID generation */
static int64_t g_next_challenge_id = 1;

/* Helper function to get player index from pseudo */
static int get_player_index(matchmaking_t* mm, const char* pseudo) {
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            return i;
        }
    }
    return -1;
}


error_code_t matchmaking_init(matchmaking_t* mm) {
    if (!mm) return ERR_INVALID_PARAM;

    mm->challenge_count = 0;
    mm->player_count = 0;
    pthread_mutex_init(&mm->lock, NULL);
    memset(mm->last_challenge_times, 0, sizeof(mm->last_challenge_times));
    memset(mm->decline_counts, 0, sizeof(mm->decline_counts));
    memset(mm->last_decline_times, 0, sizeof(mm->last_decline_times));
    for (int i = 0; i < MAX_CHALLENGES; i++) {
        mm->challenges[i].active = false;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        mm->players[i].connected = false;
    }

    // Load persisted player data
    storage_load_players(mm);

    // Add AI bot as a permanent player
    matchmaking_add_player(mm, AI_BOT_PSEUDO, "127.0.0.1");

    return SUCCESS;
}

error_code_t matchmaking_destroy(matchmaking_t* mm) {
    if (!mm) return ERR_INVALID_PARAM;
    pthread_mutex_destroy(&mm->lock);
    return SUCCESS;
}

error_code_t matchmaking_add_player(matchmaking_t* mm, const char* pseudo, const char* ip) {
    if (!mm || !pseudo || !ip) return ERR_INVALID_PARAM;
    
    pthread_mutex_lock(&mm->lock);
    
    // Check if player already exists
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            mm->players[i].connected = true;
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }
    
    if (mm->player_count >= MAX_PLAYERS) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_MAX_CAPACITY;
    }
    
    player_entry_t* player = &mm->players[mm->player_count];
    strncpy(player->info.pseudo, pseudo, MAX_PSEUDO_LEN - 1);
    strncpy(player->info.ip, ip, MAX_IP_LEN - 1);
    player->connected = true;
    player->last_seen = time(NULL);
    player->info.first_seen = time(NULL);

    /* Initialize statistics */
    player->info.games_played = 0;
    player->info.games_won = 0;
    player->info.games_lost = 0;
    player->info.total_score = 0;
    player->info.elo_rating = ELO_DEFAULT_RATING;
    player->info.bio_lines = 0;
    player->info.friend_count = 0;
    mm->player_count++;
    
    pthread_mutex_unlock(&mm->lock);
    return SUCCESS;
}

error_code_t matchmaking_remove_player(matchmaking_t* mm, const char* pseudo) {
    if (!mm || !pseudo) return ERR_INVALID_PARAM;
    
    pthread_mutex_lock(&mm->lock);
    
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            mm->players[i].connected = false;
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&mm->lock);
    return SUCCESS;  /* Not an error if player not found */
}

error_code_t matchmaking_create_challenge(matchmaking_t* mm, const char* challenger,
                                          const char* opponent, bool* mutual_found) {
    if (!mm || !challenger || !opponent || !mutual_found) return ERR_INVALID_PARAM;

    *mutual_found = false;

    pthread_mutex_lock(&mm->lock);

    // Prevent AI vs AI games
    bool challenger_is_ai = (strcmp(challenger, AI_BOT_PSEUDO) == 0);
    bool opponent_is_ai = (strcmp(opponent, AI_BOT_PSEUDO) == 0);
    if (challenger_is_ai && opponent_is_ai) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_INVALID_PARAM; // Cannot challenge AI with AI
    }

    // Check for mutual challenge
    for (int i = 0; i < mm->challenge_count; i++) {
        if (!mm->challenges[i].active) continue;

        if (strcmp(mm->challenges[i].challenger, opponent) == 0 &&
            strcmp(mm->challenges[i].opponent, challenger) == 0) {
            *mutual_found = true;
            mm->challenges[i].active = false;
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }

    // Add new challenge
    if (mm->challenge_count >= MAX_CHALLENGES) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_MAX_CAPACITY;
    }

    for (int i = 0; i < MAX_CHALLENGES; i++) {
        if (!mm->challenges[i].active) {
            strncpy(mm->challenges[i].challenger, challenger, MAX_PSEUDO_LEN - 1);
            strncpy(mm->challenges[i].opponent, opponent, MAX_PSEUDO_LEN - 1);
            mm->challenges[i].created_at = time(NULL);
            mm->challenges[i].active = true;
            mm->challenge_count++;
            break;
        }
    }

    pthread_mutex_unlock(&mm->lock);
    return SUCCESS;
}

error_code_t matchmaking_get_players(matchmaking_t* mm, player_info_t* players, int max_players, int* count) {
    if (!mm || !players || !count) return ERR_INVALID_PARAM;
    
    pthread_mutex_lock(&mm->lock);
    
    *count = 0;
    for (int i = 0; i < mm->player_count && *count < max_players; i++) {
        if (mm->players[i].connected) {
            players[*count] = mm->players[i].info;
            (*count)++;
        }
    }
    
    pthread_mutex_unlock(&mm->lock);
    return SUCCESS;
}

bool matchmaking_player_exists(matchmaking_t* mm, const char* pseudo) {
    if (!mm || !pseudo) return false;
    
    pthread_mutex_lock(&mm->lock);
    
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            pthread_mutex_unlock(&mm->lock);
            return true;
        }
    }
    
    pthread_mutex_unlock(&mm->lock);
    return false;
}

error_code_t matchmaking_remove_challenge(matchmaking_t* mm, const char* challenger, const char* opponent) {
    if (!mm || !challenger || !opponent) return ERR_INVALID_PARAM;
    
    pthread_mutex_lock(&mm->lock);
    
    for (int i = 0; i < MAX_CHALLENGES; i++) {
        if (mm->challenges[i].active &&
            strcmp(mm->challenges[i].challenger, challenger) == 0 &&
            strcmp(mm->challenges[i].opponent, opponent) == 0) {
            mm->challenges[i].active = false;
            mm->challenge_count--;
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&mm->lock);
    return ERR_GAME_NOT_FOUND;  /* Challenge not found */
}

error_code_t matchmaking_update_player_stats(matchmaking_t* mm, const char* pseudo,
                                            bool game_won, int score_earned) {
    if (!mm || !pseudo) return ERR_INVALID_PARAM;

    pthread_mutex_lock(&mm->lock);

    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            mm->players[i].info.games_played++;
            if (game_won) {
                mm->players[i].info.games_won++;
            } else {
                mm->players[i].info.games_lost++;
            }
            mm->players[i].info.total_score += score_earned;

            // Save updated player data to disk
            storage_save_players(mm);

            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }

    pthread_mutex_unlock(&mm->lock);
    return ERR_PLAYER_NOT_FOUND;
}

error_code_t matchmaking_update_player_elo(matchmaking_t* mm, const char* winner_pseudo,
                                          const char* loser_pseudo) {
    if (!mm || !winner_pseudo || !loser_pseudo) return ERR_INVALID_PARAM;

    pthread_mutex_lock(&mm->lock);

    int winner_rating = ELO_DEFAULT_RATING;
    int loser_rating = ELO_DEFAULT_RATING;
    bool winner_found = false;
    bool loser_found = false;

    // Find winner's current rating
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, winner_pseudo) == 0) {
            winner_rating = mm->players[i].info.elo_rating;
            winner_found = true;
        } else if (strcmp(mm->players[i].info.pseudo, loser_pseudo) == 0) {
            loser_rating = mm->players[i].info.elo_rating;
            loser_found = true;
        }
    }

    if (!winner_found || !loser_found) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_PLAYER_NOT_FOUND;
    }

    // Calculate new ratings
    int new_winner_rating = elo_calculate_new_rating(winner_rating, loser_rating, true);
    int new_loser_rating = elo_calculate_new_rating(loser_rating, winner_rating, false);

    // Update ratings
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, winner_pseudo) == 0) {
            mm->players[i].info.elo_rating = new_winner_rating;
        } else if (strcmp(mm->players[i].info.pseudo, loser_pseudo) == 0) {
            mm->players[i].info.elo_rating = new_loser_rating;
        }
    }

    // Save updated player data to disk
    storage_save_players(mm);

    pthread_mutex_unlock(&mm->lock);
    return SUCCESS;
}

error_code_t matchmaking_create_challenge_with_id(matchmaking_t* mm, const char* challenger,
                                                  const char* opponent, int64_t* challenge_id, bool* is_new) {
    if (!mm || !challenger || !opponent || !challenge_id || !is_new) return ERR_INVALID_PARAM;

    pthread_mutex_lock(&mm->lock);

    // Prevent AI vs AI games
    bool challenger_is_ai = (strcmp(challenger, AI_BOT_PSEUDO) == 0);
    bool opponent_is_ai = (strcmp(opponent, AI_BOT_PSEUDO) == 0);
    if (challenger_is_ai && opponent_is_ai) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_INVALID_PARAM; // Cannot challenge AI with AI
    }

    int challenger_idx = get_player_index(mm, challenger);
    int opponent_idx = get_player_index(mm, opponent);
    if (challenger_idx == -1 || opponent_idx == -1) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_PLAYER_NOT_FOUND;
    }

    time_t now = time(NULL);

    // Rate limiting: 10s between same challenger->opponent
    if (now - mm->last_challenge_times[challenger_idx][opponent_idx] < 10) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_RATE_LIMITED;
    }

    // Decline tracking: reset counts after 5 minutes
    if (now - mm->last_decline_times[opponent_idx][challenger_idx] >= 300) {
        mm->decline_counts[opponent_idx][challenger_idx] = 0;
    }
    if (mm->decline_counts[opponent_idx][challenger_idx] >= 3) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_TOO_MANY_DECLINES;
    }

    // Check if challenge already exists
    for (int i = 0; i < MAX_CHALLENGES; i++) {
        if (mm->challenges[i].active &&
            strcmp(mm->challenges[i].challenger, challenger) == 0 &&
            strcmp(mm->challenges[i].opponent, opponent) == 0) {
            *challenge_id = mm->challenges[i].challenge_id;
            *is_new = false;
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }

    if (mm->challenge_count >= MAX_CHALLENGES) {
        pthread_mutex_unlock(&mm->lock);
        return ERR_MAX_CAPACITY;
    }

    for (int i = 0; i < MAX_CHALLENGES; i++) {
        if (!mm->challenges[i].active) {
            mm->challenges[i].challenge_id = g_next_challenge_id++;
            *challenge_id = mm->challenges[i].challenge_id;
            *is_new = true;
            strncpy(mm->challenges[i].challenger, challenger, MAX_PSEUDO_LEN - 1);
            strncpy(mm->challenges[i].opponent, opponent, MAX_PSEUDO_LEN - 1);
            mm->challenges[i].created_at = now;
            mm->challenges[i].active = true;
            mm->challenge_count++;
            mm->last_challenge_times[challenger_idx][opponent_idx] = now;
            break;
        }
    }

    pthread_mutex_unlock(&mm->lock);
    return SUCCESS;
}

error_code_t matchmaking_remove_challenge_by_id(matchmaking_t* mm, int64_t challenge_id) {
    if (!mm) return ERR_INVALID_PARAM;

    pthread_mutex_lock(&mm->lock);

    for (int i = 0; i < MAX_CHALLENGES; i++) {
        if (mm->challenges[i].active && mm->challenges[i].challenge_id == challenge_id) {
            mm->challenges[i].active = false;
            mm->challenge_count--;
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }

    pthread_mutex_unlock(&mm->lock);
    return ERR_GAME_NOT_FOUND;  // Challenge not found
}

error_code_t matchmaking_find_challenge_by_id(matchmaking_t* mm, int64_t challenge_id, challenge_t** challenge) {
    if (!mm || !challenge) return ERR_INVALID_PARAM;

    pthread_mutex_lock(&mm->lock);

    for (int i = 0; i < MAX_CHALLENGES; i++) {
        if (mm->challenges[i].active && mm->challenges[i].challenge_id == challenge_id) {
            *challenge = &mm->challenges[i];
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }

    pthread_mutex_unlock(&mm->lock);
    return ERR_GAME_NOT_FOUND;  // Challenge not found
}

void matchmaking_cleanup_expired_challenges(matchmaking_t* mm) {
    if (!mm) return;

    pthread_mutex_lock(&mm->lock);

    time_t now = time(NULL);
    const int CHALLENGE_TIMEOUT_SECONDS = 60;

    for (int i = 0; i < MAX_CHALLENGES; i++) {
        if (mm->challenges[i].active &&
            (now - mm->challenges[i].created_at) > CHALLENGE_TIMEOUT_SECONDS) {
            printf("Challenge %lld expired (challenger: %s, opponent: %s)\n",
                   (long long)mm->challenges[i].challenge_id,
                   mm->challenges[i].challenger,
                   mm->challenges[i].opponent);
            mm->challenges[i].active = false;
            mm->challenge_count--;
        }
    }

    pthread_mutex_unlock(&mm->lock);
}

error_code_t matchmaking_get_player_stats(matchmaking_t* mm, const char* pseudo, 
                                         player_info_t* info_out) {
    if (!mm || !pseudo || !info_out) return ERR_INVALID_PARAM;
    
    pthread_mutex_lock(&mm->lock);
    
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            memcpy(info_out, &mm->players[i].info, sizeof(player_info_t));
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&mm->lock);
    return ERR_PLAYER_NOT_FOUND;
}

error_code_t matchmaking_set_player_bio(matchmaking_t* mm, const char* pseudo, const char bio[][256], int lines) {
    if (!mm || !pseudo || !bio || lines < 0 || lines > 10) return ERR_INVALID_PARAM;
    pthread_mutex_lock(&mm->lock);
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            mm->players[i].info.bio_lines = lines;
            for (int b = 0; b < lines; b++) {
                snprintf(mm->players[i].info.bio[b], sizeof(mm->players[i].info.bio[b]), "%s", bio[b]);
            }
            storage_save_players(mm);
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }
    pthread_mutex_unlock(&mm->lock);
    return ERR_PLAYER_NOT_FOUND;
}

error_code_t matchmaking_add_friend(matchmaking_t* mm, const char* pseudo, const char* friend_pseudo) {
    if (!mm || !pseudo || !friend_pseudo) return ERR_INVALID_PARAM;

    /* Check if friend exists */
    if (!matchmaking_player_exists(mm, friend_pseudo)) {
        return ERR_PLAYER_NOT_FOUND;
    }

    /* Cannot add self as friend */
    if (strcmp(pseudo, friend_pseudo) == 0) {
        return ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&mm->lock);
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            /* Check if already friends */
            for (int f = 0; f < mm->players[i].info.friend_count; f++) {
                if (strcmp(mm->players[i].info.friends[f], friend_pseudo) == 0) {
                    pthread_mutex_unlock(&mm->lock);
                    return ERR_DUPLICATE;  /* Already friends */
                }
            }

            /* Check if friend list is full */
            if (mm->players[i].info.friend_count >= MAX_FRIENDS) {
                pthread_mutex_unlock(&mm->lock);
                return ERR_MAX_CAPACITY;
            }

            /* Add friend */
            strncpy(mm->players[i].info.friends[mm->players[i].info.friend_count],
                    friend_pseudo, MAX_PSEUDO_LEN - 1);
            mm->players[i].info.friends[mm->players[i].info.friend_count][MAX_PSEUDO_LEN - 1] = '\0';
            mm->players[i].info.friend_count++;

            storage_save_players(mm);
            pthread_mutex_unlock(&mm->lock);
            return SUCCESS;
        }
    }
    pthread_mutex_unlock(&mm->lock);
    return ERR_PLAYER_NOT_FOUND;
}

error_code_t matchmaking_remove_friend(matchmaking_t* mm, const char* pseudo, const char* friend_pseudo) {
    if (!mm || !pseudo || !friend_pseudo) return ERR_INVALID_PARAM;

    pthread_mutex_lock(&mm->lock);
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo) == 0) {
            /* Find and remove friend */
            for (int f = 0; f < mm->players[i].info.friend_count; f++) {
                if (strcmp(mm->players[i].info.friends[f], friend_pseudo) == 0) {
                    /* Shift remaining friends */
                    for (int j = f; j < mm->players[i].info.friend_count - 1; j++) {
                        strcpy(mm->players[i].info.friends[j], mm->players[i].info.friends[j + 1]);
                    }
                    mm->players[i].info.friend_count--;

                    storage_save_players(mm);
                    pthread_mutex_unlock(&mm->lock);
                    return SUCCESS;
                }
            }
            pthread_mutex_unlock(&mm->lock);
            return ERR_PLAYER_NOT_FOUND;  /* Friend not found in list */
        }
    }
    pthread_mutex_unlock(&mm->lock);
    return ERR_PLAYER_NOT_FOUND;
}

bool matchmaking_are_friends(matchmaking_t* mm, const char* pseudo1, const char* pseudo2) {
    if (!mm || !pseudo1 || !pseudo2) return false;

    pthread_mutex_lock(&mm->lock);
    for (int i = 0; i < mm->player_count; i++) {
        if (strcmp(mm->players[i].info.pseudo, pseudo1) == 0) {
            for (int f = 0; f < mm->players[i].info.friend_count; f++) {
                if (strcmp(mm->players[i].info.friends[f], pseudo2) == 0) {
                    pthread_mutex_unlock(&mm->lock);
                    return true;
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&mm->lock);
    return false;
}
