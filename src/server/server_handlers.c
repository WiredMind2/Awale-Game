/* Server Message Handlers Implementation
 * Handles all incoming client messages and game operations
 */

#include "../../include/server/server_handlers.h"
#include "../../include/server/server_registry.h"
#include "../../include/server/matchmaking.h"
#include "../../include/game/board.h"
#include "../../include/common/protocol.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Global managers (set via handlers_init) */
static game_manager_t* g_game_manager = NULL;
static matchmaking_t* g_matchmaking = NULL;

void handlers_init(game_manager_t* game_mgr, matchmaking_t* matchmaking) {
    g_game_manager = game_mgr;
    g_matchmaking = matchmaking;
}

/* Handle MSG_LIST_PLAYERS */
void handle_list_players(session_t* session) {
    msg_player_list_t list;
    int count;
    
    error_code_t err = matchmaking_get_players(g_matchmaking, list.players, 100, &count);
    if (err == SUCCESS) {
        list.count = count;
        
        /* Calculate actual size: count field + only the actual number of players */
        size_t actual_size = sizeof(list.count) + (count * sizeof(player_info_t));
        
        session_send_message(session, MSG_PLAYER_LIST, &list, actual_size);
    } else {
        session_send_error(session, err, "Failed to get player list");
    }
}

/* Handle MSG_CHALLENGE - New notification-based approach */
void handle_challenge(session_t* session, const char* opponent) {
    /* First check if opponent exists and is online */
    session_t* opponent_session = session_registry_find(opponent);
    if (!opponent_session) {
        session_send_error(session, ERR_PLAYER_NOT_FOUND, "Player not found or offline");
        return;
    }

    /* Record the challenge with ID */
    int64_t challenge_id;
    error_code_t err = matchmaking_create_challenge_with_id(g_matchmaking, session->pseudo, opponent, &challenge_id);

    if (err != SUCCESS) {
        session_send_error(session, err, "Failed to create challenge");
        return;
    }

    /* Send confirmation to challenger */
    printf("Challenge sent: %s -> %s (ID: %lld)\n", session->pseudo, opponent, (long long)challenge_id);
    session_send_message(session, MSG_CHALLENGE_SENT, NULL, 0);

    /* Send push notification to opponent */
    msg_challenge_received_t notification;
    memset(&notification, 0, sizeof(notification));
    snprintf(notification.from, MAX_PSEUDO_LEN, "%s", session->pseudo);
    snprintf(notification.message, 256, "%s challenges you to a game!", session->pseudo);
    notification.challenge_id = challenge_id;

    session_send_message(opponent_session, MSG_CHALLENGE_RECEIVED, &notification, sizeof(notification));
    printf("Notification sent to %s\n", opponent);
}

/* Handle MSG_ACCEPT_CHALLENGE */
void handle_accept_challenge(session_t* session, const char* challenger) {
    /* Find the challenger's session */
    session_t* challenger_session = session_registry_find(challenger);
    if (!challenger_session) {
        session_send_error(session, ERR_PLAYER_NOT_FOUND, "Challenger not found or offline");
        return;
    }
    
    /* Create the game */
    char game_id[MAX_GAME_ID_LEN];
    error_code_t err = game_manager_create_game(g_game_manager, challenger, session->pseudo, game_id);
    
    if (err != SUCCESS) {
        session_send_error(session, err, "Failed to create game");
        return;
    }
    
    printf("Game started: %s vs %s (ID: %s)\n", challenger, session->pseudo, game_id);
    
    /* Remove the challenge from matchmaking */
    matchmaking_remove_challenge(g_matchmaking, challenger, session->pseudo);
    
    /* Send MSG_GAME_STARTED to both players */
    msg_game_started_t start_msg;
    memset(&start_msg, 0, sizeof(start_msg));
    snprintf(start_msg.game_id, MAX_GAME_ID_LEN, "%s", game_id);
    snprintf(start_msg.player_a, MAX_PSEUDO_LEN, "%s", challenger);
    snprintf(start_msg.player_b, MAX_PSEUDO_LEN, "%s", session->pseudo);
    
    /* Send to accepter (Player B) */
    start_msg.your_side = PLAYER_B;
    session_send_message(session, MSG_GAME_STARTED, &start_msg, sizeof(start_msg));
    
    /* Send to challenger (Player A) */
    start_msg.your_side = PLAYER_A;
    session_send_message(challenger_session, MSG_GAME_STARTED, &start_msg, sizeof(start_msg));
}

/* Handle MSG_DECLINE_CHALLENGE */
void handle_decline_challenge(session_t* session, const char* challenger) {
    /* Remove the challenge */
    matchmaking_remove_challenge(g_matchmaking, challenger, session->pseudo);
    
    printf("Challenge declined: %s -> %s\n", challenger, session->pseudo);
    
    /* Optionally notify the challenger */
    session_t* challenger_session = session_registry_find(challenger);
    if (challenger_session) {
        msg_error_t decline_msg;
        decline_msg.error_code = SUCCESS;
        snprintf(decline_msg.error_msg, 256, "%s declined your challenge", session->pseudo);
        session_send_message(challenger_session, MSG_ERROR, &decline_msg, sizeof(decline_msg));
    }
    
    /* Confirm to decliner */
    session_send_message(session, MSG_CHALLENGE_SENT, NULL, 0);  /* Reuse as ACK */
}

/* Handle MSG_GET_CHALLENGES */
void handle_get_challenges(session_t* session) {
    msg_challenge_list_t list;
    char challengers[100][MAX_PSEUDO_LEN];
    int count = 0;
    
    pthread_mutex_lock(&g_matchmaking->lock);
    
    for (int i = 0; i < g_matchmaking->challenge_count && count < 100; i++) {
        if (g_matchmaking->challenges[i].active &&
            strcmp(g_matchmaking->challenges[i].opponent, session->pseudo) == 0) {
            strncpy(challengers[count], g_matchmaking->challenges[i].challenger, MAX_PSEUDO_LEN - 1);
            challengers[count][MAX_PSEUDO_LEN - 1] = '\0';
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_matchmaking->lock);
    
    list.count = count;
    for (int i = 0; i < count; i++) {
        memcpy(list.challengers[i], challengers[i], MAX_PSEUDO_LEN);
    }
    
    /* Calculate actual size: count field + actual number of challengers */
    size_t actual_size = sizeof(list.count) + (count * MAX_PSEUDO_LEN);
    session_send_message(session, MSG_CHALLENGE_LIST, &list, actual_size);
}

/* Handle MSG_PLAY_MOVE */
void handle_play_move(session_t* session, const msg_play_move_t* move) {
    int seeds_captured = 0;
    
    error_code_t err = game_manager_play_move(g_game_manager, move->game_id, 
                                              session->pseudo, move->pit_index, &seeds_captured);
    
    msg_move_result_t result;
    result.success = (err == SUCCESS);
    result.seeds_captured = seeds_captured;
    
    if (err == SUCCESS) {
        snprintf(result.message, 255, "Move executed: pit %d, captured %d seeds", 
                 move->pit_index, seeds_captured);
        
        /* Get game instance for statistics updates */
        game_instance_t* game = game_manager_find_game(g_game_manager, move->game_id);
        
        /* Check if game is over */
        board_t board;
        if (game_manager_get_board(g_game_manager, move->game_id, &board) == SUCCESS) {
            result.game_over = board_is_game_over(&board);
            result.winner = board_get_winner(&board);
            
            /* If game is over, remove it from active games */
            if (result.game_over && game) {
                // Update player statistics
                if (result.winner == (winner_t)PLAYER_A) {
                    matchmaking_update_player_stats(g_matchmaking, game->player_a, true, board.scores[0]);
                    matchmaking_update_player_stats(g_matchmaking, game->player_b, false, board.scores[1]);
                } else if (result.winner == (winner_t)PLAYER_B) {
                    matchmaking_update_player_stats(g_matchmaking, game->player_a, false, board.scores[0]);
                    matchmaking_update_player_stats(g_matchmaking, game->player_b, true, board.scores[1]);
                } else {
                    // Draw - both get their scores but no win/loss
                    matchmaking_update_player_stats(g_matchmaking, game->player_a, false, board.scores[0]);
                    matchmaking_update_player_stats(g_matchmaking, game->player_b, false, board.scores[1]);
                }
                
                game_manager_remove_game(g_game_manager, move->game_id);
                printf("Game ended: %s vs %s - Winner: %s\n", 
                       game->player_a, game->player_b, 
                       result.winner == (winner_t)PLAYER_A ? game->player_a : 
                       result.winner == (winner_t)PLAYER_B ? game->player_b : "Draw");
            }
        } else {
            result.game_over = false;
            result.winner = NO_WINNER;
        }
        
    printf("Move: %s played pit %d in %s (captured: %d)\n", 
               session->pseudo, move->pit_index, move->game_id, seeds_captured);
    } else {
        strncpy(result.message, error_to_string(err), 255);
        result.game_over = false;
        result.winner = NO_WINNER;
    }
    
    result.message[255] = '\0';
    
    /* Send result to the player who made the move */
    session_send_move_result(session, &result);
    
    /* If move was successful, also notify the opponent */
    if (err == SUCCESS) {
        game_instance_t* game = game_manager_find_game(g_game_manager, move->game_id);
        if (game) {
            /* Determine opponent's pseudo */
            const char* opponent = (strcmp(game->player_a, session->pseudo) == 0) 
                                   ? game->player_b 
                                   : game->player_a;
            
            /* Find opponent's session and send notification */
            session_t* opponent_session = session_registry_find(opponent);
            if (opponent_session) {
                session_send_move_result(opponent_session, &result);
            }
        }
    }
}

/* Handle MSG_GET_BOARD */
void handle_get_board(session_t* session, const msg_get_board_t* req) {
    msg_board_state_t board_msg;
    memset(&board_msg, 0, sizeof(board_msg));
    
    /* Find game by players */
    game_instance_t* game = game_manager_find_game_by_players(g_game_manager, 
                                                               req->player_a, req->player_b);
    
    if (game) {
        board_msg.exists = true;
        strncpy(board_msg.game_id, game->game_id, MAX_GAME_ID_LEN - 1);
        board_msg.game_id[MAX_GAME_ID_LEN - 1] = '\0';
        strncpy(board_msg.player_a, game->player_a, MAX_PSEUDO_LEN - 1);
        board_msg.player_a[MAX_PSEUDO_LEN - 1] = '\0';
        strncpy(board_msg.player_b, game->player_b, MAX_PSEUDO_LEN - 1);
        board_msg.player_b[MAX_PSEUDO_LEN - 1] = '\0';
        
        pthread_mutex_lock(&game->lock);
        
        for (int i = 0; i < NUM_PITS; i++) {
            board_msg.pits[i] = game->board.pits[i];
        }
        board_msg.score_a = game->board.scores[0];
        board_msg.score_b = game->board.scores[1];
        board_msg.current_player = game->board.current_player;
        board_msg.state = game->board.state;
        board_msg.winner = game->board.winner;
        
        pthread_mutex_unlock(&game->lock);
    } else {
        board_msg.exists = false;
    }
    
    session_send_board_state(session, &board_msg);
}

/* Handle MSG_LIST_GAMES */
void handle_list_games(session_t* session) {
    msg_game_list_t list;
    memset(&list, 0, sizeof(list));

    list.count = game_manager_get_active_games(g_game_manager, list.games, 50);

    /* Calculate actual size */
    size_t actual_size = sizeof(list.count) + (list.count * sizeof(game_info_t));
    session_send_message(session, MSG_GAME_LIST, &list, actual_size);
}

/* Handle MSG_LIST_MY_GAMES */
void handle_list_my_games(session_t* session) {
    msg_my_game_list_t list;
    memset(&list, 0, sizeof(list));

    list.count = game_manager_get_player_games(g_game_manager, session->pseudo, list.games, 50);

    /* Calculate actual size */
    size_t actual_size = sizeof(list.count) + (list.count * sizeof(game_info_t));
    session_send_message(session, MSG_MY_GAME_LIST, &list, actual_size);
}

/* Handle MSG_SPECTATE_GAME */
void handle_spectate_game(session_t* session, const char* game_id) {
    game_instance_t* game = game_manager_find_game(g_game_manager, game_id);
    
    if (!game) {
        session_send_error(session, ERR_GAME_NOT_FOUND, "Game not found");
        return;
    }
    
    /* Add spectator */
    error_code_t err = game_manager_add_spectator(g_game_manager, game_id, session->pseudo);
    
    if (err != SUCCESS) {
        session_send_error(session, err, "Failed to join as spectator");
        return;
    }
    
    /* Send acknowledgment */
    msg_spectate_ack_t ack;
    ack.success = true;
    snprintf(ack.message, 256, "You are now spectating %s vs %s", game->player_a, game->player_b);
    ack.spectator_count = game_manager_get_spectator_count(g_game_manager, game_id);
    
    session_send_message(session, MSG_SPECTATE_ACK, &ack, sizeof(ack));
    
    printf("%s is now spectating %s\n", session->pseudo, game_id);
    
    /* Notify players and other spectators */
    msg_spectator_joined_t notification;
    memset(&notification, 0, sizeof(notification));
    snprintf(notification.spectator, MAX_PSEUDO_LEN, "%s", session->pseudo);
    snprintf(notification.game_id, MAX_GAME_ID_LEN, "%s", game_id);
    notification.spectator_count = ack.spectator_count;
    
    /* Notify player A */
    session_t* player_a_session = session_registry_find(game->player_a);
    if (player_a_session) {
        session_send_message(player_a_session, MSG_SPECTATOR_JOINED, &notification, sizeof(notification));
    }
    
    /* Notify player B */
    session_t* player_b_session = session_registry_find(game->player_b);
    if (player_b_session) {
        session_send_message(player_b_session, MSG_SPECTATOR_JOINED, &notification, sizeof(notification));
    }
    
    /* Notify other spectators */
    pthread_mutex_lock(&game->lock);
    for (int i = 0; i < game->spectator_count; i++) {
        if (strcmp(game->spectators[i], session->pseudo) != 0) {  /* Don't notify self */
            session_t* spectator_session = session_registry_find(game->spectators[i]);
            if (spectator_session) {
                session_send_message(spectator_session, MSG_SPECTATOR_JOINED, &notification, sizeof(notification));
            }
        }
    }
    pthread_mutex_unlock(&game->lock);
}

/* Handle MSG_STOP_SPECTATE */
void handle_stop_spectate(session_t* session, const char* game_id) {
    error_code_t err = game_manager_remove_spectator(g_game_manager, game_id, session->pseudo);
    
    if (err == SUCCESS) {
    printf("%s stopped spectating %s\n", session->pseudo, game_id);
        session_send_message(session, MSG_CHALLENGE_SENT, NULL, 0);  /* Reuse as ACK */
    } else {
        session_send_error(session, err, "Failed to stop spectating");
    }
}

/* Handle MSG_SET_BIO */
void handle_set_bio(session_t* session, const msg_set_bio_t* bio_msg) {
    if (!bio_msg) {
        session_send_error(session, ERR_INVALID_PARAM, "Invalid bio data");
        return;
    }

    /* Find the player in matchmaking */
    pthread_mutex_lock(&g_matchmaking->lock);
    
    bool found = false;
    for (int i = 0; i < g_matchmaking->player_count; i++) {
        if (strcmp(g_matchmaking->players[i].info.pseudo, session->pseudo) == 0) {
            /* Update bio */
            g_matchmaking->players[i].info.bio_lines = bio_msg->bio_lines;
            for (int j = 0; j < bio_msg->bio_lines && j < 10; j++) {
                strncpy(g_matchmaking->players[i].info.bio[j], bio_msg->bio[j], 255);
                g_matchmaking->players[i].info.bio[j][255] = '\0';
            }
            found = true;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_matchmaking->lock);

    if (found) {
    printf("%s updated their bio (%d lines)\n", session->pseudo, bio_msg->bio_lines);
        session_send_message(session, MSG_CHALLENGE_SENT, NULL, 0);  /* Reuse as ACK */
    } else {
        session_send_error(session, ERR_PLAYER_NOT_FOUND, "Player not found");
    }
}

/* Handle MSG_GET_BIO */
void handle_get_bio(session_t* session, const msg_get_bio_t* bio_req) {
    if (!bio_req) {
        session_send_error(session, ERR_INVALID_PARAM, "Invalid bio request");
        return;
    }

    msg_bio_response_t response;
    memset(&response, 0, sizeof(response));
    snprintf(response.player, MAX_PSEUDO_LEN, "%s", bio_req->target_player);
    response.player[MAX_PSEUDO_LEN - 1] = '\0';

    /* Find the player */
    pthread_mutex_lock(&g_matchmaking->lock);
    
    bool found = false;
    for (int i = 0; i < g_matchmaking->player_count; i++) {
        if (strcmp(g_matchmaking->players[i].info.pseudo, bio_req->target_player) == 0) {
            response.success = true;
            response.bio_lines = g_matchmaking->players[i].info.bio_lines;
            for (int j = 0; j < response.bio_lines && j < 10; j++) {
                strncpy(response.bio[j], g_matchmaking->players[i].info.bio[j], 255);
            }
            found = true;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_matchmaking->lock);

    if (!found) {
        response.success = false;
        snprintf(response.message, sizeof(response.message), "Player '%s' not found", bio_req->target_player);
    }

    session_send_message(session, MSG_BIO_RESPONSE, &response, sizeof(response));
}

/* Handle MSG_GET_PLAYER_STATS */
void handle_get_player_stats(session_t* session, const msg_get_player_stats_t* stats_req) {
    if (!stats_req) {
        session_send_error(session, ERR_INVALID_PARAM, "Invalid stats request");
        return;
    }

    msg_player_stats_t response;
    memset(&response, 0, sizeof(response));
    snprintf(response.player, MAX_PSEUDO_LEN, "%s", stats_req->target_player);
    response.player[MAX_PSEUDO_LEN - 1] = '\0';

    /* Get player stats using matchmaking function */
    player_info_t player_info;
    error_code_t err = matchmaking_get_player_stats(g_matchmaking, stats_req->target_player, &player_info);

    if (err == SUCCESS) {
        response.success = true;
        response.games_played = player_info.games_played;
        response.games_won = player_info.games_won;
        response.games_lost = player_info.games_lost;
        response.total_score = player_info.total_score;
        response.first_seen = player_info.first_seen;
        response.last_seen = player_info.last_seen;
    } else {
        response.success = false;
        snprintf(response.message, sizeof(response.message), "Player '%s' not found", stats_req->target_player);
    }

    session_send_message(session, MSG_PLAYER_STATS, &response, sizeof(response));
}

/* Handle MSG_SEND_CHAT */
void handle_send_chat(session_t* session, const msg_send_chat_t* chat_msg) {
    if (!chat_msg) {
        session_send_error(session, ERR_INVALID_PARAM, "Invalid chat message");
        return;
    }

    /* Validate message length */
    if (strlen(chat_msg->message) == 0 || strlen(chat_msg->message) >= MAX_CHAT_LEN) {
        session_send_error(session, ERR_INVALID_PARAM, "Message length must be between 1 and 511 characters");
        return;
    }

    /* Check if recipient is specified (private chat) or empty (global chat) */
    bool is_private = (strlen(chat_msg->recipient) > 0);

    /* Prepare a single chat notification to reuse for sender/recipients */
    msg_chat_message_t chat_notification;
    memset(&chat_notification, 0, sizeof(chat_notification));

    if (is_private) {
        /* Private chat: validate recipient exists */
        session_t* recipient_session = session_registry_find(chat_msg->recipient);
        if (!recipient_session) {
            session_send_error(session, ERR_PLAYER_NOT_FOUND, "Recipient not found or offline");
            return;
        }

    /* Send message to recipient */
    snprintf(chat_notification.sender, MAX_PSEUDO_LEN, "%s", session->pseudo);
    snprintf(chat_notification.recipient, MAX_PSEUDO_LEN, "%s", chat_msg->recipient);
    snprintf(chat_notification.message, MAX_CHAT_LEN, "%s", chat_msg->message);
    chat_notification.timestamp = time(NULL);

    session_send_message(recipient_session, MSG_CHAT_MESSAGE, &chat_notification, sizeof(chat_notification));

    printf("Private chat: %s -> %s\n", session->pseudo, chat_msg->recipient);
    } else {
    /* Global chat: send to all online players */
    snprintf(chat_notification.sender, MAX_PSEUDO_LEN, "%s", session->pseudo);
    /* recipient remains empty for global chat */
    snprintf(chat_notification.message, MAX_CHAT_LEN, "%s", chat_msg->message);
    chat_notification.timestamp = time(NULL);

        /* Send to all online players except sender */
        pthread_mutex_lock(&g_matchmaking->lock);
        for (int i = 0; i < g_matchmaking->player_count; i++) {
            if (strcmp(g_matchmaking->players[i].info.pseudo, session->pseudo) != 0) {
                session_t* player_session = session_registry_find(g_matchmaking->players[i].info.pseudo);
                if (player_session) {
                    session_send_message(player_session, MSG_CHAT_MESSAGE, &chat_notification, sizeof(chat_notification));
                }
            }
        }
        pthread_mutex_unlock(&g_matchmaking->lock);

    printf("Global chat: %s\n", session->pseudo);
    }

    /* Send the chat notification back to the sender so their client displays the message
       via the same MSG_CHAT_MESSAGE handler. */
    session_send_message(session, MSG_CHAT_MESSAGE, &chat_notification, sizeof(chat_notification));
}

/* Handle MSG_CHALLENGE_ACCEPT */
void handle_challenge_accept(session_t* session, const msg_challenge_accept_t* accept_msg) {
    if (!accept_msg) {
        session_send_error(session, ERR_INVALID_PARAM, "Invalid accept message");
        return;
    }

    /* Find the challenge */
    challenge_t* challenge = NULL;
    error_code_t err = matchmaking_find_challenge_by_id(g_matchmaking, accept_msg->challenge_id, &challenge);

    if (err != SUCCESS || !challenge) {
        session_send_error(session, ERR_GAME_NOT_FOUND, "Challenge not found or expired");
        return;
    }

    /* Verify that the accepter is the opponent */
    if (strcmp(session->pseudo, challenge->opponent) != 0) {
        session_send_error(session, ERR_INVALID_PARAM, "You are not the recipient of this challenge");
        return;
    }

    /* Find the challenger's session */
    session_t* challenger_session = session_registry_find(challenge->challenger);
    if (!challenger_session) {
        session_send_error(session, ERR_PLAYER_NOT_FOUND, "Challenger not found or offline");
        matchmaking_remove_challenge_by_id(g_matchmaking, accept_msg->challenge_id);
        return;
    }

    /* Create the game */
    char game_id[MAX_GAME_ID_LEN];
    err = game_manager_create_game(g_game_manager, challenge->challenger, session->pseudo, game_id);

    if (err != SUCCESS) {
        session_send_error(session, err, "Failed to create game");
        return;
    }

    printf("Challenge accepted: %s vs %s (ID: %s)\n", challenge->challenger, session->pseudo, game_id);

    /* Remove the challenge */
    matchmaking_remove_challenge_by_id(g_matchmaking, accept_msg->challenge_id);

    /* Send MSG_GAME_STARTED to both players */
    msg_game_started_t start_msg;
    memset(&start_msg, 0, sizeof(start_msg));
    snprintf(start_msg.game_id, MAX_GAME_ID_LEN, "%s", game_id);
    snprintf(start_msg.player_a, MAX_PSEUDO_LEN, "%s", challenge->challenger);
    snprintf(start_msg.player_b, MAX_PSEUDO_LEN, "%s", session->pseudo);

    /* Send to accepter (Player B) */
    start_msg.your_side = PLAYER_B;
    session_send_message(session, MSG_GAME_STARTED, &start_msg, sizeof(start_msg));

    /* Send to challenger (Player A) */
    start_msg.your_side = PLAYER_A;
    session_send_message(challenger_session, MSG_GAME_STARTED, &start_msg, sizeof(start_msg));
}

/* Handle MSG_CHALLENGE_DECLINE */
void handle_challenge_decline(session_t* session, const msg_challenge_decline_t* decline_msg) {
    if (!decline_msg) {
        session_send_error(session, ERR_INVALID_PARAM, "Invalid decline message");
        return;
    }

    /* Find the challenge */
    challenge_t* challenge = NULL;
    error_code_t err = matchmaking_find_challenge_by_id(g_matchmaking, decline_msg->challenge_id, &challenge);

    if (err != SUCCESS || !challenge) {
        session_send_error(session, ERR_GAME_NOT_FOUND, "Challenge not found or expired");
        return;
    }

    /* Verify that the decliner is the opponent */
    if (strcmp(session->pseudo, challenge->opponent) != 0) {
        session_send_error(session, ERR_INVALID_PARAM, "You are not the recipient of this challenge");
        return;
    }

    printf("Challenge declined: %s -> %s\n", challenge->challenger, session->pseudo);

    /* Remove the challenge */
    matchmaking_remove_challenge_by_id(g_matchmaking, decline_msg->challenge_id);

    /* Notify the challenger */
    session_t* challenger_session = session_registry_find(challenge->challenger);
    if (challenger_session) {
        msg_error_t decline_msg;
        decline_msg.error_code = SUCCESS;
        snprintf(decline_msg.error_msg, 256, "%s declined your challenge", session->pseudo);
        session_send_message(challenger_session, MSG_ERROR, &decline_msg, sizeof(decline_msg));
    }

    /* Confirm to decliner */
    session_send_message(session, MSG_CHALLENGE_SENT, NULL, 0);  /* Reuse as ACK */
}
