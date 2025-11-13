#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "types.h"

/* Protocol version */
#define PROTOCOL_VERSION "1.0"

/* Message types */
typedef enum {
    MSG_UNKNOWN = 0,
    
    /* Connection negotiation */
    MSG_PORT_NEGOTIATION,
    
    /* Client requests */
    MSG_CONNECT,
    MSG_DISCONNECT,
    MSG_LIST_PLAYERS,
    MSG_CHALLENGE,
    MSG_ACCEPT_CHALLENGE,
    MSG_DECLINE_CHALLENGE,
    MSG_CHALLENGE_ACCEPT,
    MSG_CHALLENGE_DECLINE,
    MSG_GET_CHALLENGES,
    MSG_PLAY_MOVE,
    MSG_GET_BOARD,
    MSG_SURRENDER,
    MSG_LIST_GAMES,            /* List all ongoing games */
    MSG_LIST_MY_GAMES,         /* List games for current player */
    MSG_SPECTATE_GAME,        /* Start spectating a game */
    MSG_STOP_SPECTATE,        /* Stop spectating */
    MSG_SET_BIO,              /* Set player bio */
    MSG_GET_BIO,              /* Get player bio */
    MSG_GET_PLAYER_STATS,     /* Get player statistics */
    MSG_SEND_CHAT,            /* Send chat message */
    MSG_CHAT_MESSAGE,         /* Receive chat message */
    MSG_CHAT_HISTORY,         /* Get chat history */
    MSG_ADD_FRIEND,           /* Add a friend */
    MSG_REMOVE_FRIEND,        /* Remove a friend */
    MSG_LIST_FRIENDS,         /* List friends */
    MSG_LIST_SAVED_GAMES,     /* List saved games for review */
    MSG_VIEW_SAVED_GAME,      /* View a saved game */

    /* Server responses for saved games */
    MSG_SAVED_GAME_LIST,      /* List of saved games */
    MSG_SAVED_GAME_STATE,     /* State of a saved game */

    /* Server responses */
    MSG_CONNECT_ACK,
    MSG_ERROR,
    MSG_PLAYER_LIST,
    MSG_CHALLENGE_SENT,
    MSG_CHALLENGE_RECEIVED,
    MSG_GAME_STARTED,
    MSG_MOVE_RESULT,
    MSG_BOARD_STATE,
    MSG_GAME_OVER,
    MSG_CHALLENGE_LIST,
    MSG_GAME_LIST,            /* List of ongoing games */
    MSG_MY_GAME_LIST,         /* List of player's games */
    MSG_SPECTATE_ACK,         /* Confirmation of spectate start */
    MSG_SPECTATOR_JOINED,     /* Notify players/spectators of new spectator */
    MSG_BIO_RESPONSE,         /* Bio data response */
    MSG_PLAYER_STATS          /* Player statistics response */
} message_type_t;

/* Notification message type filter */
#define IS_NOTIFICATION_MESSAGE(type) \
    ((type) == MSG_CHALLENGE_RECEIVED || \
     (type) == MSG_GAME_STARTED || \
     (type) == MSG_MOVE_RESULT || \
     (type) == MSG_SPECTATOR_JOINED || \
     (type) == MSG_GAME_OVER || \
     (type) == MSG_CHAT_MESSAGE)

/* Message header (fixed size for easy parsing) */
typedef struct {
    uint32_t type;           /* message_type_t */
    uint32_t length;         /* Payload length in bytes */
    uint32_t sequence;       /* Sequence number for tracking */
    uint32_t reserved;       /* Reserved for future use */
} message_header_t;

/* Maximum message size */
#define MAX_MESSAGE_SIZE 8192
#define HEADER_SIZE sizeof(message_header_t)
#define MAX_PAYLOAD_SIZE (MAX_MESSAGE_SIZE - HEADER_SIZE)

/* Protocol functions */
const char* message_type_to_string(message_type_t type);
bool is_valid_message_type(message_type_t type);

#endif /* PROTOCOL_H */
