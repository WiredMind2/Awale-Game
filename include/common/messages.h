#ifndef MESSAGES_H
#define MESSAGES_H

#include "types.h"
#include "protocol.h"

/* Message payload structures */

/* MSG_PORT_NEGOTIATION - bidirectional port exchange */
typedef struct {
    int32_t my_port;     /* Port that sender is listening on */
} msg_port_negotiation_t;

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

/* MSG_CHALLENGE_RECEIVED - Push notification to opponent */
typedef struct {
    char from[MAX_PSEUDO_LEN];
    char message[256];
    int64_t challenge_id;  /* Unique ID to accept/decline */
} msg_challenge_received_t;

/* MSG_ACCEPT_CHALLENGE / MSG_DECLINE_CHALLENGE */
typedef struct {
    char challenger[MAX_PSEUDO_LEN];  /* Who originally challenged */
} msg_challenge_response_t;

/* MSG_CHALLENGE_ACCEPT */
typedef struct {
    int64_t challenge_id;
    char response[256];
} msg_challenge_accept_t;

/* MSG_CHALLENGE_DECLINE */
typedef struct {
    int64_t challenge_id;
    char response[256];
} msg_challenge_decline_t;

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

/* MSG_UPDATE_RATING */
typedef struct {
    char player[MAX_PSEUDO_LEN];
    int new_rating;
    int rating_change;
} msg_update_rating_t;

/* MSG_GET_CHALLENGES / MSG_CHALLENGE_LIST */
typedef struct {
    int count;
    char challengers[100][MAX_PSEUDO_LEN];
} msg_challenge_list_t;

/* MSG_LIST_GAMES / MSG_GAME_LIST */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char player_a[MAX_PSEUDO_LEN];
    char player_b[MAX_PSEUDO_LEN];
    int spectator_count;
    game_state_t state;
} game_info_t;

typedef struct {
    int count;
    game_info_t games[50];  /* Max 50 games */
} msg_game_list_t;

/* MSG_LIST_MY_GAMES / MSG_MY_GAME_LIST - Same structure as game_list but for player's games */
typedef msg_game_list_t msg_my_game_list_t;

/* MSG_SPECTATE_GAME */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
} msg_spectate_game_t;

/* MSG_SPECTATE_ACK */
typedef struct {
    bool success;
    char message[256];
    int spectator_count;
} msg_spectate_ack_t;

/* MSG_SPECTATOR_JOINED */
typedef struct {
    char spectator[MAX_PSEUDO_LEN];
    int spectator_count;
    char game_id[MAX_GAME_ID_LEN];
} msg_spectator_joined_t;

/* MSG_SET_BIO */
typedef struct {
    char bio[10][256];  /* 10 lines of 256 characters each */
    int bio_lines;
} msg_set_bio_t;

/* MSG_GET_BIO */
typedef struct {
    char target_player[MAX_PSEUDO_LEN];
} msg_get_bio_t;

/* MSG_BIO_RESPONSE */
typedef struct {
    bool success;
    char player[MAX_PSEUDO_LEN];
    char bio[10][256];
    int bio_lines;
    char message[256];  /* Error message if success=false */
} msg_bio_response_t;

/* MSG_GET_PLAYER_STATS */
typedef struct {
    char target_player[MAX_PSEUDO_LEN];
} msg_get_player_stats_t;

/* MSG_GET_LEADERBOARD */
typedef struct {
    int max_entries;  /* Maximum number of entries to return */
} msg_get_leaderboard_t;

/* MSG_LEADERBOARD */
typedef struct {
    int count;
    struct {
        char player[MAX_PSEUDO_LEN];
        int elo_rating;
        int games_played;
        int games_won;
        int games_lost;
    } entries[50];  /* Max 50 entries */
} msg_leaderboard_t;

/* MSG_PLAYER_STATS */
typedef struct {
    bool success;
    char player[MAX_PSEUDO_LEN];
    int games_played;
    int games_won;
    int games_lost;
    int total_score;
    int elo_rating;
    time_t first_seen;
    time_t last_seen;
    char message[256];  /* Error message if success=false */
} msg_player_stats_t;

/* MSG_SEND_CHAT */
typedef struct {
    char recipient[MAX_PSEUDO_LEN];  /* Empty for global chat */
    char message[MAX_CHAT_LEN];
} msg_send_chat_t;

/* MSG_CHAT_MESSAGE */
typedef struct {
    char sender[MAX_PSEUDO_LEN];
    char recipient[MAX_PSEUDO_LEN];  /* Empty for global chat */
    char message[MAX_CHAT_LEN];
    time_t timestamp;
} msg_chat_message_t;

/* MSG_CHAT_HISTORY */
typedef struct {
    char target_player[MAX_PSEUDO_LEN];  /* Empty for global history */
    int count;
    msg_chat_message_t messages[50];  /* Max 50 messages */
} msg_chat_history_t;

/* MSG_ADD_FRIEND */
typedef struct {
    char friend_pseudo[MAX_PSEUDO_LEN];
} msg_add_friend_t;

/* MSG_REMOVE_FRIEND */
typedef struct {
    char friend_pseudo[MAX_PSEUDO_LEN];
} msg_remove_friend_t;

/* MSG_LIST_FRIENDS */
typedef struct {
    int count;
    char friends[MAX_FRIENDS][MAX_PSEUDO_LEN];
} msg_list_friends_t;

/* MSG_LIST_SAVED_GAMES */
typedef struct {
    char player[MAX_PSEUDO_LEN];  /* Optional: filter by player */
} msg_list_saved_games_t;

/* MSG_VIEW_SAVED_GAME */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
} msg_view_saved_game_t;

/* MSG_START_AI_GAME - No payload needed, server uses session pseudo */
typedef struct {
    /* Empty structure */
} msg_start_ai_game_t;

/* MSG_SAVED_GAME_LIST */
typedef struct {
    int count;
    game_info_t games[50];  /* Max 50 saved games */
} msg_saved_game_list_t;

/* MSG_SAVED_GAME_STATE - Same structure as msg_board_state_t */
typedef msg_board_state_t msg_saved_game_state_t;

#endif /* MESSAGES_H */
