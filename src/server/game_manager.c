#include "../../include/server/game_manager.h"
#include "../../include/server/storage.h"
#include "../../include/game/ai.h"
#include "../../include/common/types.h"
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
    
    // Load persisted games
    storage_load_all_games(manager);
    
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
    
    // Initialize spectators
    game->spectator_count = 0;
    
    game->active = true;
    manager->game_count++;
    
    if (game_id_out) {
        strncpy(game_id_out, game->game_id, MAX_GAME_ID_LEN - 1);
    }
    
    pthread_mutex_unlock(&manager->lock);
    return SUCCESS;
}

error_code_t game_manager_remove_game(game_manager_t* manager, const char* game_id) {
    if (!manager || !game_id) return ERR_INVALID_PARAM;

    pthread_mutex_lock(&manager->lock);

    game_instance_t* game = game_manager_find_game(manager, game_id);
    if (!game) {
        pthread_mutex_unlock(&manager->lock);
        return ERR_GAME_NOT_FOUND;
    }

    // Keep saved game file for review when game ends
    // storage_delete_game(game->game_id);  // Commented out to preserve completed games

    // Mark game as inactive
    game->active = false;
    manager->game_count--;

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
                                     const char* player, int pit_index, int* seeds_captured, int* ai_seeds_captured_out) {
    if (!manager || !game_id || !player || !seeds_captured || !ai_seeds_captured_out) return ERR_INVALID_PARAM;

    *ai_seeds_captured_out = -1;  /* Initialize to indicate no AI move */

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

    // Save game state after successful move
    if (result == SUCCESS) {
        storage_save_game(game);

        /* If game is still active and it's now AI's turn, make AI move */
        if (game->board.state == GAME_STATE_IN_PROGRESS) {
            const char* next_player;
            if (game->board.current_player == PLAYER_A) {
                next_player = game->player_a;
            } else {
                next_player = game->player_b;
            }

            if (game_manager_is_ai_player(next_player)) {
                /* Make AI move */
                int ai_seeds_captured;
                error_code_t ai_result = game_manager_make_ai_move(manager, game_id, &ai_seeds_captured);
                if (ai_result == SUCCESS) {
                    /* AI move was made, seeds captured is in ai_seeds_captured */
                    *ai_seeds_captured_out = ai_seeds_captured;
                }
            }
        }
    }

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

int game_manager_get_active_games(game_manager_t* manager, game_info_t* games_out, int max_games) {
    if (!manager || !games_out) return 0;

    int count = 0;
    for (int i = 0; i < MAX_GAMES && count < max_games; i++) {
        if (manager->games[i].active) {
            snprintf(games_out[count].game_id, MAX_GAME_ID_LEN, "%s", manager->games[i].game_id);
            snprintf(games_out[count].player_a, MAX_PSEUDO_LEN, "%s", manager->games[i].player_a);
            snprintf(games_out[count].player_b, MAX_PSEUDO_LEN, "%s", manager->games[i].player_b);
            games_out[count].spectator_count = manager->games[i].spectator_count;
            games_out[count].state = manager->games[i].board.state;
            count++;
        }
    }

    return count;
}

int game_manager_get_player_games(game_manager_t* manager, const char* player, game_info_t* games_out, int max_games) {
    if (!manager || !player || !games_out) return 0;

    int count = 0;
    for (int i = 0; i < MAX_GAMES && count < max_games; i++) {
        if (manager->games[i].active &&
            (strcmp(manager->games[i].player_a, player) == 0 ||
             strcmp(manager->games[i].player_b, player) == 0)) {
            snprintf(games_out[count].game_id, MAX_GAME_ID_LEN, "%s", manager->games[i].game_id);
            snprintf(games_out[count].player_a, MAX_PSEUDO_LEN, "%s", manager->games[i].player_a);
            snprintf(games_out[count].player_b, MAX_PSEUDO_LEN, "%s", manager->games[i].player_b);
            games_out[count].spectator_count = manager->games[i].spectator_count;
            games_out[count].state = manager->games[i].board.state;
            count++;
        }
    }

    return count;
}

error_code_t game_manager_add_spectator(game_manager_t* manager, const char* game_id, const char* spectator) {
    if (!manager || !game_id || !spectator) return ERR_INVALID_PARAM;
    
    game_instance_t* game = game_manager_find_game(manager, game_id);
    if (!game) return ERR_GAME_NOT_FOUND;
    
    pthread_mutex_lock(&game->lock);
    
    /* Check if already spectating */
    for (int i = 0; i < game->spectator_count; i++) {
        if (strcmp(game->spectators[i], spectator) == 0) {
            pthread_mutex_unlock(&game->lock);
            return SUCCESS;  /* Already spectating */
        }
    }
    
    /* Check capacity */
    if (game->spectator_count >= MAX_SPECTATORS_PER_GAME) {
        pthread_mutex_unlock(&game->lock);
        return ERR_MAX_CAPACITY;
    }
    
    /* Add spectator */
    snprintf(game->spectators[game->spectator_count], MAX_PSEUDO_LEN, "%s", spectator);
    game->spectator_count++;
    
    pthread_mutex_unlock(&game->lock);
    return SUCCESS;
}

error_code_t game_manager_remove_spectator(game_manager_t* manager, const char* game_id, const char* spectator) {
    if (!manager || !game_id || !spectator) return ERR_INVALID_PARAM;
    
    game_instance_t* game = game_manager_find_game(manager, game_id);
    if (!game) return ERR_GAME_NOT_FOUND;
    
    pthread_mutex_lock(&game->lock);
    
    /* Find and remove spectator */
    for (int i = 0; i < game->spectator_count; i++) {
        if (strcmp(game->spectators[i], spectator) == 0) {
            /* Shift remaining spectators */
            for (int j = i; j < game->spectator_count - 1; j++) {
                snprintf(game->spectators[j], MAX_PSEUDO_LEN, "%s", game->spectators[j + 1]);
            }
            game->spectator_count--;
            pthread_mutex_unlock(&game->lock);
            return SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&game->lock);
    return ERR_PLAYER_NOT_FOUND;  /* Spectator not found */
}

int game_manager_get_spectator_count(game_manager_t* manager, const char* game_id) {
    if (!manager || !game_id) return 0;

    game_instance_t* game = game_manager_find_game(manager, game_id);
    if (!game) return 0;

    pthread_mutex_lock(&game->lock);
    int count = game->spectator_count;
    pthread_mutex_unlock(&game->lock);

    return count;
}

/* Check if a player is the AI bot */
bool game_manager_is_ai_player(const char* player) {
    return player && strcmp(player, AI_BOT_PSEUDO) == 0;
}

/* Make AI move if it's AI's turn */
error_code_t game_manager_make_ai_move(game_manager_t* manager, const char* game_id, int* seeds_captured_out) {
    if (!manager || !game_id || !seeds_captured_out) return ERR_INVALID_PARAM;

    game_instance_t* game = game_manager_find_game(manager, game_id);
    if (!game) return ERR_GAME_NOT_FOUND;

    pthread_mutex_lock(&game->lock);

    /* Check if it's AI's turn */
    const char* current_player_pseudo;
    if (game->board.current_player == PLAYER_A) {
        current_player_pseudo = game->player_a;
    } else {
        current_player_pseudo = game->player_b;
    }

    if (!game_manager_is_ai_player(current_player_pseudo)) {
        pthread_mutex_unlock(&game->lock);
        return ERR_NOT_YOUR_TURN; /* Not AI's turn */
    }

    /* Get AI move - use medium difficulty by default */
    ai_move_t ai_move;
    error_code_t err = ai_get_best_move(&game->board, game->board.current_player,
                                       AI_DIFFICULTY_MEDIUM, &ai_move);
    if (err != SUCCESS) {
        pthread_mutex_unlock(&game->lock);
        return err;
    }

    /* Execute the AI move */
    int seeds_captured;
    err = board_execute_move(&game->board, game->board.current_player, ai_move.pit_index, &seeds_captured);
    if (err != SUCCESS) {
        pthread_mutex_unlock(&game->lock);
        return err;
    }

    /* Save game state */
    storage_save_game(game);

    printf("AI move: %s played pit %d in %s (captured: %d)\n",
           current_player_pseudo, ai_move.pit_index, game_id, seeds_captured);

    *seeds_captured_out = seeds_captured;

    pthread_mutex_unlock(&game->lock);
    return SUCCESS;
}
