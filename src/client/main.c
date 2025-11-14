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
#include "../../include/client/client_logging.h"
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
        client_log_error(CLIENT_LOG_USAGE, argv[0]);
        client_log_info(CLIENT_LOG_USAGE_PSEUDO);
        client_log_info(CLIENT_LOG_USAGE_SERVER);
        client_log_info(CLIENT_LOG_USAGE_DISCOVERY);
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
        client_log_error(CLIENT_LOG_MISSING_PSEUDO, argv[0]);
        return 1;
    }
    
    print_banner();
    client_state_set_pseudo(pseudo);
    client_log_info(CLIENT_LOG_PLAYER_NAME, client_state_get_pseudo());
    
    /* Establish connection */
    session_t g_session;
    if (establish_connection(pseudo, server_ip, &g_session) != SUCCESS) {
        return 1;
    }
    
    /* Initialize client state */
    client_state_init(&g_session);
    
    /* Start notification listener thread */
    pthread_t notif_thread = start_notification_listener();
    client_log_info(CLIENT_LOG_WAITING_NOTIFICATION);
    
    /* Main menu loop */
    client_state_set_running(true);
    int current_selection = 1;
    while (client_state_is_running()) {
        print_menu_highlighted(current_selection);

        int choice = read_arrow_input(&current_selection, 1, 13);
        if (choice == -1) { /* ESC pressed */
            client_log_info(CLIENT_LOG_DISCONNECTING);
            session_send_message(&g_session, MSG_DISCONNECT, NULL, 0);
            client_state_set_running(false);
            break;
        } else if (choice >= 1 && choice <= 13) {
            /* Valid choice selected */
            current_selection = choice;
        } else {
            /* Continue loop for navigation */
            continue;
        }

        switch (current_selection) {
            case 1: cmd_list_players(); break;
            case 2: cmd_challenge_player(); break;
            case 3: cmd_view_challenges(); break;
            case 4: cmd_profile(); break;
            case 5: cmd_play_mode(); break;
            case 6: cmd_chat(); break;
            case 7: cmd_spectator_mode(); break;
            case 8: cmd_friend_management(); break;
            case 9: cmd_list_saved_games(); break;
            case 10: cmd_view_saved_game(); break;
            case 11:
                client_log_info(CLIENT_LOG_DISCONNECTING);
                session_send_message(&g_session, MSG_DISCONNECT, NULL, 0);
                client_state_set_running(false);
                break;
            case 12: cmd_tutorial(); break;
            case 13: cmd_start_ai_game(); break;
            default:
                client_log_warning(CLIENT_LOG_INVALID_CHOICE);
                break;
        }

        /* Pause after command execution to keep output visible */
        printf("Press Enter to return to menu...\n");
        char buffer[256];
        read_line(buffer, sizeof(buffer));
    }
    
    /* Cleanup */
    pthread_join(notif_thread, NULL);
    session_close(&g_session);
    client_log_info(CLIENT_LOG_GOODBYE);
    
    return 0;
}

/* Connection establishment (UDP discovery + bidirectional setup) */
static error_code_t establish_connection(const char* pseudo, const char* server_ip, session_t* session) {
    discovery_response_t discovery;
    error_code_t err = SUCCESS;

    if (server_ip == NULL) {
        client_log_info(CLIENT_LOG_BROADCAST_DISCOVERY);
        /* Step 1: UDP broadcast to discover server */
        err = connection_broadcast_discovery(&discovery, 5);
        if (err != SUCCESS) {
            client_log_error(CLIENT_LOG_NO_SERVER_FOUND);
            return err;
        }
        client_log_info(CLIENT_LOG_SERVER_DISCOVERED, discovery.server_ip, discovery.discovery_port);
    } else {
        /* Use provided server IP and default discovery port */
        client_log_info(CLIENT_LOG_USING_PROVIDED_IP, server_ip);
        memset(&discovery, 0, sizeof(discovery));
        snprintf(discovery.server_ip, sizeof(discovery.server_ip), "%s", server_ip);
        discovery.discovery_port = 12345; /* default discovery port */
    }
    
    /* Connect to server discovery port (single socket for all communication) */
    connection_init(&session->conn);
    err = connection_connect(&session->conn, (server_ip != NULL) ? server_ip : discovery.server_ip, discovery.discovery_port);
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_CONNECTION_FAILED);
        return err;
    }
    
    client_log_info(CLIENT_LOG_CONNECTED, (server_ip != NULL) ? server_ip : discovery.server_ip, discovery.discovery_port);

    /* Send MSG_CONNECT */
    msg_connect_t connect_msg;
    memset(&connect_msg, 0, sizeof(connect_msg));
    snprintf(connect_msg.pseudo, MAX_PSEUDO_LEN, "%s", pseudo);
    snprintf(connect_msg.version, 16, "%s", PROTOCOL_VERSION);
    
    err = session_send_message(session, MSG_CONNECT, &connect_msg, sizeof(connect_msg));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_SEND_CONNECT_FAILED);
        connection_close(&session->conn);
        return err;
    }
    
    /* Receive MSG_CONNECT_ACK */
    message_type_t type;
    msg_connect_ack_t ack;
    size_t size;

    err = session_recv_message_timeout(session, &type, &ack, sizeof(ack), &size, 10000, NULL, 0);  /* 10 second timeout */
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        connection_close(&session->conn);
        return err;
    }
    if (err != SUCCESS || type != MSG_CONNECT_ACK) {
        client_log_error(CLIENT_LOG_RECV_ACK_FAILED);
        connection_close(&session->conn);
        return ERR_NETWORK_ERROR;
    }
    
    if (!ack.success) {
        client_log_error(CLIENT_LOG_CONNECTION_REJECTED, ack.message);
        connection_close(&session->conn);
        return ERR_NETWORK_ERROR;
    }
    
    client_log_info(CLIENT_LOG_CONNECTION_SUCCESS, ack.message);
    snprintf(session->session_id, sizeof(session->session_id), "%s", ack.session_id);
    session->authenticated = true;
    
    return SUCCESS;
}
