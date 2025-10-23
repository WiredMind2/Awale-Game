/* Server Connection Management
 * Handles client connection lifecycle and UDP discovery thread
 */

#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include "../network/connection.h"
#include "../network/session.h"
#include "../server/game_manager.h"
#include "../server/matchmaking.h"
#include <pthread.h>
#include <stdbool.h>

/* Initialize connection manager with global managers and running flag */
void connection_manager_init(game_manager_t* game_mgr, matchmaking_t* matchmaking, 
                             volatile bool* running_flag, int discovery_port);

/* UDP discovery thread function
 * Listens for UDP broadcast discovery requests and responds with server info
 */
void* udp_discovery_thread(void* arg);

/* Client handler thread function
 * Processes messages from a connected client
 */
void* client_handler(void* arg);

#endif /* SERVER_CONNECTION_H */
