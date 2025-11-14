/* Client Notifications - Background Notification Listener
 * Handles incoming server notifications in separate thread
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_notifications.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_ui.h"
#include "../../include/client/client_logging.h"
#include "../../include/common/messages.h"
#include "../../include/common/protocol.h"
#include "../../include/network/session.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#ifndef MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD_SIZE MAX_MESSAGE_SIZE
#endif

/* Notification listener thread */
void* notification_listener(void* arg) {
    (void)arg;
    session_t* session = client_state_get_session();
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 3;
    
    while (client_state_is_running()) {
        message_type_t type;

        /* Peek at the message type without consuming it */
        error_code_t err = session_peek_message_type(session, &type, 1000);

        if (err == ERR_TIMEOUT) {
            consecutive_errors = 0;  /* Reset error counter on successful timeout */
            continue;  /* Normal timeout, continue listening */
        }

        if (err != SUCCESS) {
            consecutive_errors++;

            if (client_state_is_running()) {
                if (err == ERR_NETWORK_ERROR) {
                    /* Connection is broken - stop the client */
                    ui_display_connection_lost();
                    client_state_set_running(false);

                    /* Force stop the client */
                    raise(SIGTERM); /* send SIGTERM to the process to terminate immediately */

                    break;
                } else {
                    /* Other error - log and retry */
                    ui_display_network_error(error_to_string(err), consecutive_errors, MAX_CONSECUTIVE_ERRORS);

                    if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                        break;
                    }

                    /* Brief delay before retry */
                    struct timespec ts = {0, 500000000};  /* 500ms */
                    nanosleep(&ts, NULL);
                    continue;
                }
            }
            break;
        }

        /* Check if this is a notification message */
        if (IS_NOTIFICATION_MESSAGE(type)) {

            /* This is a notification, consume and process it */
            char payload[MAX_PAYLOAD_SIZE];
            size_t size;

            err = session_recv_message_timeout(session, &type, payload, MAX_PAYLOAD_SIZE, &size, 1000, NULL, 0);
            if (err != SUCCESS) {
                /* Message may have been consumed by main thread between peek and recv - not an error */
                consecutive_errors = 0;  /* Reset error counter since this is not a network error */
                continue;
            }

            /* Reset error counter on successful receive */
            consecutive_errors = 0;

            /* Handle the notification */
            handle_notification_message(type, payload, size);
        } else {
            /* Not a notification message - leave it for the main thread */
            consecutive_errors = 0;  /* Reset on successful peek */
            continue;
        }
    }
    
    return NULL;
}

/* Send challenge accept message */
error_code_t send_challenge_accept(int64_t challenge_id) {
    session_t* session = client_state_get_session();
    msg_challenge_accept_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.challenge_id = challenge_id;
    /* response can be empty for now */

    return session_send_message(session, MSG_CHALLENGE_ACCEPT, &msg, sizeof(msg));
}

/* Send challenge decline message */
error_code_t send_challenge_decline(int64_t challenge_id) {
    session_t* session = client_state_get_session();
    msg_challenge_decline_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.challenge_id = challenge_id;
    /* response can be empty for now */

    return session_send_message(session, MSG_CHALLENGE_DECLINE, &msg, sizeof(msg));
}

/* Handle a notification message */
void handle_notification_message(message_type_t type, const void* payload, size_t size) {
    (void)size;  /* Size is not used in handling */

    /* Handle push notifications */
    if (type == MSG_CHALLENGE_RECEIVED) {
        msg_challenge_received_t* notif = (msg_challenge_received_t*)payload;
        pending_challenges_add(notif->from, notif->challenge_id);
        ui_display_challenge_received(notif);
    } else if (type == MSG_GAME_STARTED) {
        msg_game_started_t* start = (msg_game_started_t*)payload;

        /* Add to active games */
        active_games_add(start->game_id, start->player_a, start->player_b, start->your_side);

        ui_display_game_started(start);
    } else if (type == MSG_MOVE_RESULT) {
        /* Notify that a move happened */
        active_games_notify_turn();

        /* If we're spectating, notify board update */
        if (spectator_state_is_active()) {
            spectator_state_notify_update();
        }
    } else if (type == MSG_SPECTATOR_JOINED) {
        msg_spectator_joined_t* notif = (msg_spectator_joined_t*)payload;
        ui_display_spectator_joined(notif);
    } else if (type == MSG_GAME_OVER) {
        msg_game_over_t* game_over = (msg_game_over_t*)payload;
        active_games_remove(game_over->game_id);
        ui_display_game_over(game_over);
    } else if (type == MSG_CHAT_MESSAGE) {
        msg_chat_message_t* chat = (msg_chat_message_t*)payload;
        ui_display_chat_message(chat);
    }
}

/* Start notification listener thread */
pthread_t start_notification_listener(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, notification_listener, NULL);
    return thread;
}
