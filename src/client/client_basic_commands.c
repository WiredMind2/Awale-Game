/* Client Basic Commands - Implementation
 * Simple command handlers for basic client operations
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_basic_commands.h"
#include "../../include/client/client_notifications.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_ui.h"
#include "../../include/client/client_logging.h"
#include "../../include/common/messages.h"
#include "../../include/common/protocol.h"
#include "../../include/network/session.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* List connected players */
void cmd_list_players(void) {
    session_t* session = client_state_get_session();

    client_log_info(CLIENT_LOG_LISTING_PLAYERS);

    error_code_t err = session_send_message(session, MSG_LIST_PLAYERS, NULL, 0);
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
        return;
    }
    message_type_t type;
    msg_player_list_t* list = calloc(1, sizeof(msg_player_list_t));
    if (!list) {
        client_log_error("Memory allocation failed");
        return;
    }
    size_t size;

    err = session_recv_message_timeout(session, &type, list, sizeof(*list), &size, 10000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        free(list);
        return;
    }
    if (err != SUCCESS || type != MSG_PLAYER_LIST) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        free(list);
        return;
    }

    printf("Received player list with %d players\n", list->count);
    ui_display_player_list(list);
    free(list);
}

/* Challenge a player */
__attribute__((unused)) void cmd_challenge_player(void)
{
    const char* my_pseudo = client_state_get_pseudo();
    session_t* session = client_state_get_session();
    (void)session;  /* Suppress unused warning */
    char opponent[MAX_PSEUDO_LEN];
    
    client_log_info(CLIENT_LOG_CHALLENGE_PLAYER);
    client_log_info(CLIENT_LOG_ENTER_OPPONENT_PSEUDO);
    
    if (read_line(opponent, MAX_PSEUDO_LEN) == NULL) {
        client_log_error(CLIENT_LOG_INVALID_INPUT);
        return;
    }
    
    if (strlen(opponent) == 0) {
        client_log_error(CLIENT_LOG_PSEUDO_EMPTY);
        return;
    }
    
    if (strcmp(opponent, my_pseudo) == 0) {
        client_log_error(CLIENT_LOG_CHALLENGE_SELF);
        return;
    }
    
    msg_challenge_t challenge;
    memset(&challenge, 0, sizeof(challenge));
    snprintf(challenge.challenger, MAX_PSEUDO_LEN, "%s", my_pseudo);
    snprintf(challenge.opponent, MAX_PSEUDO_LEN, "%s", opponent);
    
    error_code_t err = session_send_message(session, MSG_CHALLENGE, &challenge, sizeof(challenge));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_CHALLENGE, error_to_string(err));
        return;
    }
    
    /* Wait for acknowledgment */
    message_type_t type;
    char response[MAX_MESSAGE_SIZE];
    size_t size;
    
    err = session_recv_message_timeout(session, &type, response, MAX_MESSAGE_SIZE, &size, 5000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE_WITH_ERR, error_to_string(err));
        return;
    }
    
    if (type == MSG_CHALLENGE_SENT) {
    ui_display_challenge_sent(opponent);
    } else if (type == MSG_ERROR) {
        msg_error_t* error = (msg_error_t*)response;
    ui_display_challenge_error(error->error_msg);
    }
}

/* View and respond to challenges */
__attribute__((unused)) void cmd_view_challenges(void)
{
    int count = pending_challenges_count();

    ui_display_pending_challenges(count);

    if (count == 0) {
        return;
    }

    char choice[32];
    if (read_line(choice, 32) == NULL || strlen(choice) == 0) {
        return;
    }

    /* Parse choice */
    bool is_decline = (choice[0] == 'd' || choice[0] == 'D');
    int index = atoi(is_decline ? choice + 1 : choice);

    if (index == 0) {
        client_log_info(CLIENT_LOG_CANCELLED);
        return;
    }

    if (index < 1 || index > count) {
        client_log_error(CLIENT_LOG_INVALID_CHOICE);
        return;
    }

    /* Find the selected challenge */
    pending_challenge_t* selected = pending_challenges_get(index - 1);
    if (!selected) {
        client_log_error(CLIENT_LOG_CHALLENGE_NOT_FOUND);
        return;
    }

    char selected_challenger[MAX_PSEUDO_LEN];
    snprintf(selected_challenger, MAX_PSEUDO_LEN, "%s", selected->challenger);

    /* Send accept or decline */
    error_code_t err;
    if (is_decline) {
        err = send_challenge_decline(selected->challenge_id);
    } else {
        err = send_challenge_accept(selected->challenge_id);
    }

    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE_WITH_ERR, error_to_string(err));
        return;
    }

    /* Remove from pending */
    pending_challenges_remove(selected_challenger);

    ui_display_challenge_response(selected_challenger, !is_decline);
}

/* Set player bio */
__attribute__((unused)) void cmd_set_bio(void)
{
    session_t* session = client_state_get_session();
    
    msg_set_bio_t bio_msg;
    memset(&bio_msg, 0, sizeof(bio_msg));
    
    ui_prompt_bio(&bio_msg);
    
    if (bio_msg.bio_lines == 0) {
        client_log_info(CLIENT_LOG_NO_BIO_ENTERED);
        return;
    }
    
    error_code_t err = session_send_message(session, MSG_SET_BIO, &bio_msg, sizeof(bio_msg));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_BIO, error_to_string(err));
        return;
    }
    
    message_type_t type;
    char dummy[1];
    size_t size;

    err = session_recv_message_timeout(session, &type, dummy, sizeof(dummy), &size, 5000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }
    ui_display_bio_updated(bio_msg.bio_lines);
}

/* View player bio */
__attribute__((unused)) void cmd_view_bio(void)
{
    session_t* session = client_state_get_session();
    
    client_log_info(CLIENT_LOG_VIEW_BIO_HEADER);
    client_log_info(CLIENT_LOG_VIEW_BIO_PROMPT);
    
    char target_player[MAX_PSEUDO_LEN];
    if (!fgets(target_player, sizeof(target_player), stdin)) {
        client_log_error(CLIENT_LOG_ERROR_READING_INPUT);
        return;
    }
    
    /* Remove newline */
    size_t len = strlen(target_player);
    if (len > 0 && target_player[len - 1] == '\n') {
        target_player[len - 1] = '\0';
    }
    
    if (strlen(target_player) == 0) {
        client_log_error(CLIENT_LOG_NO_PLAYER_NAME_ENTERED);
        return;
    }
    
    msg_get_bio_t bio_req;
    memset(&bio_req, 0, sizeof(bio_req));
    snprintf(bio_req.target_player, MAX_PSEUDO_LEN, "%s", target_player);
    
    error_code_t err = session_send_message(session, MSG_GET_BIO, &bio_req, sizeof(bio_req));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
        return;
    }
    
    message_type_t type;
    msg_bio_response_t response;
    size_t size;

    err = session_recv_message_timeout(session, &type, &response, sizeof(response), &size, 5000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS || type != MSG_BIO_RESPONSE) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }
    
    ui_display_bio(&response);
}

/* View player statistics */
__attribute__((unused)) void cmd_view_player_stats(void)
{
    session_t* session = client_state_get_session();
    
    client_log_info(CLIENT_LOG_VIEW_STATS_HEADER);
    client_log_info(CLIENT_LOG_VIEW_STATS_PROMPT);
    
    char target_player[MAX_PSEUDO_LEN];
    if (!fgets(target_player, sizeof(target_player), stdin)) {
        client_log_error(CLIENT_LOG_ERROR_READING_INPUT);
        return;
    }    /* Remove newline */
    size_t len = strlen(target_player);
    if (len > 0 && target_player[len - 1] == '\n') {
        target_player[len - 1] = '\0';
    }
    
    /* If empty, use current player's pseudo */
    if (strlen(target_player) == 0) {
        strncpy(target_player, client_state_get_pseudo(), MAX_PSEUDO_LEN - 1);
    }
    
    msg_get_player_stats_t stats_req;
    memset(&stats_req, 0, sizeof(stats_req));
    snprintf(stats_req.target_player, MAX_PSEUDO_LEN, "%s", target_player);
    
    error_code_t err = session_send_message(session, MSG_GET_PLAYER_STATS, &stats_req, sizeof(stats_req));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
        return;
    }
    
    message_type_t type;
    msg_player_stats_t response;
    size_t size;

    err = session_recv_message_timeout(session, &type, &response, sizeof(response), &size, 5000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS || type != MSG_PLAYER_STATS) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }
    
    ui_display_player_stats(&response);
}

/* Send chat message - Interactive mode */
__attribute__((unused)) void cmd_chat(void)
{
    session_t* session = client_state_get_session();
    const char* my_pseudo = client_state_get_pseudo();

    client_log_info(CLIENT_LOG_CHAT_MODE_HEADER);
    client_log_info(CLIENT_LOG_CHAT_MODE_INSTRUCTIONS);
    client_log_info(CLIENT_LOG_CHAT_MODE_RECIPIENT);

    char recipient_input[MAX_PSEUDO_LEN];
    if (read_line(recipient_input, sizeof(recipient_input)) == NULL) {
        client_log_error(CLIENT_LOG_INVALID_INPUT);
        return;
    }

    char recipient[MAX_PSEUDO_LEN];
    memset(recipient, 0, sizeof(recipient));

    if (strcmp(recipient_input, "all") == 0) {
        /* Global chat - leave recipient empty */
        client_log_info(CLIENT_LOG_CHAT_MODE_GLOBAL);
    } else {
        /* Private chat */
        snprintf(recipient, MAX_PSEUDO_LEN, "%s", recipient_input);
        if (strlen(recipient) == 0) {
            client_log_error(CLIENT_LOG_INVALID_RECIPIENT);
            return;
        }
        if (strcmp(recipient, my_pseudo) == 0) {
            client_log_error(CLIENT_LOG_CHALLENGE_SELF);
            return;
        }
        client_log_info(CLIENT_LOG_CHAT_MODE_PRIVATE, recipient);
    }

    client_log_info(CLIENT_LOG_CHAT_MODE_INSTRUCTIONS2);

    /* Interactive chat loop */
    while (client_state_is_running()) {
        client_log_info(CLIENT_LOG_CHAT_PROMPT);
        fflush(stdout);

        char message[MAX_CHAT_LEN];
        memset(message, 0, sizeof(message));

        if (read_line(message, sizeof(message)) == NULL) {
            client_log_error(CLIENT_LOG_INVALID_INPUT);
            break;
        }

        /* Check for exit commands */
        if (strcmp(message, "exit") == 0 || strcmp(message, "quit") == 0 ||
            strcmp(message, "") == 0) {
            client_log_info(CLIENT_LOG_CHAT_EXITED);
            break;
        }

        if (strlen(message) == 0) {
            client_log_error(CLIENT_LOG_CHAT_MESSAGE_EMPTY);
            continue;
        }

        /* Prepare message */
        msg_send_chat_t chat_msg;
        memset(&chat_msg, 0, sizeof(chat_msg));
        snprintf(chat_msg.recipient, MAX_PSEUDO_LEN, "%s", recipient);
        snprintf(chat_msg.message, MAX_CHAT_LEN, "%s", message);

        error_code_t err = session_send_message(session, MSG_SEND_CHAT, &chat_msg, sizeof(chat_msg));
        if (err != SUCCESS) {
            client_log_error(CLIENT_LOG_CHAT_ERROR_SENDING, error_to_string(err));
            continue;
        }

        /* Chat messages are handled asynchronously by the notification listener */
        /* No response waiting needed - MSG_CHAT_MESSAGE will be displayed by notification handler */
    }
}

/* Friend management menu */
__attribute__((unused)) void cmd_friend_management(void)
{
    session_t* session = client_state_get_session();

    while (client_state_is_running()) {
        ui_display_friend_menu();

        int choice;
        if (scanf("%d", &choice) != 1) {
            clear_input();
            client_log_error(CLIENT_LOG_INVALID_INPUT);
            continue;
        }
        clear_input();

        switch (choice) {
            case 1: {  /* Add friend */
                printf(CLIENT_UI_FRIEND_ADD_HEADER);
                printf(CLIENT_UI_FRIEND_ADD_PROMPT);

                char friend_name[MAX_PSEUDO_LEN];
                if (read_line(friend_name, MAX_PSEUDO_LEN) == NULL || strlen(friend_name) == 0) {
                    client_log_error(CLIENT_LOG_INVALID_INPUT);
                    continue;
                }

                msg_add_friend_t add_msg;
                memset(&add_msg, 0, sizeof(add_msg));
                snprintf(add_msg.friend_pseudo, MAX_PSEUDO_LEN, "%s", friend_name);

                error_code_t err = session_send_message(session, MSG_ADD_FRIEND, &add_msg, sizeof(add_msg));
                if (err != SUCCESS) {
                    printf(CLIENT_UI_FRIEND_ADD_ERROR, error_to_string(err));
                    continue;
                }

                message_type_t type;
                char response[MAX_MESSAGE_SIZE];
                size_t size;
                err = session_recv_message_timeout(session, &type, response, sizeof(response), &size, 5000);
                if (err == SUCCESS && type == MSG_CHALLENGE_SENT) {  /* Reuse ACK message */
                    printf(CLIENT_UI_FRIEND_ADD_SUCCESS, friend_name);
                } else if (err == SUCCESS && type == MSG_ERROR) {
                    msg_error_t* error = (msg_error_t*)response;
                    printf(CLIENT_UI_FRIEND_ADD_ERROR, error->error_msg);
                } else {
                    printf(CLIENT_UI_FRIEND_ADD_ERROR, "Server error");
                }
                break;
            }

            case 2: {  /* Remove friend */
                printf(CLIENT_UI_FRIEND_REMOVE_HEADER);
                printf(CLIENT_UI_FRIEND_REMOVE_PROMPT);

                char friend_name[MAX_PSEUDO_LEN];
                if (read_line(friend_name, MAX_PSEUDO_LEN) == NULL || strlen(friend_name) == 0) {
                    client_log_error(CLIENT_LOG_INVALID_INPUT);
                    continue;
                }

                msg_remove_friend_t remove_msg;
                memset(&remove_msg, 0, sizeof(remove_msg));
                snprintf(remove_msg.friend_pseudo, MAX_PSEUDO_LEN, "%s", friend_name);

                error_code_t err = session_send_message(session, MSG_REMOVE_FRIEND, &remove_msg, sizeof(remove_msg));
                if (err != SUCCESS) {
                    printf(CLIENT_UI_FRIEND_REMOVE_ERROR, error_to_string(err));
                    continue;
                }

                message_type_t type;
                char response[MAX_MESSAGE_SIZE];
                size_t size;
                err = session_recv_message_timeout(session, &type, response, sizeof(response), &size, 5000);
                if (err == SUCCESS && type == MSG_CHALLENGE_SENT) {  /* Reuse ACK message */
                    printf(CLIENT_UI_FRIEND_REMOVE_SUCCESS, friend_name);
                } else if (err == SUCCESS && type == MSG_ERROR) {
                    msg_error_t* error = (msg_error_t*)response;
                    printf(CLIENT_UI_FRIEND_REMOVE_ERROR, error->error_msg);
                } else {
                    printf(CLIENT_UI_FRIEND_REMOVE_ERROR, "Server error");
                }
                break;
            }

            case 3: {  /* List friends */
                error_code_t err = session_send_message(session, MSG_LIST_FRIENDS, NULL, 0);
                if (err != SUCCESS) {
                    client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
                    continue;
                }

                message_type_t type;
                msg_list_friends_t friends;
                size_t size;
                err = session_recv_message_timeout(session, &type, &friends, sizeof(friends), &size, 5000);
                if (err == ERR_TIMEOUT) {
                    client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
                    continue;
                }
                if (err != SUCCESS || type != MSG_LIST_FRIENDS) {
                    client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
                    continue;
                }

                ui_display_friend_list(&friends);
                break;
            }

            case 4:  /* Back to main menu */
                return;

            default:
                client_log_error(CLIENT_LOG_INVALID_CHOICE);
                break;
        }
    }
}

/* List saved games for review */
__attribute__((unused)) void cmd_list_saved_games(void) {
    session_t* session = client_state_get_session();

    client_log_info(CLIENT_LOG_LIST_SAVED_GAMES_HEADER);
    client_log_info(CLIENT_LOG_LIST_SAVED_GAMES_PROMPT);

    char player_filter[MAX_PSEUDO_LEN];
    memset(player_filter, 0, sizeof(player_filter));

    if (read_line(player_filter, sizeof(player_filter)) == NULL) {
        client_log_error(CLIENT_LOG_INVALID_INPUT);
        return;
    }

    /* If empty, list all games */
    msg_list_saved_games_t req;
    memset(&req, 0, sizeof(req));
    if (strlen(player_filter) > 0) {
        snprintf(req.player, MAX_PSEUDO_LEN, "%s", player_filter);
    }

    error_code_t err = session_send_message(session, MSG_LIST_SAVED_GAMES, &req, sizeof(req));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
        return;
    }

    message_type_t type;
    msg_saved_game_list_t list;
    size_t size;

    err = session_recv_message_timeout(session, &type, &list, sizeof(list), &size, 5000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS || type != MSG_SAVED_GAME_LIST) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }

    /* Display the list - reuse game list UI */
    printf("Saved Games for Review:\n");
    if (list.count == 0) {
        printf("No saved games found.\n");
        return;
    }

    for (int i = 0; i < list.count; i++) {
        game_info_t* game = &list.games[i];
        printf("%d. %s vs %s (%s)\n",
               i + 1, game->player_a, game->player_b, game->game_id);
    }
}

/* View a saved game */
__attribute__((unused)) void cmd_view_saved_game(void) {
    session_t* session = client_state_get_session();

    client_log_info(CLIENT_LOG_VIEW_SAVED_GAME_HEADER);

    /* First, get and display the list of saved games */
    msg_list_saved_games_t list_req;
    memset(&list_req, 0, sizeof(list_req));
    /* List all games for selection */
    error_code_t err = session_send_message(session, MSG_LIST_SAVED_GAMES, &list_req, sizeof(list_req));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
        return;
    }

    message_type_t type;
    msg_saved_game_list_t list;
    size_t size;

    err = session_recv_message_timeout(session, &type, &list, sizeof(list), &size, 5000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS || type != MSG_SAVED_GAME_LIST) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }

    /* Display the list */
    printf("Saved Games for Review:\n");
    if (list.count == 0) {
        printf("No saved games found.\n");
        return;
    }

    for (int i = 0; i < list.count; i++) {
        game_info_t* game = &list.games[i];
        printf("%d. %s vs %s (ID: %s)\n",
               i + 1, game->player_a, game->player_b, game->game_id);
    }

    /* Now prompt for selection */
    printf("\nEnter game number (1-%d) or full game ID: ", list.count);

    char input[MAX_GAME_ID_LEN];
    if (read_line(input, sizeof(input)) == NULL || strlen(input) == 0) {
        client_log_error(CLIENT_LOG_INVALID_INPUT);
        return;
    }

    char game_id[MAX_GAME_ID_LEN];
    memset(game_id, 0, sizeof(game_id));

    /* Check if input is a number (selection from list) */
    int selection = atoi(input);
    if (selection >= 1 && selection <= list.count) {
        /* Use the game ID from the selected game */
        game_info_t* selected_game = &list.games[selection - 1];
        snprintf(game_id, MAX_GAME_ID_LEN, "%s", selected_game->game_id);
    } else {
        /* Treat input as direct game ID */
        snprintf(game_id, MAX_GAME_ID_LEN, "%s", input);
    }

    msg_view_saved_game_t req;
    memset(&req, 0, sizeof(req));
    snprintf(req.game_id, MAX_GAME_ID_LEN, "%s", game_id);

    err = session_send_message(session, MSG_VIEW_SAVED_GAME, &req, sizeof(req));
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
        return;
    }

    msg_saved_game_state_t board_state;

    err = session_recv_message_timeout(session, &type, &board_state, sizeof(board_state), &size, 5000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS || type != MSG_SAVED_GAME_STATE) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }

    if (!board_state.exists) {
        printf("Game not found.\n");
        return;
    }

    /* Display the saved game board */
    printf("Saved Game: %s vs %s\n", board_state.player_a, board_state.player_b);
    ui_display_board_simple((const board_t*)&board_state.pits);  /* Cast to board_t for display */
}
