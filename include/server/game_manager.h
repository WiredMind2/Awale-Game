#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "../common/types.h"
#include "../game/board.h"
#include <pthread.h>

#define MAX_GAMES 100

/* Game instance */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char player_a[MAX_PSEUDO_LEN];
    char player_b[MAX_PSEUDO_LEN];
    board_t board;
    bool active;
    pthread_mutex_t lock;
} game_instance_t;

/* Game manager */
typedef struct {
    game_instance_t games[MAX_GAMES];
    int game_count;
    pthread_mutex_t lock;
} game_manager_t;

/* Game manager initialization */
error_code_t game_manager_init(game_manager_t* manager);
error_code_t game_manager_destroy(game_manager_t* manager);

/* Game creation and destruction */
error_code_t game_manager_create_game(game_manager_t* manager, const char* player_a, 
                                     const char* player_b, char* game_id_out);
error_code_t game_manager_remove_game(game_manager_t* manager, const char* game_id);

/* Game lookup */
game_instance_t* game_manager_find_game(game_manager_t* manager, const char* game_id);
game_instance_t* game_manager_find_game_by_players(game_manager_t* manager, 
                                                   const char* player_a, const char* player_b);

/* Game operations */
error_code_t game_manager_play_move(game_manager_t* manager, const char* game_id, 
                                   const char* player, int pit_index, int* seeds_captured);
error_code_t game_manager_get_board(game_manager_t* manager, const char* game_id, board_t* board_out);

/* Game queries */
int game_manager_count_active_games(game_manager_t* manager);
int game_manager_count_player_games(game_manager_t* manager, const char* player);
bool game_manager_is_player_in_game(game_manager_t* manager, const char* player);

/* Game ID generation */
void game_manager_generate_id(const char* player_a, const char* player_b, char* game_id);

#endif /* GAME_MANAGER_H */
