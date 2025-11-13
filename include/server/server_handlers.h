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

/* Handle MSG_CHALLENGE_ACCEPT - Accept a challenge by ID */
void handle_challenge_accept(session_t* session, const msg_challenge_accept_t* accept_msg);

/* Handle MSG_CHALLENGE_DECLINE - Decline a challenge by ID */
void handle_challenge_decline(session_t* session, const msg_challenge_decline_t* decline_msg);

/* Handle MSG_GET_CHALLENGES - Get list of pending challenges */
void handle_get_challenges(session_t* session);

/* Handle MSG_PLAY_MOVE - Execute a move in an active game */
void handle_play_move(session_t* session, const msg_play_move_t* move);

/* Handle MSG_GET_BOARD - Retrieve board state for a game */
void handle_get_board(session_t* session, const msg_get_board_t* req);

/* Handle MSG_LIST_GAMES - Get list of active games */
void handle_list_games(session_t* session);

/* Handle MSG_LIST_MY_GAMES - Get list of player's active games */
void handle_list_my_games(session_t* session);

/* Handle MSG_SPECTATE_GAME - Join as spectator for a game */
void handle_spectate_game(session_t* session, const char* game_id);

/* Handle MSG_STOP_SPECTATE - Stop spectating a game */
void handle_stop_spectate(session_t* session, const char* game_id);

/* Handle MSG_SET_BIO - Set player bio */
void handle_set_bio(session_t* session, const msg_set_bio_t* bio);

/* Handle MSG_GET_BIO - Get player bio */
void handle_get_bio(session_t* session, const msg_get_bio_t* bio_req);

/* Handle MSG_GET_PLAYER_STATS - Get player statistics */
void handle_get_player_stats(session_t* session, const msg_get_player_stats_t* stats_req);

/* Handle MSG_SEND_CHAT - Send chat message */
void handle_send_chat(session_t* session, const msg_send_chat_t* chat_msg);

#endif /* SERVER_HANDLERS_H */
