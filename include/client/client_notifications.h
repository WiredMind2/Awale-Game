/* Client Notifications - Background Notification Listener
 * Handles incoming server notifications in separate thread
 */

#ifndef CLIENT_NOTIFICATIONS_H
#define CLIENT_NOTIFICATIONS_H

#include <pthread.h>

/* Notification listener thread */
void* notification_listener(void* arg);

/* Start notification listener thread */
pthread_t start_notification_listener(void);

#endif /* CLIENT_NOTIFICATIONS_H */
