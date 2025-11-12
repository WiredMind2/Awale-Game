/* Client Notifications - Background Notification Listener
 * Handles incoming server notifications in separate thread
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_notifications.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_ui.h"
#include "../../include/common/messages.h"
#include "../../include/common/protocol.h"
#include "../../include/network/session.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifndef MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD_SIZE MAX_MESSAGE_SIZE
#endif

/* Notification listener thread */
void* notification_listener(void* arg) {
    (void)arg;
    session_t* session = client_state_get_session();
    
    while (client_state_is_running()) {
        message_type_t type;
        char payload[MAX_PAYLOAD_SIZE];
        size_t size;
        
        /* Wait for notifications with 1 second timeout to allow graceful exit */
        error_code_t err = session_recv_message_timeout(session, &type, payload, 
                                                        MAX_PAYLOAD_SIZE, &size, 1000);
        
        if (err == ERR_TIMEOUT) {
            continue;  /* Normal timeout, continue listening */
        }
        
        if (err != SUCCESS) {
            if (client_state_is_running()) {
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
            
            /* Add to active games */
            active_games_add(start->game_id, start->player_a, start->player_b, start->your_side);
            
            printf("\n\n🎮 ═══════════════════════════════════════════════════\n");
            printf("   GAME STARTED!\n");
            printf("   Game ID: %s\n", start->game_id);
            printf("   Players: %s vs %s\n", start->player_a, start->player_b);
            printf("   You are: %s\n", (start->your_side == PLAYER_A) ? "Player A" : "Player B");
            printf("   Use option 7 to enter play mode\n");
            printf("═══════════════════════════════════════════════════\n");
            printf("Your choice: ");
            fflush(stdout);
        } else if (type == MSG_MOVE_RESULT) {
            /* Notify that a move happened */
            active_games_notify_turn();
            
            /* If we're spectating, notify board update */
            if (spectator_state_is_active()) {
                spectator_state_notify_update();
            }
        } else if (type == MSG_SPECTATOR_JOINED) {
            msg_spectator_joined_t* notif = (msg_spectator_joined_t*)payload;
            printf("\n\n👁️ ═══════════════════════════════════════════════════\n");
            printf("   SPECTATOR JOINED: %s\n", notif->spectator);
            printf("   Game ID: %s\n", notif->game_id);
            printf("   Total spectators: %d\n", notif->spectator_count);
            printf("═══════════════════════════════════════════════════\n");
            printf("Your choice: ");
            fflush(stdout);
        } else if (type == MSG_GAME_OVER) {
            msg_game_over_t* game_over = (msg_game_over_t*)payload;
            active_games_remove(game_over->game_id);
            printf("\n\n🏁 ═══════════════════════════════════════════════════\n");
            printf("   GAME OVER!\n");
            printf("   %s\n", game_over->message);
            printf("═══════════════════════════════════════════════════\n");
            printf("Your choice: ");
            fflush(stdout);
        } else if (type == MSG_CHAT_MESSAGE) {
            msg_chat_message_t* chat = (msg_chat_message_t*)payload;
            printf("\n\n💬 ═══════════════════════════════════════════════════\n");
            if (strlen(chat->recipient) == 0) {
                printf("   GLOBAL CHAT from %s:\n", chat->sender);
            } else {
                printf("   PRIVATE MESSAGE from %s:\n", chat->sender);
            }
            printf("   %s\n", chat->message);
            printf("═══════════════════════════════════════════════════\n");
            printf("Your choice: ");
            fflush(stdout);
        }
    }
    
    return NULL;
}

/* Start notification listener thread */
pthread_t start_notification_listener(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, notification_listener, NULL);
    return thread;
}
