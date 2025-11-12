#include "../../include/server/matchmaking.h"
#include "../../include/server/storage.h"
#include <string.h>
#include <stdio.h>

error_code_t matchmaking_init(matchmaking_t* mm) {
    if (!mm) return ERR_INVALID_PARAM;
    
    mm->challenge_count = 0;
    mm->player_count = 0;
    pthread_mutex_init(&mm->lock, NULL);
    
    for (int i = 0; i < MAX_CHALLENGES; i++) {
        mm->challenges[i].active = false;
    }
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        mm->players[i].connected = false;
    }
    
    // Load persisted player data
    storage_load_players(mm);
    
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
            mm->players[i].last_seen = time(NULL);
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
