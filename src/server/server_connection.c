/* Server Connection Management Implementation
 * Handles client connection lifecycle and UDP discovery thread
 */

#include "../../include/server/server_connection.h"
#include "../../include/server/server_registry.h"
#include "../../include/server/server_handlers.h"
#include "../../include/common/messages.h"
#include "../../include/common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Client handler structure */
typedef struct {
    connection_t conn;
    char pseudo[MAX_PSEUDO_LEN];
    pthread_t thread;
} client_handler_t;

/* Global state for connection manager */
static game_manager_t* g_game_manager = NULL;
static matchmaking_t* g_matchmaking = NULL;
static volatile bool* g_running = NULL;
static int g_discovery_port = 12345;

void connection_manager_init(game_manager_t* game_mgr, matchmaking_t* matchmaking,
                             volatile bool* running_flag, int discovery_port) {
    g_game_manager = game_mgr;
    g_matchmaking = matchmaking;
    g_running = running_flag;
    g_discovery_port = discovery_port;
}

/* UDP broadcast discovery thread */
void* udp_discovery_thread(void* arg) {
    (void)arg;
    connection_listen_for_discovery(g_discovery_port, 12346);
    return NULL;
}

/* Client thread handler */
void* client_handler(void* arg) {
    client_handler_t* handler = (client_handler_t*)arg;
    session_t session;
    
    session_init(&session);
    memcpy(&session.conn, &handler->conn, sizeof(connection_t));
    strncpy(session.pseudo, handler->pseudo, MAX_PSEUDO_LEN - 1);
    session.pseudo[MAX_PSEUDO_LEN - 1] = '\0';
    session.authenticated = true;
    
    printf("✓ Client thread started for %s\n", session.pseudo);
    
    /* Register session for push notifications */
    if (!session_registry_add(&session)) {
        printf("❌ Failed to register session for %s (max sessions reached)\n", session.pseudo);
        session_close(&session);
        free(handler);
        return NULL;
    }
    
    time_t last_check = time(NULL);
    const time_t CHECK_INTERVAL = 60;  /* Check connection health every 60 seconds */
    
    /* Main message loop */
    while (*g_running && session_is_active(&session)) {
        message_type_t msg_type;
        char payload[MAX_PAYLOAD_SIZE];
        size_t payload_size;
        
        /* Periodically check if connection is still alive */
        time_t now = time(NULL);
        if (now - last_check >= CHECK_INTERVAL) {
            error_code_t check_err = connection_check_alive(&session.conn);
            if (check_err != SUCCESS) {
                printf("⚠️ Client %s connection check failed - disconnecting\n", session.pseudo);
                break;
            }
            last_check = now;
        }
        
        /* Use timeout to allow periodic connection checking */
        error_code_t err = session_recv_message_timeout(&session, &msg_type, payload, 
                                                        MAX_PAYLOAD_SIZE, &payload_size, 5000);
        
        if (err == ERR_TIMEOUT) {
            /* Normal timeout - continue loop to check connection health */
            continue;
        }
        
        if (err != SUCCESS) {
            if (err == ERR_NETWORK_ERROR) {
                printf("🔌 Client %s disconnected (network error)\n", session.pseudo);
            } else {
                printf("⚠️ Client %s error: %s\n", session.pseudo, error_to_string(err));
            }
            break;
        }
        
        /* Route message to appropriate handler */
        switch (msg_type) {
            case MSG_LIST_PLAYERS:
                handle_list_players(&session);
                break;
                
            case MSG_CHALLENGE: {
                msg_challenge_t* challenge = (msg_challenge_t*)payload;
                handle_challenge(&session, challenge->opponent);
                break;
            }
            
            case MSG_ACCEPT_CHALLENGE: {
                msg_challenge_response_t* response = (msg_challenge_response_t*)payload;
                handle_accept_challenge(&session, response->challenger);
                break;
            }
            
            case MSG_DECLINE_CHALLENGE: {
                msg_challenge_response_t* response = (msg_challenge_response_t*)payload;
                handle_decline_challenge(&session, response->challenger);
                break;
            }
            
            case MSG_GET_CHALLENGES:
                handle_get_challenges(&session);
                break;
                
            case MSG_PLAY_MOVE: {
                msg_play_move_t* move = (msg_play_move_t*)payload;
                handle_play_move(&session, move);
                break;
            }
            
            case MSG_GET_BOARD: {
                msg_get_board_t* req = (msg_get_board_t*)payload;
                handle_get_board(&session, req);
                break;
            }
            
            case MSG_LIST_GAMES:
                handle_list_games(&session);
                break;
                
            case MSG_SPECTATE_GAME: {
                msg_spectate_game_t* req = (msg_spectate_game_t*)payload;
                handle_spectate_game(&session, req->game_id);
                break;
            }
            
            case MSG_STOP_SPECTATE: {
                msg_spectate_game_t* req = (msg_spectate_game_t*)payload;
                handle_stop_spectate(&session, req->game_id);
                break;
            }
            
            case MSG_SET_BIO: {
                msg_set_bio_t* bio_msg = (msg_set_bio_t*)payload;
                handle_set_bio(&session, bio_msg);
                break;
            }
            
            case MSG_GET_BIO: {
                msg_get_bio_t* bio_req = (msg_get_bio_t*)payload;
                handle_get_bio(&session, bio_req);
                break;
            }
            
            case MSG_GET_PLAYER_STATS: {
                msg_get_player_stats_t* stats_req = (msg_get_player_stats_t*)payload;
                handle_get_player_stats(&session, stats_req);
                break;
            }

            case MSG_SEND_CHAT: {
                msg_send_chat_t* chat_msg = (msg_send_chat_t*)payload;
                handle_send_chat(&session, chat_msg);
                break;
            }

            case MSG_DISCONNECT:
                printf("Client %s requested disconnect\n", session.pseudo);
                goto cleanup;
                
            default:
                printf("Unknown message type %d from %s\n", msg_type, session.pseudo);
                session_send_error(&session, ERR_UNKNOWN, "Unknown message type");
                break;
        }
    }
    
cleanup:
    printf("🔌 Client %s disconnected\n", session.pseudo);
    
    /* Clean up all server-side resources for this client */
    session_registry_remove(&session);
    matchmaking_remove_player(g_matchmaking, session.pseudo);
    
    /* Remove player from any active games as spectator */
    for (int i = 0; i < g_game_manager->game_count; i++) {
        if (g_game_manager->games[i].active) {
            game_manager_remove_spectator(g_game_manager, 
                                         g_game_manager->games[i].game_id, 
                                         session.pseudo);
        }
    }
    
    session_close(&session);
    free(handler);
    
    return NULL;
}
