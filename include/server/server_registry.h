/* Session Registry - Thread-safe registry for active client sessions
 * Used for push notifications and session lookup by pseudo
 */

#ifndef SERVER_REGISTRY_H
#define SERVER_REGISTRY_H

#include "../network/session.h"
#include <stdbool.h>

/* Initialize the session registry */
void session_registry_init(void);

/* Add a session to the registry
 * Returns true if successful, false if registry is full
 */
bool session_registry_add(session_t* session);

/* Remove a session from the registry */
void session_registry_remove(session_t* session);

/* Find a session by pseudo (player name)
 * Returns the session pointer if found, NULL otherwise
 */
session_t* session_registry_find(const char* pseudo);

#endif /* SERVER_REGISTRY_H */
