#ifndef STORAGE_H
#define STORAGE_H

#include "../common/types.h"
#include "game_manager.h"
#include "matchmaking.h"

/* Storage paths */
#define STORAGE_DIR "./data"
#define GAMES_FILE "./data/games.dat"
#define PLAYERS_FILE "./data/players.dat"

/* Storage operations */
error_code_t storage_init(void);
error_code_t storage_cleanup(void);

/* Game persistence */
error_code_t storage_save_game(const game_instance_t* game);
error_code_t storage_load_game(const char* game_id, game_instance_t* game);
error_code_t storage_delete_game(const char* game_id);
error_code_t storage_load_all_games(game_manager_t* manager);

/* Saved games for review */
error_code_t storage_list_saved_games(int* count, char game_ids[][MAX_GAME_ID_LEN], int max_games);
error_code_t storage_load_saved_game(const char* game_id, game_instance_t* game);

/* Player persistence */
error_code_t storage_save_players(const matchmaking_t* mm);
error_code_t storage_load_players(matchmaking_t* mm);

/* Utility functions */
bool storage_directory_exists(const char* path);
error_code_t storage_create_directory(const char* path);
error_code_t storage_file_exists(const char* path, bool* exists);

#endif /* STORAGE_H */
