#include "../../include/server/game_manager.h"
#include <string.h>
#include <stdio.h>

error_code_t game_manager_init(game_manager_t* manager) {
    if (!manager) return ERR_INVALID_PARAM;
    
    manager->game_count = 0;
    pthread_mutex_init(&manager->lock, NULL);
    
    for (int i = 0; i < MAX_GAMES; i++) {
        manager->games[i].active = false;
        pthread_mutex_init(&manager->games[i].lock, NULL);
    }
    
    return SUCCESS;
}

error_code_t game_manager_destroy(game_manager_t* manager) {
    if (!manager) return ERR_INVALID_PARAM;
    
    pthread_mutex_destroy(&manager->lock);
    
    for (int i = 0; i < MAX_GAMES; i++) {
        pthread_mutex_destroy(&manager->games[i].lock);
    }
    
    return SUCCESS;
}

error_code_t game_manager_create_game(game_manager_t* manager, const char* player_a, 
                                     const char* player_b, char* game_id_out) {
    if (!manager || !player_a || !player_b) return ERR_INVALID_PARAM;
    
    pthread_mutex_lock(&manager->lock);
    
    if (manager->game_count >= MAX_GAMES) {
        pthread_mutex_unlock(&manager->lock);
        return ERR_MAX_CAPACITY;
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!manager->games[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        pthread_mutex_unlock(&manager->lock);
        return ERR_MAX_CAPACITY;
    }
    
    game_instance_t* game = &manager->games[slot];
    
    // Generate game ID
    game_manager_generate_id(player_a, player_b, game->game_id);
    
    // Set players
    strncpy(game->player_a, player_a, MAX_PSEUDO_LEN - 1);
    strncpy(game->player_b, player_b, MAX_PSEUDO_LEN - 1);
    
    // Initialize board
    board_init(&game->board);
    
    game->active = true;
    manager->game_count++;
    
    if (game_id_out) {
        strncpy(game_id_out, game->game_id, MAX_GAME_ID_LEN - 1);
    }
    
    pthread_mutex_unlock(&manager->lock);
    return SUCCESS;
}

void game_manager_generate_id(const char* player_a, const char* player_b, char* game_id) {
    snprintf(game_id, MAX_GAME_ID_LEN, "%s-vs-%s", player_a, player_b);
}

game_instance_t* game_manager_find_game(game_manager_t* manager, const char* game_id) {
    if (!manager || !game_id) return NULL;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (manager->games[i].active && strcmp(manager->games[i].game_id, game_id) == 0) {
            return &manager->games[i];
        }
    }
    
    return NULL;
}

game_instance_t* game_manager_find_game_by_players(game_manager_t* manager, 
                                                   const char* player_a, const char* player_b) {
    if (!manager || !player_a || !player_b) return NULL;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!manager->games[i].active) continue;
        
        if ((strcmp(manager->games[i].player_a, player_a) == 0 && 
             strcmp(manager->games[i].player_b, player_b) == 0) ||
            (strcmp(manager->games[i].player_a, player_b) == 0 && 
             strcmp(manager->games[i].player_b, player_a) == 0)) {
            return &manager->games[i];
        }
    }
    
    return NULL;
}

error_code_t game_manager_play_move(game_manager_t* manager, const char* game_id, 
                                   const char* player, int pit_index, int* seeds_captured) {
    if (!manager || !game_id || !player || !seeds_captured) return ERR_INVALID_PARAM;
    
    game_instance_t* game = game_manager_find_game(manager, game_id);
    if (!game) return ERR_GAME_NOT_FOUND;
    
    pthread_mutex_lock(&game->lock);
    
    // Determine player ID
    player_id_t player_id;
    if (strcmp(game->player_a, player) == 0) {
        player_id = PLAYER_A;
    } else if (strcmp(game->player_b, player) == 0) {
        player_id = PLAYER_B;
    } else {
        pthread_mutex_unlock(&game->lock);
        return ERR_PLAYER_NOT_FOUND;
    }
    
    // Execute move
    error_code_t result = board_execute_move(&game->board, player_id, pit_index, seeds_captured);
    
    pthread_mutex_unlock(&game->lock);
    return result;
}

error_code_t game_manager_get_board(game_manager_t* manager, const char* game_id, board_t* board_out) {
    if (!manager || !game_id || !board_out) return ERR_INVALID_PARAM;
    
    game_instance_t* game = game_manager_find_game(manager, game_id);
    if (!game) return ERR_GAME_NOT_FOUND;
    
    pthread_mutex_lock(&game->lock);
    board_copy(&game->board, board_out);
    pthread_mutex_unlock(&game->lock);
    
    return SUCCESS;
}

int game_manager_count_active_games(game_manager_t* manager) {
    if (!manager) return 0;
    return manager->game_count;
}

bool game_manager_is_player_in_game(game_manager_t* manager, const char* player) {
    if (!manager || !player) return false;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!manager->games[i].active) continue;
        
        if (strcmp(manager->games[i].player_a, player) == 0 ||
            strcmp(manager->games[i].player_b, player) == 0) {
            return true;
        }
    }
    
    return false;
}
