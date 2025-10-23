#include "../../include/common/types.h"
#include <stdio.h>

const char* error_to_string(error_code_t err) {
    switch (err) {
        case SUCCESS: return "Success";
        case ERR_INVALID_PARAM: return "Invalid parameter";
        case ERR_GAME_NOT_FOUND: return "Game not found";
        case ERR_INVALID_MOVE: return "Invalid move";
        case ERR_NOT_YOUR_TURN: return "Not your turn";
        case ERR_EMPTY_PIT: return "Pit is empty";
        case ERR_WRONG_SIDE: return "Wrong side";
        case ERR_STARVE_VIOLATION: return "Move would starve opponent";
        case ERR_GAME_EXISTS: return "Game already exists";
        case ERR_PLAYER_NOT_FOUND: return "Player not found";
        case ERR_NETWORK_ERROR: return "Network error";
        case ERR_SERIALIZATION: return "Serialization error";
        case ERR_MAX_CAPACITY: return "Maximum capacity reached";
        case ERR_DUPLICATE: return "Duplicate entry";
        default: return "Unknown error";
    }
}
