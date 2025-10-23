#ifndef MESSAGES_H
#define MESSAGES_H

#include "types.h"
#include "protocol.h"

/* Message payload structures */

/* MSG_CONNECT */
typedef struct {
    char pseudo[MAX_PSEUDO_LEN];
    char version[16];
} msg_connect_t;

/* MSG_CONNECT_ACK */
typedef struct {
    bool success;
    char message[256];
    char session_id[64];
} msg_connect_ack_t;

/* MSG_ERROR */
typedef struct {
    int32_t error_code;
    char error_msg[256];
} msg_error_t;

/* MSG_LIST_PLAYERS / MSG_PLAYER_LIST */
typedef struct {
    int count;
    player_info_t players[100];  /* Max 100 players */
} msg_player_list_t;

/* MSG_CHALLENGE */
typedef struct {
    char challenger[MAX_PSEUDO_LEN];
    char opponent[MAX_PSEUDO_LEN];
} msg_challenge_t;

/* MSG_CHALLENGE_RECEIVED */
typedef struct {
    char from[MAX_PSEUDO_LEN];
    char message[256];
} msg_challenge_received_t;

/* MSG_GAME_STARTED */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char player_a[MAX_PSEUDO_LEN];
    char player_b[MAX_PSEUDO_LEN];
    player_id_t your_side;
} msg_game_started_t;

/* MSG_PLAY_MOVE */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char player[MAX_PSEUDO_LEN];
    int pit_index;
} msg_play_move_t;

/* MSG_MOVE_RESULT */
typedef struct {
    bool success;
    char message[256];
    int seeds_captured;
    bool game_over;
    winner_t winner;
} msg_move_result_t;

/* MSG_GET_BOARD / MSG_BOARD_STATE */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char player_a[MAX_PSEUDO_LEN];
    char player_b[MAX_PSEUDO_LEN];
} msg_get_board_t;

typedef struct {
    bool exists;
    char game_id[MAX_GAME_ID_LEN];
    char player_a[MAX_PSEUDO_LEN];
    char player_b[MAX_PSEUDO_LEN];
    int pits[NUM_PITS];
    int score_a;
    int score_b;
    player_id_t current_player;
    game_state_t state;
    winner_t winner;
} msg_board_state_t;

/* MSG_GAME_OVER */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    winner_t winner;
    int score_a;
    int score_b;
    char message[256];
} msg_game_over_t;

/* MSG_GET_CHALLENGES / MSG_CHALLENGE_LIST */
typedef struct {
    int count;
    char challengers[100][MAX_PSEUDO_LEN];
} msg_challenge_list_t;

#endif /* MESSAGES_H */
