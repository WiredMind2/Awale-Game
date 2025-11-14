/* Awale Server - Entry Point
 * Multi-threaded TCP server with modular architecture
 */

#define _POSIX_C_SOURCE 199309L /* Enable nanosleep */

#include "../../include/common/types.h"
#include "../../include/common/protocol.h"
#include "../../include/common/messages.h"
#include "../../include/server/game_manager.h"
#include "../../include/server/matchmaking.h"
#include "../../include/server/server_registry.h"
#include "../../include/server/server_handlers.h"
#include "../../include/server/server_connection.h"
#include "../../include/network/connection.h"
#include "../../include/network/session.h"
#include "../../include/server/storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

/* Global managers */
static game_manager_t g_game_manager;
static matchmaking_t g_matchmaking;
static volatile bool g_running = true;
static int g_discovery_port = 12345; /* Discovery port for TCP connections */

/* Client handler structure (used in main connection accept loop) */
typedef struct
{
    connection_t conn;
    char pseudo[MAX_PSEUDO_LEN];
    pthread_t thread;
} client_handler_t;

/* Signal handler */
void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
    printf("\n\nShutting down server...\n");
}

/* Simple FNV-1a 32-bit hash for strings */
static uint32_t fnv1a_hash(const char *s) {
    uint32_t hash = 2166136261u;
    while (*s) {
        hash ^= (unsigned char)*s++;
        hash *= 16777619u;
    }
    return hash;
}

int main(int argc, char** argv) {
    g_discovery_port = 12345;  /* Default discovery port */
    
    if (argc == 2) {
        g_discovery_port = atoi(argv[1]);
    }
    else if (argc > 2)
    {
        printf("Usage: %s [discovery_port]\n", argv[0]);
        printf("  discovery_port: Port for initial client connections (default: 12345)\n");
        printf("  Clients will discover server via UDP broadcast.\n");
        return 1;
    }

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║         AWALE SERVER (Modular Architecture)          ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("Discovery Port: %d (TCP)\n", g_discovery_port);
    printf("Broadcast Port: 12346 (UDP)\n");
    printf("Initializing...\n");

    /* Initialize managers */
    if (game_manager_init(&g_game_manager) != SUCCESS)
    {
        fprintf(stderr, "Failed to initialize game manager\n");
        return 1;
    }

    if (matchmaking_init(&g_matchmaking) != SUCCESS)
    {
        fprintf(stderr, "Failed to initialize matchmaking\n");
        return 1;
    }

    session_registry_init();
    handlers_init(&g_game_manager, &g_matchmaking);
    connection_manager_init(&g_game_manager, &g_matchmaking, &g_running, g_discovery_port);
    
    /* Initialize storage */
    if (storage_init() != SUCCESS) {
        fprintf(stderr, "Failed to initialize storage\n");
        return 1;
    }
    
    printf("Game manager initialisé\n");
    printf("Matchmaking initialisé\n");
    printf("Session registry initialisé\n");
    printf("Message handlers initialisé\n");
    printf("Connection manager initialisé\n");

    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Start UDP broadcast discovery thread */
    pthread_t udp_thread;
    if (pthread_create(&udp_thread, NULL, udp_discovery_thread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create UDP discovery thread\n");
        return 1;
    }
    pthread_detach(udp_thread);
    printf("UDP broadcast discovery listening on port 12346\n");

    /* Create discovery server (single socket) */
    connection_t discovery_server;
    if (connection_init(&discovery_server) != SUCCESS ||
        connection_create_server(&discovery_server, g_discovery_port) != SUCCESS)
    {
        fprintf(stderr, "Failed to create discovery server\n");
        return 1;
    }

    printf("Discovery server listening on port %d\n", g_discovery_port);
    printf("\nServer ready! Waiting for connections...\n\n");

    /* Accept loop */
    time_t last_cleanup = time(NULL);
    while (g_running)
    {
        /* Periodic cleanup of expired challenges */
        time_t now = time(NULL);
        if (now - last_cleanup >= 30) {  /* Clean up every 30 seconds */
            matchmaking_cleanup_expired_challenges(&g_matchmaking);
            last_cleanup = now;
        }

        /* Accept client connection on discovery port */
        connection_t client_conn;
        connection_init(&client_conn);

        error_code_t err = connection_accept(&discovery_server, &client_conn);
        if (err != SUCCESS)
        {
            if (g_running)
            {
                fprintf(stderr, "Failed to accept client connection\n");
            }
            continue;
        }

        printf("Connection client acceptee a partir de %s\n", connection_get_peer_ip(&client_conn));

        /* Receive initial MSG_CONNECT */
        message_header_t header;
        size_t received;
        if (connection_recv_raw(&client_conn, &header, sizeof(header), &received) != SUCCESS)
        {
            connection_close(&client_conn);
            continue;
        }

        header.type = ntohl(header.type);
        header.length = ntohl(header.length);

        if (header.type != MSG_CONNECT || header.length == 0)
        {
            connection_close(&client_conn);
            continue;
        }

        /* Read connect message */
        msg_connect_t connect_msg;
        if (connection_recv_raw(&client_conn, &connect_msg, header.length, &received) != SUCCESS)
        {
            connection_close(&client_conn);
            continue;
        }

        connect_msg.pseudo[MAX_PSEUDO_LEN - 1] = '\0';

        player_info_t players[100];
        int count;
        error_code_t error_matchmaking = matchmaking_get_players(&g_matchmaking, players, 100, &count);
        bool pseudo_already_use = false;
        if (error_matchmaking == SUCCESS)
        {
            for (int i = 0; i < count; i++)
            {
                if (strcmp(connect_msg.pseudo, players[i].pseudo) == 0)
                {
                    // Send a proper connect ACK with success=false so the client will detect the rejection
                    session_t temp_fail_session;
                    memset(&temp_fail_session, 0, sizeof(temp_fail_session));
                    temp_fail_session.conn = client_conn;
                    session_send_connect_ack(&temp_fail_session, false, "Pseudo deja utilise");
                    pseudo_already_use = true;
                    break;
                }
            }
        }
        if (pseudo_already_use)
        {
            connection_close(&client_conn);
            continue;
        }
        printf("Connection from %s (%s)\n", connect_msg.pseudo, connection_get_peer_ip(&client_conn));

        /* Add to matchmaking */
        if (matchmaking_add_player(&g_matchmaking, connect_msg.pseudo, connection_get_peer_ip(&client_conn)) != SUCCESS)
        {
            connection_close(&client_conn);
            continue;
        }

        /* Send acknowledgment */
        session_t temp_session;
        temp_session.conn = client_conn;
        strncpy(temp_session.pseudo, connect_msg.pseudo, MAX_PSEUDO_LEN - 1);
        temp_session.pseudo[MAX_PSEUDO_LEN - 1] = '\0';

        /* Create simple session ID */
        uint32_t h = fnv1a_hash(connect_msg.pseudo);
        snprintf(temp_session.session_id, sizeof(temp_session.session_id), "S%08x", h);
        temp_session.session_id[sizeof(temp_session.session_id) - 1] = '\0';

        session_send_connect_ack(&temp_session, true, "Bienvenue sur Awale!");

        /* Create client handler thread */
        client_handler_t *handler = malloc(sizeof(client_handler_t));
        handler->conn = client_conn;
        strncpy(handler->pseudo, connect_msg.pseudo, MAX_PSEUDO_LEN - 1);
        handler->pseudo[MAX_PSEUDO_LEN - 1] = '\0';

        if (pthread_create(&handler->thread, NULL, client_handler, handler) != 0)
        {
            fprintf(stderr, "Failed to create client thread\n");
            connection_close(&client_conn);
            free(handler);
            continue;
        }

        pthread_detach(handler->thread);

    printf("Client handler thread started for %s\n\n", handler->pseudo);
    }

    printf("\nServer stopped\n");

    connection_close(&discovery_server);
    game_manager_destroy(&g_game_manager);
    matchmaking_destroy(&g_matchmaking);
    storage_cleanup();
    
    return 0;
}
