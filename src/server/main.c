/* Awale Server - Entry Point
 * Multi-threaded TCP server with modular architecture
 */

#define _POSIX_C_SOURCE 199309L  /* Enable nanosleep */

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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <time.h>

/* Global managers */
static game_manager_t g_game_manager;
static matchmaking_t g_matchmaking;
static volatile bool g_running = true;
static int g_discovery_port = 12345;  /* Discovery port for TCP connections */

/* Client handler structure (used in main connection accept loop) */
typedef struct {
    connection_t conn;
    char pseudo[MAX_PSEUDO_LEN];
    pthread_t thread;
} client_handler_t;

/* Signal handler */
void signal_handler(int sig) {
    (void)sig;
    g_running = false;
    printf("\n\nShutting down server...\n");
}

int main(int argc, char** argv) {
    g_discovery_port = 12345;  /* Default discovery port */
    
    if (argc == 2) {
        g_discovery_port = atoi(argv[1]);
    } else if (argc > 2) {
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
    if (game_manager_init(&g_game_manager) != SUCCESS) {
        fprintf(stderr, "Failed to initialize game manager\n");
        return 1;
    }
    
    if (matchmaking_init(&g_matchmaking) != SUCCESS) {
        fprintf(stderr, "Failed to initialize matchmaking\n");
        return 1;
    }
    
    session_registry_init();
    handlers_init(&g_game_manager, &g_matchmaking);
    connection_manager_init(&g_game_manager, &g_matchmaking, &g_running, g_discovery_port);
    
    printf("✓ Game manager initialized\n");
    printf("✓ Matchmaking initialized\n");
    printf("✓ Session registry initialized\n");
    printf("✓ Message handlers initialized\n");
    printf("✓ Connection manager initialized\n");
    
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Start UDP broadcast discovery thread */
    pthread_t udp_thread;
    if (pthread_create(&udp_thread, NULL, udp_discovery_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create UDP discovery thread\n");
        return 1;
    }
    pthread_detach(udp_thread);
    printf("✓ UDP broadcast discovery listening on port 12346\n");
    
    /* Create discovery server (single socket) */
    connection_t discovery_server;
    if (connection_init(&discovery_server) != SUCCESS ||
        connection_create_discovery_server(&discovery_server, g_discovery_port) != SUCCESS) {
        fprintf(stderr, "Failed to create discovery server\n");
        return 1;
    }
    
    printf("✓ Discovery server listening on port %d\n", g_discovery_port);
    printf("\n🎮 Server ready! Waiting for connections...\n\n");
    
    /* Accept loop */
    while (g_running) {
        /* Accept initial connection on discovery port */
        connection_t discovery_conn;
        connection_init(&discovery_conn);
        
        error_code_t err = connection_accept_discovery(&discovery_server, &discovery_conn);
        if (err != SUCCESS) {
            if (g_running) {
                fprintf(stderr, "Failed to accept discovery connection\n");
            }
            continue;
        }
        
        printf("📡 Discovery connection accepted from %s\n", connection_get_peer_ip(&discovery_conn));
        
        /* Step 1: Receive client's listening port */
        msg_port_negotiation_t client_port_msg;
        size_t bytes_received;
        err = connection_recv_raw(&discovery_conn, &client_port_msg, sizeof(client_port_msg), &bytes_received);
        if (err != SUCCESS || bytes_received != sizeof(client_port_msg)) {
            fprintf(stderr, "Failed to receive client port\n");
            connection_close(&discovery_conn);
            continue;
        }
        
        int client_port = ntohl(client_port_msg.my_port);
        printf("   Client listening on port: %d\n", client_port);
        
        /* Step 2: Find a free port for server to listen on */
        int server_port;
        err = connection_find_free_port(&server_port);
        if (err != SUCCESS) {
            fprintf(stderr, "Failed to find free port\n");
            connection_close(&discovery_conn);
            continue;
        }
        
        printf("   Server will listen on port: %d\n", server_port);
        
        /* Step 3: Send server's listening port to client */
        msg_port_negotiation_t server_port_msg;
        server_port_msg.my_port = htonl(server_port);
        
        /* Get client IP before closing discovery connection */
        char client_ip[64];
        strncpy(client_ip, connection_get_peer_ip(&discovery_conn), sizeof(client_ip) - 1);
        
        err = connection_send_raw(&discovery_conn, &server_port_msg, sizeof(server_port_msg));
        if (err != SUCCESS) {
            fprintf(stderr, "Failed to send server port\n");
            connection_close(&discovery_conn);
            continue;
        }
        
        /* Close discovery connection */
        connection_close(&discovery_conn);
        
        /* Step 4: Create server listening socket */
        connection_t server_listen;
        if (connection_init(&server_listen) != SUCCESS ||
            connection_create_discovery_server(&server_listen, server_port) != SUCCESS) {
            fprintf(stderr, "Failed to create server listening socket\n");
            continue;
        }
        
        /* Step 5: Connect to client's listening port */
        connection_t client_conn;
        connection_init(&client_conn);
        
        /* Connect to client (write socket) with retry logic */
        client_conn.write_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_conn.write_sockfd < 0) {
            fprintf(stderr, "Failed to create socket\n");
            connection_close(&server_listen);
            continue;
        }
        
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(client_port);
        inet_pton(AF_INET, client_ip, &client_addr.sin_addr);
        
        /* Retry connection to client (client may need time to set up listening socket) */
        int connect_attempts = 0;
        const int max_attempts = 10;
        const int retry_delay_ms = 100;  /* 100ms between attempts */
        bool connected = false;
        
        while (connect_attempts < max_attempts && !connected) {
            if (connect(client_conn.write_sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) == 0) {
                connected = true;
                break;
            }
            
            connect_attempts++;
            if (connect_attempts < max_attempts) {
                /* Sleep for retry_delay_ms milliseconds */
                struct timespec ts;
                ts.tv_sec = retry_delay_ms / 1000;
                ts.tv_nsec = (retry_delay_ms % 1000) * 1000000;
                nanosleep(&ts, NULL);
                
                /* Need to create a new socket for retry (can't reuse after failed connect) */
                close(client_conn.write_sockfd);
                client_conn.write_sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (client_conn.write_sockfd < 0) {
                    fprintf(stderr, "Failed to create socket for retry\n");
                    connection_close(&server_listen);
                    connected = false;
                    break;
                }
            }
        }
        
        if (!connected) {
            fprintf(stderr, "Failed to connect to client port %d after %d attempts\n", 
                    client_port, max_attempts);
            close(client_conn.write_sockfd);
            connection_close(&server_listen);
            continue;
        }
        
        /* Step 6: Accept client's connection to our listening port (read socket) */
        connection_t temp_conn;
        err = connection_accept_discovery(&server_listen, &temp_conn);
        if (err != SUCCESS) {
            fprintf(stderr, "Failed to accept from client\n");
            close(client_conn.write_sockfd);
            connection_close(&server_listen);
            continue;
        }
        
        client_conn.read_sockfd = temp_conn.read_sockfd;
        client_conn.connected = true;
        client_conn.sequence = 0;
        
        /* Close server listening socket (no longer needed) */
        connection_close(&server_listen);
        
        printf("✓ Bidirectional connection established with %s\n", client_ip);
        
        /* Receive initial MSG_CONNECT */
        message_header_t header;
        size_t received;
        if (connection_recv_raw(&client_conn, &header, sizeof(header), &received) != SUCCESS) {
            connection_close(&client_conn);
            continue;
        }
        
        header.type = ntohl(header.type);
        header.length = ntohl(header.length);
        
        if (header.type != MSG_CONNECT || header.length == 0) {
            connection_close(&client_conn);
            continue;
        }
        
        /* Read connect message */
        msg_connect_t connect_msg;
        if (connection_recv_raw(&client_conn, &connect_msg, header.length, &received) != SUCCESS) {
            connection_close(&client_conn);
            continue;
        }
        
        connect_msg.pseudo[MAX_PSEUDO_LEN - 1] = '\0';
        
        printf("📥 Connection from %s (%s)\n", connect_msg.pseudo, client_ip);
        
        /* Add to matchmaking */
        if (matchmaking_add_player(&g_matchmaking, connect_msg.pseudo, client_ip) != SUCCESS) {
            connection_close(&client_conn);
            continue;
        }
        
        /* Send acknowledgment */
        session_t temp_session;
        temp_session.conn = client_conn;
        strncpy(temp_session.pseudo, connect_msg.pseudo, MAX_PSEUDO_LEN - 1);
        temp_session.pseudo[MAX_PSEUDO_LEN - 1] = '\0';
        
        /* Create simple session ID */
        int session_num = (int)(time(NULL) % 10000);
        snprintf(temp_session.session_id, sizeof(temp_session.session_id), "S%d", session_num);
        temp_session.session_id[sizeof(temp_session.session_id) - 1] = '\0';
        
        session_send_connect_ack(&temp_session, true, "Welcome to Awale!");
        
        /* Create client handler thread */
        client_handler_t* handler = malloc(sizeof(client_handler_t));
        handler->conn = client_conn;
        strncpy(handler->pseudo, connect_msg.pseudo, MAX_PSEUDO_LEN - 1);
        handler->pseudo[MAX_PSEUDO_LEN - 1] = '\0';
        
        if (pthread_create(&handler->thread, NULL, client_handler, handler) != 0) {
            fprintf(stderr, "Failed to create client thread\n");
            connection_close(&client_conn);
            free(handler);
            continue;
        }
        
        pthread_detach(handler->thread);
        
        printf("✓ Client handler thread started for %s\n\n", handler->pseudo);
    }
    
    printf("\n🛑 Server stopped\n");
    
    connection_close(&discovery_server);
    game_manager_destroy(&g_game_manager);
    matchmaking_destroy(&g_matchmaking);
    
    return 0;
}
