#include "../../include/game/player.h"
#include <string.h>
#include <time.h>
#include <ctype.h>

error_code_t player_init(player_t* player, const char* pseudo, const char* ip) {
    if (!player || !pseudo || !ip) return ERR_INVALID_PARAM;
    
    if (!player_is_valid_pseudo(pseudo)) return ERR_INVALID_PARAM;
    
    strncpy(player->info.pseudo, pseudo, MAX_PSEUDO_LEN - 1);
    player->info.pseudo[MAX_PSEUDO_LEN - 1] = '\0';
    
    strncpy(player->info.ip, ip, MAX_IP_LEN - 1);
    player->info.ip[MAX_IP_LEN - 1] = '\0';
    
    player->connected = true;
    player->connect_time = time(NULL);
    player->last_activity = time(NULL);
    player->games_played = 0;
    player->games_won = 0;
    
    return SUCCESS;
}

error_code_t player_copy(const player_t* src, player_t* dest) {
    if (!src || !dest) return ERR_INVALID_PARAM;
    memcpy(dest, src, sizeof(player_t));
    return SUCCESS;
}

bool player_equals(const player_t* p1, const player_t* p2) {
    if (!p1 || !p2) return false;
    return strcmp(p1->info.pseudo, p2->info.pseudo) == 0;
}

bool player_is_valid_pseudo(const char* pseudo) {
    if (!pseudo) return false;
    
    size_t len = strlen(pseudo);
    if (len == 0 || len >= MAX_PSEUDO_LEN) return false;
    
    // Check for valid characters (alphanumeric, underscore, hyphen)
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(pseudo[i]) && pseudo[i] != '_' && pseudo[i] != '-') {
            return false;
        }
    }
    
    return true;
}

bool player_is_connected(const player_t* player) {
    if (!player) return false;
    return player->connected;
}

void player_update_stats(player_t* player, bool won) {
    if (!player) return;
    
    player->games_played++;
    if (won) {
        player->games_won++;
    }
    player->last_activity = time(NULL);
}

void player_touch_activity(player_t* player) {
    if (!player) return;
    player->last_activity = time(NULL);
}
