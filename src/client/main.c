/* Awale Client - Main Entry Point
 * Simplified main that uses modular components
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/common/types.h"
#include "../../include/common/protocol.h"
#include "../../include/common/messages.h"
#include "../../include/network/connection.h"
#include "../../include/network/session.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_ui.h"
#include "../../include/client/client_commands.h"
#include "../../include/client/client_notifications.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

/* Connection setup helper */
static error_code_t establish_connection(const char* pseudo, const char* server_ip, session_t* session);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <pseudo> [-s server_ip]\n", argv[0]);
        printf("  pseudo: Your player name\n");
        printf("  -s <server_ip> : Optional - directly connect to server IP instead of UDP discovery\n");
        printf("  If no server IP is provided the client will use UDP broadcast discovery.\n");
        return 1;
    }

    /* Parse optional args: allow -s server_ip before or after pseudo */
    const char* server_ip = NULL;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server-ip") == 0) && i + 1 < argc) {
            server_ip = argv[i + 1];
            i++; /* skip next */
            continue;
        }
    }

    /* The first non-option argument is the pseudo */
    const char* pseudo = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        pseudo = argv[i];
        break;
    }
    if (!pseudo) {
        printf("Il manque le pseudo. Usage: %s <pseudo> [-s server_ip]\n", argv[0]);
        return 1;
    }


    print_banner();
    client_state_set_pseudo(pseudo);
    printf("Player: %s\n", client_state_get_pseudo());
    
    /* Establish connection */
    session_t g_session;
    if (establish_connection(pseudo, server_ip, &g_session) != SUCCESS) {
        return 1;
    }
    
    /* Initialize client state */
    client_state_init(&g_session);
    
    /* Start notification listener thread */
    pthread_t notif_thread = start_notification_listener();
    printf("✓ Notification listener started\n");
    
    /* Main menu loop */
    client_state_set_running(true);
    while (client_state_is_running()) {
        print_menu();
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            clear_input();
            printf("Invalid input\n");
            continue;
        }
        clear_input();
        
        switch (choice) {
            case 1: cmd_list_players(); break;
            case 2: cmd_challenge_player(); break;
            case 3: cmd_view_challenges(); break;
            case 4:
                printf("\nDisconnecting...\n");
                session_send_message(&g_session, MSG_DISCONNECT, NULL, 0);
                client_state_set_running(false);
                break;
            case 5: cmd_play_mode(); break;
            case 6: cmd_spectator_mode(); break;
            default:
                printf("Invalid choice. Please select 1-6.\n");
                break;
        }
    }
    
    /* Cleanup */
    pthread_join(notif_thread, NULL);
    session_close(&g_session);
    printf("✓ Goodbye!\n\n");
    
    return 0;
}

/* Connection establishment (UDP discovery + bidirectional setup) */
static error_code_t establish_connection(const char* pseudo, const char* server_ip, session_t* session) {
    discovery_response_t discovery;
    error_code_t err = SUCCESS;

    if (server_ip == NULL) {
        printf("🔍 Broadcasting discovery request on local network...\n");
        /* Step 1: UDP broadcast to discover server */
        err = connection_broadcast_discovery(&discovery, 5);
        if (err != SUCCESS) {
            printf("❌ No server found on local network\n");
            return err;
        }
        printf("✓ Server discovered at %s:%d\n", discovery.server_ip, discovery.discovery_port);
    } else {
        /* Use provided server IP and default discovery port */
        printf("🔍 Using provided server IP: %s\n", server_ip);
        memset(&discovery, 0, sizeof(discovery));
        snprintf(discovery.server_ip, sizeof(discovery.server_ip), "%s", server_ip);
        discovery.discovery_port = 12345; /* default discovery port */
    }
    
    /* Connect to server discovery port (single socket for all communication) */
    connection_init(&session->conn);
    err = connection_connect(&session->conn, (server_ip != NULL) ? server_ip : discovery.server_ip, discovery.discovery_port);
    if (err != SUCCESS) {
        printf("❌ Failed to connect to server\n");
        return err;
    }
    
    printf("✓ Connected to server at %s:%d\n", (server_ip != NULL) ? server_ip : discovery.server_ip, discovery.discovery_port);

    /* Send MSG_CONNECT */
    msg_connect_t connect_msg;
    memset(&connect_msg, 0, sizeof(connect_msg));
    snprintf(connect_msg.pseudo, MAX_PSEUDO_LEN, "%s", pseudo);
    snprintf(connect_msg.version, 16, "%s", PROTOCOL_VERSION);
    
    err = session_send_message(session, MSG_CONNECT, &connect_msg, sizeof(connect_msg));
    if (err != SUCCESS) {
        printf("❌ Failed to send connect message\n");
        connection_close(&session->conn);
        return err;
    }
    
    /* Receive MSG_CONNECT_ACK */
    message_type_t type;
    msg_connect_ack_t ack;
    size_t size;
    
    err = session_recv_message(session, &type, &ack, sizeof(ack), &size);
    if (err != SUCCESS || type != MSG_CONNECT_ACK) {
        printf("❌ Failed to receive acknowledgment\n");
        connection_close(&session->conn);
        return ERR_NETWORK_ERROR;
    }
    
    if (!ack.success) {
        printf("❌ Connection rejected: %s\n", ack.message);
        connection_close(&session->conn);
        return ERR_NETWORK_ERROR;
    }
    
    printf("✓ %s\n", ack.message);
    snprintf(session->session_id, sizeof(session->session_id), "%s", ack.session_id);
    session->authenticated = true;
    
    return SUCCESS;
}
