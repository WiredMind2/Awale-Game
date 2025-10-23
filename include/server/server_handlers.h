/* Server Message Handlers
 * Handles all incoming client messages and game operations
 */

#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

#include "../network/session.h"
#include "../common/messages.h"
#include "../server/game_manager.h"
#include "../server/matchmaking.h"

/* Initialize handlers with global managers (call once at startup) */
void handlers_init(game_manager_t* game_mgr, matchmaking_t* matchmaking);

/* Handle MSG_LIST_PLAYERS - Get list of online players */
void handle_list_players(session_t* session);

/* Handle MSG_CHALLENGE - Send challenge to another player */
void handle_challenge(session_t* session, const char* opponent);

/* Handle MSG_ACCEPT_CHALLENGE - Accept a challenge and start game */
void handle_accept_challenge(session_t* session, const char* challenger);

/* Handle MSG_DECLINE_CHALLENGE - Decline a challenge */
void handle_decline_challenge(session_t* session, const char* challenger);

/* Handle MSG_GET_CHALLENGES - Get list of pending challenges */
void handle_get_challenges(session_t* session);

/* Handle MSG_PLAY_MOVE - Execute a move in an active game */
void handle_play_move(session_t* session, const msg_play_move_t* move);

/* Handle MSG_GET_BOARD - Retrieve board state for a game */
void handle_get_board(session_t* session, const msg_get_board_t* req);

/* Handle MSG_LIST_GAMES - Get list of active games */
void handle_list_games(session_t* session);

/* Handle MSG_SPECTATE_GAME - Join as spectator for a game */
void handle_spectate_game(session_t* session, const char* game_id);

/* Handle MSG_STOP_SPECTATE - Stop spectating a game */
void handle_stop_spectate(session_t* session, const char* game_id);

#endif /* SERVER_HANDLERS_H */
