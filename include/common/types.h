#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h> // For time_t

/* Constants */
#define MAX_PSEUDO_LEN 100
#define MAX_GAME_ID_LEN 256
#define MAX_IP_LEN 46  /* IPv6 max length */
#define MAX_CHAT_LEN 512
#define MAX_FRIENDS 50
#define NUM_PITS 12
#define PITS_PER_PLAYER 6
#define INITIAL_SEEDS_PER_PIT 4
#define TOTAL_SEEDS 48
#define WIN_SCORE 25

/* Error codes */
typedef enum {
    SUCCESS = 0,
    ERR_INVALID_PARAM = -1,
    ERR_GAME_NOT_FOUND = -2,
    ERR_INVALID_MOVE = -3,
    ERR_NOT_YOUR_TURN = -4,
    ERR_EMPTY_PIT = -5,
    ERR_WRONG_SIDE = -6,
    ERR_STARVE_VIOLATION = -7,
    ERR_GAME_EXISTS = -8,
    ERR_PLAYER_NOT_FOUND = -9,
    ERR_NETWORK_ERROR = -10,
    ERR_SERIALIZATION = -11,
    ERR_MAX_CAPACITY = -12,
    ERR_DUPLICATE = -13,
    ERR_TIMEOUT = -14,
    ERR_UNKNOWN = -99
} error_code_t;

/* Player identifier */
typedef enum {
    PLAYER_A = 0,
    PLAYER_B = 1
} player_id_t;

/* Game state */
typedef enum {
    GAME_STATE_WAITING = 0,
    GAME_STATE_IN_PROGRESS = 1,
    GAME_STATE_FINISHED = 2,
    GAME_STATE_ABANDONED = 3
} game_state_t;

/* Winner */
typedef enum {
    NO_WINNER = -1,
    WINNER_A = 0,
    WINNER_B = 1,
    DRAW = 2
} winner_t;

/* Basic types for players */
typedef struct {
    char pseudo[MAX_PSEUDO_LEN];
    char ip[MAX_IP_LEN];
    /* Statistics */
    int games_played;
    int games_won;
    int games_lost;
    int total_score;
    /* Bio (10 lines of 256 characters each) */
    char bio[10][256];
    int bio_lines;
    /* Friends list */
    char friends[MAX_FRIENDS][MAX_PSEUDO_LEN];
    int friend_count;
} player_info_t;

/* Utility macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Error to string */
const char* error_to_string(error_code_t err);

#endif /* TYPES_H */
