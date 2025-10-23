#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "types.h"

/* Protocol version */
#define PROTOCOL_VERSION "1.0"

/* Message types */
typedef enum {
    MSG_UNKNOWN = 0,
    
    /* Client requests */
    MSG_CONNECT,
    MSG_DISCONNECT,
    MSG_LIST_PLAYERS,
    MSG_CHALLENGE,
    MSG_ACCEPT_CHALLENGE,
    MSG_DECLINE_CHALLENGE,
    MSG_GET_CHALLENGES,
    MSG_PLAY_MOVE,
    MSG_GET_BOARD,
    MSG_SURRENDER,
    
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
    MSG_CHALLENGE_LIST
} message_type_t;

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
