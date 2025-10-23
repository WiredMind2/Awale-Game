/* Awale Client - Full CLI Implementation
 * Interactive command-line client for the Awale game
 */

#include "../../include/common/types.h"
#include "../../include/common/protocol.h"
#include "../../include/common/messages.h"
#include "../../include/network/connection.h"
#include "../../include/network/session.h"
#include "../../include/game/board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <pthread.h>

/* Use protocol-defined payload size */
#ifndef MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD_SIZE MAX_MESSAGE_SIZE
#endif

/* Global session */
static session_t g_session;
static char g_pseudo[MAX_PSEUDO_LEN];
static volatile bool g_running = true;

/* Pending challenges tracking */
#define MAX_PENDING_CHALLENGES 10
typedef struct {
    char challenger[MAX_PSEUDO_LEN];
    int64_t challenge_id;
    bool active;
} pending_challenge_t;

static struct {
    pending_challenge_t challenges[MAX_PENDING_CHALLENGES];
    int count;
    pthread_mutex_t lock;
} g_pending_challenges;

/* Forward declarations */
void pending_challenges_init();
void pending_challenges_add(const char* challenger, int64_t challenge_id);
void pending_challenges_remove(const char* challenger);
int pending_challenges_count();
void* notification_listener(void* arg);

/* UI Functions */
void print_banner() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║            AWALE GAME - CLI Client                   ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void print_menu() {
    int pending = pending_challenges_count();
    
    printf("\n");
    printf("═════════════════════════════════════════════════════════\n");
    printf("                    MAIN MENU                            \n");
    printf("═════════════════════════════════════════════════════════\n");
    printf("  1. List connected players\n");
    printf("  2. Challenge a player\n");
    printf("  3. View/Respond to challenges");
    if (pending > 0) {
        printf(" 🔔 [%d pending]", pending);
    }
    printf("\n");
    printf("  4. Play a move\n");
    printf("  5. View game board\n");
    printf("  6. Quit\n");
    printf("═════════════════════════════════════════════════════════\n");
    printf("Your choice: ");
}

void print_board(const msg_board_state_t* board) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("                    PLATEAU AWALE                          \n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Joueur B: %s (Score: %d)                    %s\n", 
           board->player_b, board->score_b,
           (board->current_player == PLAYER_B) ? "← À TOI!" : "");
    printf("\n");
    
    printf("   ┌────┬────┬────┬────┬────┬────┐\n");
    printf("   │%2d  │%2d  │%2d  │%2d  │%2d  │%2d  │\n",
           board->pits[11], board->pits[10], board->pits[9], 
           board->pits[8], board->pits[7], board->pits[6]);
    printf("   │ 11 │ 10 │ 9  │ 8  │ 7  │ 6  │\n");
    printf("   ├────┼────┼────┼────┼────┼────┤\n");
    printf("   │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │\n");
    printf("   │%2d  │%2d  │%2d  │%2d  │%2d  │%2d  │\n",
           board->pits[0], board->pits[1], board->pits[2], 
           board->pits[3], board->pits[4], board->pits[5]);
    printf("   └────┴────┴────┴────┴────┴────┘\n");
    printf("\n");
    printf("%s Joueur A: %s (Score: %d)\n", 
           (board->current_player == PLAYER_A) ? "À TOI! →" : "",
           board->player_a, board->score_a);
    printf("═══════════════════════════════════════════════════════════\n");
    
    if (board->state == GAME_STATE_FINISHED) {
        printf("🏁 PARTIE TERMINÉE - ");
        if (board->winner == WINNER_A) {
            printf("%s gagne!\n", board->player_a);
        } else if (board->winner == WINNER_B) {
            printf("%s gagne!\n", board->player_b);
        } else {
            printf("Match nul!\n");
        }
    } else {
        printf("Tour du joueur: %s\n", 
               (board->current_player == PLAYER_A) ? board->player_a : board->player_b);
        
        if (board->current_player == PLAYER_A) {
            printf("💡 %s peut jouer les fosses 0 à 5 (rangée du bas)\n", board->player_a);
        } else {
            printf("💡 %s peut jouer les fosses 6 à 11 (rangée du haut)\n", board->player_b);
        }
    }
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
}

void clear_input() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

char* read_line(char* buffer, size_t size) {
    if (fgets(buffer, size, stdin) == NULL) {
        return NULL;
    }
    
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    return buffer;
}

/* Pending challenges management */
void pending_challenges_init() {
    pthread_mutex_init(&g_pending_challenges.lock, NULL);
    g_pending_challenges.count = 0;
    for (int i = 0; i < MAX_PENDING_CHALLENGES; i++) {
        g_pending_challenges.challenges[i].active = false;
    }
}

void pending_challenges_add(const char* challenger, int64_t challenge_id) {
    pthread_mutex_lock(&g_pending_challenges.lock);
    for (int i = 0; i < MAX_PENDING_CHALLENGES; i++) {
        if (!g_pending_challenges.challenges[i].active) {
            snprintf(g_pending_challenges.challenges[i].challenger, MAX_PSEUDO_LEN, "%s", challenger);
            g_pending_challenges.challenges[i].challenge_id = challenge_id;
            g_pending_challenges.challenges[i].active = true;
            g_pending_challenges.count++;
            break;
        }
    }
    pthread_mutex_unlock(&g_pending_challenges.lock);
}

void pending_challenges_remove(const char* challenger) {
    pthread_mutex_lock(&g_pending_challenges.lock);
    for (int i = 0; i < MAX_PENDING_CHALLENGES; i++) {
        if (g_pending_challenges.challenges[i].active &&
            strcmp(g_pending_challenges.challenges[i].challenger, challenger) == 0) {
            g_pending_challenges.challenges[i].active = false;
            g_pending_challenges.count--;
            break;
        }
    }
    pthread_mutex_unlock(&g_pending_challenges.lock);
}

int pending_challenges_count() {
    pthread_mutex_lock(&g_pending_challenges.lock);
    int count = g_pending_challenges.count;
    pthread_mutex_unlock(&g_pending_challenges.lock);
    return count;
}

/* Notification listener thread */
void* notification_listener(void* arg) {
    (void)arg;
    
    while (g_running) {
        message_type_t type;
        char payload[MAX_PAYLOAD_SIZE];
        size_t size;
        
        /* Wait for notifications with 1 second timeout to allow graceful exit */
        error_code_t err = session_recv_message_timeout(&g_session, &type, payload, 
                                                        MAX_PAYLOAD_SIZE, &size, 1000);
        
        if (err == ERR_TIMEOUT) {
            continue;  /* Normal timeout, continue listening */
        }
        
        if (err != SUCCESS) {
            if (g_running) {
                printf("\n❌ Connection lost\n");
            }
            break;
        }
        
        /* Handle push notifications */
        if (type == MSG_CHALLENGE_RECEIVED) {
            msg_challenge_received_t* notif = (msg_challenge_received_t*)payload;
            pending_challenges_add(notif->from, notif->challenge_id);
            printf("\n\n🔔 ═══════════════════════════════════════════════════\n");
            printf("   CHALLENGE RECEIVED!\n");
            printf("   %s\n", notif->message);
            printf("   Use option 3 to accept or decline this challenge\n");
            printf("═══════════════════════════════════════════════════\n");
            printf("Your choice: ");
            fflush(stdout);
        } else if (type == MSG_GAME_STARTED) {
            msg_game_started_t* start = (msg_game_started_t*)payload;
            printf("\n\n🎮 ═══════════════════════════════════════════════════\n");
            printf("   GAME STARTED!\n");
            printf("   Game ID: %s\n", start->game_id);
            printf("   Players: %s vs %s\n", start->player_a, start->player_b);
            printf("   You are: %s\n", (start->your_side == PLAYER_A) ? "Player A" : "Player B");
            printf("   Use option 5 to view the board, option 4 to play moves\n");
            printf("═══════════════════════════════════════════════════\n");
            printf("Your choice: ");
            fflush(stdout);
        }
    }
    
    return NULL;
}

/* Command handlers */
void cmd_list_players() {
    printf("\n📋 Listing connected players...\n");
    
    error_code_t err = session_send_message(&g_session, MSG_LIST_PLAYERS, NULL, 0);
    if (err != SUCCESS) {
        printf("❌ Error sending request: %s\n", error_to_string(err));
        return;
    }
    
    message_type_t type;
    msg_player_list_t list;
    size_t size;
    
    err = session_recv_message(&g_session, &type, &list, sizeof(list), &size);
    if (err != SUCCESS || type != MSG_PLAYER_LIST) {
        printf("❌ Error receiving response\n");
        return;
    }
    
    printf("\n✓ Connected players (%d):\n", list.count);
    printf("─────────────────────────────\n");
    for (int i = 0; i < list.count; i++) {
        printf("  %d. %s (%s)\n", i + 1, list.players[i].pseudo, list.players[i].ip);
    }
    printf("─────────────────────────────\n");
}

void cmd_challenge() {
    char opponent[MAX_PSEUDO_LEN];
    
    printf("\n⚔️  Challenge a player\n");
    printf("Enter opponent's pseudo: ");
    
    if (read_line(opponent, MAX_PSEUDO_LEN) == NULL) {
        printf("❌ Invalid input\n");
        return;
    }
    
    if (strlen(opponent) == 0) {
        printf("❌ Pseudo cannot be empty\n");
        return;
    }
    
    if (strcmp(opponent, g_pseudo) == 0) {
        printf("❌ You cannot challenge yourself!\n");
        return;
    }
    
    msg_challenge_t challenge;
    memset(&challenge, 0, sizeof(challenge));
    snprintf(challenge.challenger, MAX_PSEUDO_LEN, "%s", g_pseudo);
    snprintf(challenge.opponent, MAX_PSEUDO_LEN, "%s", opponent);
    
    error_code_t err = session_send_message(&g_session, MSG_CHALLENGE, &challenge, sizeof(challenge));
    if (err != SUCCESS) {
        printf("❌ Error sending challenge: %s\n", error_to_string(err));
        return;
    }
    
    /* Wait for acknowledgment - server responds immediately now */
    message_type_t type;
    char response[MAX_PAYLOAD_SIZE];
    size_t size;
    
    err = session_recv_message_timeout(&g_session, &type, response, MAX_PAYLOAD_SIZE, &size, 5000);
    if (err == ERR_TIMEOUT) {
        printf("❌ Timeout: Server did not respond\n");
        return;
    }
    if (err != SUCCESS) {
        printf("❌ Error receiving response: %s\n", error_to_string(err));
        return;
    }
    
    if (type == MSG_CHALLENGE_SENT) {
        printf("✓ Challenge sent to %s!\n", opponent);
        printf("💡 They will receive a notification. Wait for them to accept or decline.\n");
    } else if (type == MSG_ERROR) {
        msg_error_t* error = (msg_error_t*)response;
        printf("❌ Error: %s\n", error->error_msg);
    }
}

void cmd_view_challenges() {
    pthread_mutex_lock(&g_pending_challenges.lock);
    
    if (g_pending_challenges.count == 0) {
        pthread_mutex_unlock(&g_pending_challenges.lock);
        printf("\n✓ No pending challenges\n");
        return;
    }
    
    printf("\n📨 Pending challenges (%d):\n", g_pending_challenges.count);
    printf("═══════════════════════════════════════════════════\n");
    
    /* Display challenges */
    int displayed = 0;
    for (int i = 0; i < MAX_PENDING_CHALLENGES; i++) {
        if (g_pending_challenges.challenges[i].active) {
            printf("  %d. %s wants to play!\n", displayed + 1, 
                   g_pending_challenges.challenges[i].challenger);
            displayed++;
        }
    }
    
    pthread_mutex_unlock(&g_pending_challenges.lock);
    
    printf("═══════════════════════════════════════════════════\n");
    printf("\nChoose an option:\n");
    printf("  [number] Accept challenge\n");
    printf("  d[number] Decline challenge\n");
    printf("  0 Cancel\n");
    printf("Your choice: ");
    
    char choice[32];
    if (read_line(choice, 32) == NULL || strlen(choice) == 0) {
        return;
    }
    
    /* Parse choice */
    bool is_decline = (choice[0] == 'd' || choice[0] == 'D');
    int index = atoi(is_decline ? choice + 1 : choice);
    
    if (index == 0) {
        printf("Cancelled.\n");
        return;
    }
    
    if (index < 1 || index > displayed) {
        printf("❌ Invalid choice\n");
        return;
    }
    
    /* Find the selected challenge */
    pthread_mutex_lock(&g_pending_challenges.lock);
    char selected_challenger[MAX_PSEUDO_LEN];
    int found_index = 0;
    bool found = false;
    
    for (int i = 0; i < MAX_PENDING_CHALLENGES; i++) {
        if (g_pending_challenges.challenges[i].active) {
            found_index++;
            if (found_index == index) {
                snprintf(selected_challenger, MAX_PSEUDO_LEN, "%s", 
                         g_pending_challenges.challenges[i].challenger);
                found = true;
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_pending_challenges.lock);
    
    if (!found) {
        printf("❌ Challenge not found\n");
        return;
    }
    
    /* Send accept or decline */
    msg_challenge_response_t response;
    memset(&response, 0, sizeof(response));
    snprintf(response.challenger, MAX_PSEUDO_LEN, "%s", selected_challenger);
    
    message_type_t msg_type = is_decline ? MSG_DECLINE_CHALLENGE : MSG_ACCEPT_CHALLENGE;
    error_code_t err = session_send_message(&g_session, msg_type, &response, sizeof(response));
    
    if (err != SUCCESS) {
        printf("❌ Error sending response: %s\n", error_to_string(err));
        return;
    }
    
    /* Remove from pending */
    pending_challenges_remove(selected_challenger);
    
    if (is_decline) {
        printf("✓ Challenge from %s declined\n", selected_challenger);
    } else {
        printf("✓ Challenge from %s accepted! Game will start shortly...\n", selected_challenger);
        /* MSG_GAME_STARTED will be received by notification thread */
    }
}

void cmd_play_move() {
    char player_a[MAX_PSEUDO_LEN], player_b[MAX_PSEUDO_LEN];
    int pit;
    
    printf("\n🎲 Play a move\n");
    printf("Enter Player A pseudo: ");
    if (read_line(player_a, MAX_PSEUDO_LEN) == NULL) return;
    
    printf("Enter Player B pseudo: ");
    if (read_line(player_b, MAX_PSEUDO_LEN) == NULL) return;
    
    /* First show the board */
    msg_get_board_t board_req;
    memset(&board_req, 0, sizeof(board_req));
    snprintf(board_req.player_a, MAX_PSEUDO_LEN, "%s", player_a);
    snprintf(board_req.player_b, MAX_PSEUDO_LEN, "%s", player_b);
    
    session_send_message(&g_session, MSG_GET_BOARD, &board_req, sizeof(board_req));
    
    message_type_t type;
    msg_board_state_t board;
    size_t size;
    
    if (session_recv_message(&g_session, &type, &board, sizeof(board), &size) == SUCCESS &&
        type == MSG_BOARD_STATE && board.exists) {
        print_board(&board);
    } else {
        printf("❌ Game not found between %s and %s\n", player_a, player_b);
        return;
    }
    
    printf("Enter pit number (0-11): ");
    if (scanf("%d", &pit) != 1) {
        clear_input();
        printf("❌ Invalid input\n");
        return;
    }
    clear_input();
    
    if (pit < 0 || pit >= 12) {
        printf("❌ Pit must be between 0 and 11\n");
        return;
    }
    
    /* Send move */
    msg_play_move_t move;
    memset(&move, 0, sizeof(move));
    snprintf(move.game_id, MAX_GAME_ID_LEN, "%s", board.game_id);
    snprintf(move.player, MAX_PSEUDO_LEN, "%s", g_pseudo);
    move.pit_index = pit;
    
    error_code_t err = session_send_message(&g_session, MSG_PLAY_MOVE, &move, sizeof(move));
    if (err != SUCCESS) {
        printf("❌ Error sending move: %s\n", error_to_string(err));
        return;
    }
    
    msg_move_result_t result;
    err = session_recv_message(&g_session, &type, &result, sizeof(result), &size);
    if (err != SUCCESS || type != MSG_MOVE_RESULT) {
        printf("❌ Error receiving response\n");
        return;
    }
    
    if (result.success) {
        printf("\n✓ Move played successfully!\n");
        if (result.seeds_captured > 0) {
            printf("⭐ Captured %d seeds!\n", result.seeds_captured);
        }
        
        if (result.game_over) {
            printf("\n🏁 GAME OVER!\n");
            if (result.winner == WINNER_A) {
                printf("Winner: %s\n", player_a);
            } else if (result.winner == WINNER_B) {
                printf("Winner: %s\n", player_b);
            } else {
                printf("Result: Draw\n");
            }
        }
        
        /* Show updated board */
        printf("\nUpdated board:\n");
        session_send_message(&g_session, MSG_GET_BOARD, &board_req, sizeof(board_req));
        if (session_recv_message(&g_session, &type, &board, sizeof(board), &size) == SUCCESS &&
            type == MSG_BOARD_STATE) {
            print_board(&board);
        }
    } else {
        printf("\n❌ Move failed: %s\n", result.message);
    }
}

void cmd_view_board() {
    char player_a[MAX_PSEUDO_LEN], player_b[MAX_PSEUDO_LEN];
    
    printf("\n👁️  View game board\n");
    printf("Enter Player A pseudo: ");
    if (read_line(player_a, MAX_PSEUDO_LEN) == NULL) return;
    
    printf("Enter Player B pseudo: ");
    if (read_line(player_b, MAX_PSEUDO_LEN) == NULL) return;
    
    msg_get_board_t req;
    memset(&req, 0, sizeof(req));
    snprintf(req.player_a, MAX_PSEUDO_LEN, "%s", player_a);
    snprintf(req.player_b, MAX_PSEUDO_LEN, "%s", player_b);
    
    error_code_t err = session_send_message(&g_session, MSG_GET_BOARD, &req, sizeof(req));
    if (err != SUCCESS) {
        printf("❌ Error sending request: %s\n", error_to_string(err));
        return;
    }
    
    message_type_t type;
    msg_board_state_t board;
    size_t size;
    
    err = session_recv_message(&g_session, &type, &board, sizeof(board), &size);
    if (err != SUCCESS || type != MSG_BOARD_STATE) {
        printf("❌ Error receiving response\n");
        return;
    }
    
    if (board.exists) {
        print_board(&board);
    } else {
        printf("\n❌ No game found between %s and %s\n", player_a, player_b);
        printf("💡 Use option 2 to challenge them!\n");
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <pseudo>\n", argv[0]);
        printf("  pseudo: Your player name\n");
        printf("  Server will be discovered automatically via UDP broadcast.\n");
        return 1;
    }
    
    strncpy(g_pseudo, argv[1], MAX_PSEUDO_LEN - 1);
    g_pseudo[MAX_PSEUDO_LEN - 1] = '\0';
    
    print_banner();
    printf("Player: %s\n", g_pseudo);
    printf("🔍 Broadcasting discovery request on local network...\n");
    
    /* Step 1: UDP broadcast to discover server */
    discovery_response_t discovery;
    error_code_t err = connection_broadcast_discovery(&discovery, 5);  /* 5 second timeout */
    if (err != SUCCESS) {
        printf("❌ No server found on local network\n");
        printf("   Make sure the server is running and on the same network.\n");
        return 1;
    }
    
    printf("✓ Server discovered at %s:%d\n", discovery.server_ip, discovery.discovery_port);
    
    /* Step 2: Connect to discovery port */
    connection_t discovery_conn;
    connection_init(&discovery_conn);
    
    discovery_conn.read_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (discovery_conn.read_sockfd < 0) {
        printf("❌ Failed to create socket\n");
        return 1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(discovery.discovery_port);
    inet_pton(AF_INET, discovery.server_ip, &server_addr.sin_addr);
    
    if (connect(discovery_conn.read_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("❌ Failed to connect to server\n");
        close(discovery_conn.read_sockfd);
        return 1;
    }
    
    /* Mark connection as connected and set write socket */
    discovery_conn.write_sockfd = discovery_conn.read_sockfd;  /* Same socket for both directions */
    discovery_conn.connected = true;
    
    printf("✓ Connected to server\n");
    
    /* Step 3: Find a free port for client to listen on */
    int client_port;
    err = connection_find_free_port(&client_port);
    if (err != SUCCESS) {
        printf("❌ Failed to find free port\n");
        connection_close(&discovery_conn);
        return 1;
    }
    
    printf("   Client will listen on port: %d\n", client_port);
    
    /* Step 4: Send client's listening port to server */
    msg_port_negotiation_t client_port_msg;
    client_port_msg.my_port = htonl(client_port);
    
    err = connection_send_raw(&discovery_conn, &client_port_msg, sizeof(client_port_msg));
    if (err != SUCCESS) {
        printf("❌ Failed to send port to server\n");
        connection_close(&discovery_conn);
        return 1;
    }
    
    /* Step 5: Receive server's listening port */
    msg_port_negotiation_t server_port_msg;
    size_t received;
    err = connection_recv_raw(&discovery_conn, &server_port_msg, sizeof(server_port_msg), &received);
    if (err != SUCCESS || received != sizeof(server_port_msg)) {
        printf("❌ Failed to receive server port\n");
        connection_close(&discovery_conn);
        return 1;
    }
    
    int server_port = ntohl(server_port_msg.my_port);
    printf("   Server listening on port: %d\n", server_port);
    
    /* Close discovery connection */
    connection_close(&discovery_conn);
    
    /* Step 6: Create client listening socket */
    connection_t client_listen;
    if (connection_init(&client_listen) != SUCCESS ||
        connection_create_discovery_server(&client_listen, client_port) != SUCCESS) {
        printf("❌ Failed to create client listening socket\n");
        return 1;
    }
    
    /* Step 7: Connect to server's listening port (write socket) */
    session_init(&g_session);
    connection_init(&g_session.conn);
    
    g_session.conn.write_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_session.conn.write_sockfd < 0) {
        printf("❌ Failed to create socket\n");
        connection_close(&client_listen);
        return 1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, discovery.server_ip, &server_addr.sin_addr);
    
    if (connect(g_session.conn.write_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("❌ Failed to connect to server port %d\n", server_port);
        close(g_session.conn.write_sockfd);
        connection_close(&client_listen);
        return 1;
    }
    
    printf("   Connected to server's port\n");
    
    /* Step 8: Accept server's connection to our listening port (read socket) */
    connection_t temp_conn;
    err = connection_accept_discovery(&client_listen, &temp_conn);
    if (err != SUCCESS) {
        printf("❌ Failed to accept connection from server\n");
        close(g_session.conn.write_sockfd);
        connection_close(&client_listen);
        return 1;
    }
    
    g_session.conn.read_sockfd = temp_conn.read_sockfd;
    g_session.conn.connected = true;
    g_session.conn.sequence = 0;
    
    /* Close client listening socket (no longer needed) */
    connection_close(&client_listen);
    
    printf("✓ Bidirectional connection established\n");
    
    /* Send MSG_CONNECT */
    msg_connect_t connect_msg;
    memset(&connect_msg, 0, sizeof(connect_msg));
    snprintf(connect_msg.pseudo, MAX_PSEUDO_LEN, "%s", g_pseudo);
    snprintf(connect_msg.version, 16, "%s", PROTOCOL_VERSION);
    
    err = session_send_message(&g_session, MSG_CONNECT, &connect_msg, sizeof(connect_msg));
    if (err != SUCCESS) {
        printf("❌ Failed to send connect message\n");
        connection_close(&g_session.conn);
        return 1;
    }
    
    /* Receive MSG_CONNECT_ACK */
    message_type_t type;
    msg_connect_ack_t ack;
    size_t size;
    
    err = session_recv_message(&g_session, &type, &ack, sizeof(ack), &size);
    if (err != SUCCESS || type != MSG_CONNECT_ACK) {
        printf("❌ Failed to receive acknowledgment\n");
        connection_close(&g_session.conn);
        return 1;
    }
    
    if (!ack.success) {
        printf("❌ Connection rejected: %s\n", ack.message);
        connection_close(&g_session.conn);
        return 1;
    }
    
    printf("✓ %s\n", ack.message);
    snprintf(g_session.session_id, sizeof(g_session.session_id), "%s", ack.session_id);
    g_session.authenticated = true;
    
    /* Initialize pending challenges tracking */
    pending_challenges_init();
    
    /* Start notification listener thread */
    pthread_t notif_thread;
    if (pthread_create(&notif_thread, NULL, notification_listener, NULL) != 0) {
        printf("❌ Failed to start notification listener\n");
        connection_close(&g_session.conn);
        return 1;
    }
    
    printf("✓ Notification listener started\n");
    
    /* Main loop */
    g_running = true;
    while (g_running) {
        print_menu();
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            clear_input();
            printf("❌ Invalid input\n");
            continue;
        }
        clear_input();
        
        switch (choice) {
            case 1:
                cmd_list_players();
                break;
                
            case 2:
                cmd_challenge();
                break;
                
            case 3:
                cmd_view_challenges();
                break;
                
            case 4:
                cmd_play_move();
                break;
                
            case 5:
                cmd_view_board();
                break;
                
            case 6:
                printf("\n👋 Disconnecting...\n");
                session_send_message(&g_session, MSG_DISCONNECT, NULL, 0);
                g_running = false;
                break;
                
            default:
                printf("❌ Invalid choice. Please select 1-6.\n");
                break;
        }
    }
    
    /* Wait for notification thread to finish */
    pthread_join(notif_thread, NULL);
    
    session_close(&g_session);
    printf("✓ Goodbye!\n\n");
    
    return 0;
}
