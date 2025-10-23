/* Awale Server - Full Implementation
 * Multi-threaded TCP server with complete protocol handling
 */

#include "../../include/common/types.h"
#include "../../include/common/protocol.h"
#include "../../include/common/messages.h"
#include "../../include/server/game_manager.h"
#include "../../include/server/matchmaking.h"
#include "../../include/network/connection.h"
#include "../../include/network/session.h"
#include "../../include/game/board.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>

/* Global managers */
static game_manager_t g_game_manager;
static matchmaking_t g_matchmaking;
static volatile bool g_running = true;
static int g_discovery_port = 12345;  /* Discovery port for TCP connections */

/* UDP broadcast discovery thread */
void* udp_discovery_thread(void* arg) {
    (void)arg;
    connection_listen_for_discovery(g_discovery_port, 12346);
    return NULL;
}

/* Client handler structure */
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

/* Handle MSG_LIST_PLAYERS */
void handle_list_players(session_t* session) {
    msg_player_list_t list;
    int count;
    
    error_code_t err = matchmaking_get_players(&g_matchmaking, list.players, 100, &count);
    if (err == SUCCESS) {
        list.count = count;
        
        /* Calculate actual size: count field + only the actual number of players */
        size_t actual_size = sizeof(list.count) + (count * sizeof(player_info_t));
        
        session_send_message(session, MSG_PLAYER_LIST, &list, actual_size);
    } else {
        session_send_error(session, err, "Failed to get player list");
    }
}

/* Handle MSG_CHALLENGE */
void handle_challenge(session_t* session, const char* opponent) {
    bool mutual_found = false;
    
    error_code_t err = matchmaking_create_challenge(&g_matchmaking, session->pseudo, opponent, &mutual_found);
    
    if (err != SUCCESS) {
        session_send_error(session, err, "Failed to create challenge");
        return;
    }
    
    if (mutual_found) {
        /* Both players challenged each other - start game! */
        char game_id[MAX_GAME_ID_LEN];
        err = game_manager_create_game(&g_game_manager, session->pseudo, opponent, game_id);
        
        if (err == SUCCESS) {
            printf("🎮 Game started: %s vs %s (ID: %s)\n", session->pseudo, opponent, game_id);
            
            /* Send MSG_GAME_STARTED to both players would require tracking sessions
             * For now, they'll discover it via MSG_GET_BOARD */
            msg_game_started_t start_msg;
            strncpy(start_msg.game_id, game_id, MAX_GAME_ID_LEN - 1);
            start_msg.game_id[MAX_GAME_ID_LEN - 1] = '\0';
            strncpy(start_msg.player_a, session->pseudo, MAX_PSEUDO_LEN - 1);
            start_msg.player_a[MAX_PSEUDO_LEN - 1] = '\0';
            strncpy(start_msg.player_b, opponent, MAX_PSEUDO_LEN - 1);
            start_msg.player_b[MAX_PSEUDO_LEN - 1] = '\0';
            
            /* Determine which side this player is */
            if (strcmp(session->pseudo, start_msg.player_a) == 0) {
                start_msg.your_side = PLAYER_A;
            } else {
                start_msg.your_side = PLAYER_B;
            }
            
            session_send_message(session, MSG_GAME_STARTED, &start_msg, sizeof(start_msg));
        }
    } else {
        /* Challenge recorded, waiting for opponent to challenge back */
        printf("📨 Challenge sent: %s -> %s\n", session->pseudo, opponent);
        session_send_message(session, MSG_CHALLENGE_SENT, NULL, 0);
    }
}

/* Handle MSG_GET_CHALLENGES */
void handle_get_challenges(session_t* session) {
    msg_challenge_list_t list;
    char challengers[100][MAX_PSEUDO_LEN];
    int count = 0;
    
    pthread_mutex_lock(&g_matchmaking.lock);
    
    for (int i = 0; i < g_matchmaking.challenge_count && count < 100; i++) {
        if (g_matchmaking.challenges[i].active &&
            strcmp(g_matchmaking.challenges[i].opponent, session->pseudo) == 0) {
            strncpy(challengers[count], g_matchmaking.challenges[i].challenger, MAX_PSEUDO_LEN - 1);
            challengers[count][MAX_PSEUDO_LEN - 1] = '\0';
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_matchmaking.lock);
    
    list.count = count;
    for (int i = 0; i < count; i++) {
        memcpy(list.challengers[i], challengers[i], MAX_PSEUDO_LEN);
    }
    
    session_send_message(session, MSG_CHALLENGE_LIST, &list, sizeof(list));
}

/* Handle MSG_PLAY_MOVE */
void handle_play_move(session_t* session, const msg_play_move_t* move) {
    int seeds_captured = 0;
    
    error_code_t err = game_manager_play_move(&g_game_manager, move->game_id, 
                                              session->pseudo, move->pit_index, &seeds_captured);
    
    msg_move_result_t result;
    result.success = (err == SUCCESS);
    result.seeds_captured = seeds_captured;
    
    if (err == SUCCESS) {
        snprintf(result.message, 255, "Move executed: pit %d, captured %d seeds", 
                 move->pit_index, seeds_captured);
        
        /* Check if game is over */
        board_t board;
        if (game_manager_get_board(&g_game_manager, move->game_id, &board) == SUCCESS) {
            result.game_over = board_is_game_over(&board);
            result.winner = board_get_winner(&board);
        } else {
            result.game_over = false;
            result.winner = NO_WINNER;
        }
        
        printf("🎲 Move: %s played pit %d in %s (captured: %d)\n", 
               session->pseudo, move->pit_index, move->game_id, seeds_captured);
    } else {
        strncpy(result.message, error_to_string(err), 255);
        result.game_over = false;
        result.winner = NO_WINNER;
    }
    
    result.message[255] = '\0';
    session_send_move_result(session, &result);
}

/* Handle MSG_GET_BOARD */
void handle_get_board(session_t* session, const msg_get_board_t* req) {
    msg_board_state_t board_msg;
    memset(&board_msg, 0, sizeof(board_msg));
    
    /* Find game by players */
    game_instance_t* game = game_manager_find_game_by_players(&g_game_manager, 
                                                               req->player_a, req->player_b);
    
    if (game) {
        board_msg.exists = true;
        strncpy(board_msg.game_id, game->game_id, MAX_GAME_ID_LEN - 1);
        board_msg.game_id[MAX_GAME_ID_LEN - 1] = '\0';
        strncpy(board_msg.player_a, game->player_a, MAX_PSEUDO_LEN - 1);
        board_msg.player_a[MAX_PSEUDO_LEN - 1] = '\0';
        strncpy(board_msg.player_b, game->player_b, MAX_PSEUDO_LEN - 1);
        board_msg.player_b[MAX_PSEUDO_LEN - 1] = '\0';
        
        pthread_mutex_lock(&game->lock);
        
        for (int i = 0; i < NUM_PITS; i++) {
            board_msg.pits[i] = game->board.pits[i];
        }
        board_msg.score_a = game->board.scores[0];
        board_msg.score_b = game->board.scores[1];
        board_msg.current_player = game->board.current_player;
        board_msg.state = game->board.state;
        board_msg.winner = game->board.winner;
        
        pthread_mutex_unlock(&game->lock);
    } else {
        board_msg.exists = false;
    }
    
    session_send_board_state(session, &board_msg);
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
    
    /* Main message loop */
    while (g_running && session_is_active(&session)) {
        message_type_t msg_type;
        char payload[MAX_PAYLOAD_SIZE];
        size_t payload_size;
        
        error_code_t err = session_recv_message(&session, &msg_type, payload, 
                                                MAX_PAYLOAD_SIZE, &payload_size);
        
        if (err != SUCCESS) {
            printf("Client %s disconnected\n", session.pseudo);
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
    matchmaking_remove_player(&g_matchmaking, session.pseudo);
    session_close(&session);
    free(handler);
    
    return NULL;
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
    printf("║         AWALE SERVER (New Architecture)              ║\n");
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
    
    printf("✓ Game manager initialized\n");
    printf("✓ Matchmaking initialized\n");
    
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
        
        /* Connect to client (write socket) */
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
        
        if (connect(client_conn.write_sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
            fprintf(stderr, "Failed to connect to client port %d\n", client_port);
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
