/* Client State Management
 * Manages pending challenges, active games, and spectator state
 */

#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

#include "../common/types.h"
#include "../network/session.h"
#include <pthread.h>
#include <stdbool.h>

/* Global state access */
extern session_t* client_state_get_session(void);
extern const char* client_state_get_pseudo(void);
extern void client_state_set_pseudo(const char* pseudo);
extern bool client_state_is_running(void);
extern void client_state_set_running(bool running);

/* Pending challenges tracking */
#define MAX_PENDING_CHALLENGES 10

typedef struct {
    char challenger[MAX_PSEUDO_LEN];
    int64_t challenge_id;
    bool active;
} pending_challenge_t;

void pending_challenges_init(void);
void pending_challenges_add(const char* challenger, int64_t challenge_id);
void pending_challenges_remove(const char* challenger);
int pending_challenges_count(void);
pending_challenge_t* pending_challenges_get(int index);

/* Active games tracking */
#define MAX_ACTIVE_GAMES 10

typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char player_a[MAX_PSEUDO_LEN];
    char player_b[MAX_PSEUDO_LEN];
    player_id_t my_side;
    bool active;
} active_game_t;

void active_games_init(void);
void active_games_add(const char* game_id, const char* player_a, const char* player_b, player_id_t my_side);
void active_games_remove(const char* game_id);
int active_games_count(void);
active_game_t* active_games_get(int index);
void active_games_notify_turn(void);
bool active_games_wait_for_turn(int timeout_sec);
int active_games_get_notification_fd(void);  /* Get fd for select() */
void active_games_clear_notifications(void);  /* Clear all pending notifications */

/* Spectator state tracking */
void spectator_state_init(void);
void spectator_state_set(const char* game_id, const char* player_a, const char* player_b);
void spectator_state_clear(void);
bool spectator_state_is_active(void);
const char* spectator_state_get_game_id(void);
void spectator_state_notify_update(void);
bool spectator_state_wait_for_update(int timeout_sec);
bool spectator_state_check_and_clear_updated(void);

/* Initialize all client state */
void client_state_init(session_t* session);

#endif /* CLIENT_STATE_H */
