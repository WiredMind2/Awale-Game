/* Session Registry Implementation
 * Thread-safe registry for managing active client sessions
 */

#include "../../include/server/server_registry.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>

#define MAX_SESSIONS 100

/* Session entry structure */
typedef struct {
    session_t* session;
    bool active;
    pthread_mutex_t lock;
} session_entry_t;

/* Global session registry */
static struct {
    session_entry_t sessions[MAX_SESSIONS];
    pthread_mutex_t lock;
} g_session_registry;

void session_registry_init(void) {
    pthread_mutex_init(&g_session_registry.lock, NULL);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        g_session_registry.sessions[i].session = NULL;
        g_session_registry.sessions[i].active = false;
        pthread_mutex_init(&g_session_registry.sessions[i].lock, NULL);
    }
}

bool session_registry_add(session_t* session) {
    pthread_mutex_lock(&g_session_registry.lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!g_session_registry.sessions[i].active) {
            g_session_registry.sessions[i].session = session;
            g_session_registry.sessions[i].active = true;
            pthread_mutex_unlock(&g_session_registry.lock);
            return true;
        }
    }
    pthread_mutex_unlock(&g_session_registry.lock);
    return false;
}

void session_registry_remove(session_t* session) {
    pthread_mutex_lock(&g_session_registry.lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (g_session_registry.sessions[i].active && 
            g_session_registry.sessions[i].session == session) {
            g_session_registry.sessions[i].active = false;
            g_session_registry.sessions[i].session = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&g_session_registry.lock);
}

session_t* session_registry_find(const char* pseudo) {
    session_t* result = NULL;
    pthread_mutex_lock(&g_session_registry.lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (g_session_registry.sessions[i].active && 
            strcmp(g_session_registry.sessions[i].session->pseudo, pseudo) == 0) {
            result = g_session_registry.sessions[i].session;
            break;
        }
    }
    pthread_mutex_unlock(&g_session_registry.lock);
    return result;
}
