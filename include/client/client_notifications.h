/* Client Notifications - Background Notification Listener
 * Handles incoming server notifications in separate thread
 */

#ifndef CLIENT_NOTIFICATIONS_H
#define CLIENT_NOTIFICATIONS_H

#include <pthread.h>
#include "../../include/common/types.h"
#include "../../include/common/protocol.h"

/* Notification listener thread */
void* notification_listener(void* arg);

/* Start notification listener thread */
pthread_t start_notification_listener(void);

/* Send challenge accept message */
error_code_t send_challenge_accept(int64_t challenge_id);

/* Send challenge decline message */
error_code_t send_challenge_decline(int64_t challenge_id);

/* Handle a notification message */
void handle_notification_message(message_type_t type, const void* payload, size_t size);

#endif /* CLIENT_NOTIFICATIONS_H */
