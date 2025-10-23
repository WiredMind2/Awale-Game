#include "../../include/common/protocol.h"
#include <stdio.h>

const char* message_type_to_string(message_type_t type) {
    switch (type) {
        case MSG_UNKNOWN: return "UNKNOWN";
        case MSG_CONNECT: return "CONNECT";
        case MSG_DISCONNECT: return "DISCONNECT";
        case MSG_LIST_PLAYERS: return "LIST_PLAYERS";
        case MSG_CHALLENGE: return "CHALLENGE";
        case MSG_ACCEPT_CHALLENGE: return "ACCEPT_CHALLENGE";
        case MSG_DECLINE_CHALLENGE: return "DECLINE_CHALLENGE";
        case MSG_GET_CHALLENGES: return "GET_CHALLENGES";
        case MSG_PLAY_MOVE: return "PLAY_MOVE";
        case MSG_GET_BOARD: return "GET_BOARD";
        case MSG_SURRENDER: return "SURRENDER";
        case MSG_CONNECT_ACK: return "CONNECT_ACK";
        case MSG_ERROR: return "ERROR";
        case MSG_PLAYER_LIST: return "PLAYER_LIST";
        case MSG_CHALLENGE_SENT: return "CHALLENGE_SENT";
        case MSG_CHALLENGE_RECEIVED: return "CHALLENGE_RECEIVED";
        case MSG_GAME_STARTED: return "GAME_STARTED";
        case MSG_MOVE_RESULT: return "MOVE_RESULT";
        case MSG_BOARD_STATE: return "BOARD_STATE";
        case MSG_GAME_OVER: return "GAME_OVER";
        case MSG_CHALLENGE_LIST: return "CHALLENGE_LIST";
        default: return "INVALID";
    }
}

bool is_valid_message_type(message_type_t type) {
    return type > MSG_UNKNOWN && type <= MSG_CHALLENGE_LIST;
}
