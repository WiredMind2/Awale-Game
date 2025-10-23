/* Client State Management - Implementation
 * Manages pending challenges, active games, and spectator state
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_state.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Global state */
static session_t* g_session_ptr = NULL;
static char g_pseudo[MAX_PSEUDO_LEN] = {0};
static volatile bool g_running = true;

/* Pending challenges data structure */
static struct {
    pending_challenge_t challenges[MAX_PENDING_CHALLENGES];
    int count;
    pthread_mutex_t lock;
} g_pending_challenges;

/* Active games data structure */
static struct {
    active_game_t games[MAX_ACTIVE_GAMES];
    int count;
    pthread_mutex_t lock;
    bool turn_notification;
    pthread_cond_t turn_cond;
} g_active_games;

/* Spectator state data structure */
static struct {
    char game_id[MAX_GAME_ID_LEN];
    char player_a[MAX_PSEUDO_LEN];
    char player_b[MAX_PSEUDO_LEN];
    bool active;
    bool board_updated;
    pthread_mutex_t lock;
    pthread_cond_t update_cond;
} g_spectator_state;

/* Global state access */
session_t* client_state_get_session(void) {
    return g_session_ptr;
}

const char* client_state_get_pseudo(void) {
    return g_pseudo;
}

void client_state_set_pseudo(const char* pseudo) {
    strncpy(g_pseudo, pseudo, MAX_PSEUDO_LEN - 1);
    g_pseudo[MAX_PSEUDO_LEN - 1] = '\0';
}

bool client_state_is_running(void) {
    return g_running;
}

void client_state_set_running(bool running) {
    g_running = running;
}

/* Pending challenges implementation */
void pending_challenges_init(void) {
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

int pending_challenges_count(void) {
    pthread_mutex_lock(&g_pending_challenges.lock);
    int count = g_pending_challenges.count;
    pthread_mutex_unlock(&g_pending_challenges.lock);
    return count;
}

pending_challenge_t* pending_challenges_get(int index) {
    static pending_challenge_t result;
    pthread_mutex_lock(&g_pending_challenges.lock);
    
    int found = 0;
    for (int i = 0; i < MAX_PENDING_CHALLENGES; i++) {
        if (g_pending_challenges.challenges[i].active) {
            if (found == index) {
                result = g_pending_challenges.challenges[i];
                pthread_mutex_unlock(&g_pending_challenges.lock);
                return &result;
            }
            found++;
        }
    }
    pthread_mutex_unlock(&g_pending_challenges.lock);
    return NULL;
}

/* Active games implementation */
void active_games_init(void) {
    pthread_mutex_init(&g_active_games.lock, NULL);
    pthread_cond_init(&g_active_games.turn_cond, NULL);
    g_active_games.count = 0;
    g_active_games.turn_notification = false;
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        g_active_games.games[i].active = false;
    }
}

void active_games_add(const char* game_id, const char* player_a, const char* player_b, player_id_t my_side) {
    pthread_mutex_lock(&g_active_games.lock);
    
    /* Check if game already exists */
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        if (g_active_games.games[i].active && 
            strcmp(g_active_games.games[i].game_id, game_id) == 0) {
            pthread_mutex_unlock(&g_active_games.lock);
            return;
        }
    }
    
    /* Add new game */
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        if (!g_active_games.games[i].active) {
            snprintf(g_active_games.games[i].game_id, MAX_GAME_ID_LEN, "%s", game_id);
            snprintf(g_active_games.games[i].player_a, MAX_PSEUDO_LEN, "%s", player_a);
            snprintf(g_active_games.games[i].player_b, MAX_PSEUDO_LEN, "%s", player_b);
            g_active_games.games[i].my_side = my_side;
            g_active_games.games[i].active = true;
            g_active_games.count++;
            break;
        }
    }
    pthread_mutex_unlock(&g_active_games.lock);
}

void active_games_remove(const char* game_id) {
    pthread_mutex_lock(&g_active_games.lock);
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        if (g_active_games.games[i].active &&
            strcmp(g_active_games.games[i].game_id, game_id) == 0) {
            g_active_games.games[i].active = false;
            g_active_games.count--;
            break;
        }
    }
    pthread_mutex_unlock(&g_active_games.lock);
}

int active_games_count(void) {
    pthread_mutex_lock(&g_active_games.lock);
    int count = g_active_games.count;
    pthread_mutex_unlock(&g_active_games.lock);
    return count;
}

active_game_t* active_games_get(int index) {
    static active_game_t result;
    pthread_mutex_lock(&g_active_games.lock);
    
    int found = 0;
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        if (g_active_games.games[i].active) {
            if (found == index) {
                result = g_active_games.games[i];
                pthread_mutex_unlock(&g_active_games.lock);
                return &result;
            }
            found++;
        }
    }
    pthread_mutex_unlock(&g_active_games.lock);
    return NULL;
}

void active_games_notify_turn(void) {
    pthread_mutex_lock(&g_active_games.lock);
    g_active_games.turn_notification = true;
    pthread_cond_broadcast(&g_active_games.turn_cond);
    pthread_mutex_unlock(&g_active_games.lock);
}

bool active_games_wait_for_turn(int timeout_sec) {
    pthread_mutex_lock(&g_active_games.lock);
    
    if (g_active_games.turn_notification) {
        g_active_games.turn_notification = false;
        pthread_mutex_unlock(&g_active_games.lock);
        return true;
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_sec;
    
    int result = pthread_cond_timedwait(&g_active_games.turn_cond, &g_active_games.lock, &ts);
    
    bool notified = g_active_games.turn_notification;
    g_active_games.turn_notification = false;
    
    pthread_mutex_unlock(&g_active_games.lock);
    return (result == 0 && notified);
}

/* Spectator state implementation */
void spectator_state_init(void) {
    pthread_mutex_init(&g_spectator_state.lock, NULL);
    pthread_cond_init(&g_spectator_state.update_cond, NULL);
    g_spectator_state.active = false;
    g_spectator_state.board_updated = false;
    g_spectator_state.game_id[0] = '\0';
    g_spectator_state.player_a[0] = '\0';
    g_spectator_state.player_b[0] = '\0';
}

void spectator_state_set(const char* game_id, const char* player_a, const char* player_b) {
    pthread_mutex_lock(&g_spectator_state.lock);
    snprintf(g_spectator_state.game_id, sizeof(g_spectator_state.game_id), "%s", game_id);
    snprintf(g_spectator_state.player_a, sizeof(g_spectator_state.player_a), "%s", player_a);
    snprintf(g_spectator_state.player_b, sizeof(g_spectator_state.player_b), "%s", player_b);
    g_spectator_state.active = true;
    g_spectator_state.board_updated = false;
    pthread_mutex_unlock(&g_spectator_state.lock);
}

void spectator_state_clear(void) {
    pthread_mutex_lock(&g_spectator_state.lock);
    g_spectator_state.active = false;
    g_spectator_state.board_updated = false;
    g_spectator_state.game_id[0] = '\0';
    g_spectator_state.player_a[0] = '\0';
    g_spectator_state.player_b[0] = '\0';
    pthread_mutex_unlock(&g_spectator_state.lock);
}

bool spectator_state_is_active(void) {
    pthread_mutex_lock(&g_spectator_state.lock);
    bool active = g_spectator_state.active;
    pthread_mutex_unlock(&g_spectator_state.lock);
    return active;
}

const char* spectator_state_get_game_id(void) {
    return g_spectator_state.game_id;
}

void spectator_state_notify_update(void) {
    pthread_mutex_lock(&g_spectator_state.lock);
    g_spectator_state.board_updated = true;
    pthread_cond_signal(&g_spectator_state.update_cond);
    pthread_mutex_unlock(&g_spectator_state.lock);
}

bool spectator_state_wait_for_update(int timeout_sec) {
    pthread_mutex_lock(&g_spectator_state.lock);
    
    if (g_spectator_state.board_updated) {
        g_spectator_state.board_updated = false;
        pthread_mutex_unlock(&g_spectator_state.lock);
        return true;
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_sec;
    
    int result = pthread_cond_timedwait(&g_spectator_state.update_cond, &g_spectator_state.lock, &ts);
    
    bool updated = g_spectator_state.board_updated;
    g_spectator_state.board_updated = false;
    
    pthread_mutex_unlock(&g_spectator_state.lock);
    return (result == 0 && updated);
}

bool spectator_state_check_and_clear_updated(void) {
    pthread_mutex_lock(&g_spectator_state.lock);
    bool updated = g_spectator_state.board_updated;
    if (updated) {
        g_spectator_state.board_updated = false;
    }
    pthread_mutex_unlock(&g_spectator_state.lock);
    return updated;
}

/* Initialize all client state */
void client_state_init(session_t* session) {
    g_session_ptr = session;
    g_running = true;
    pending_challenges_init();
    active_games_init();
    spectator_state_init();
}
