/* Client Basic Commands - Implementation
 * Simple command handlers for basic client operations
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_basic_commands.h"
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
    msg_player_list_t list;
    size_t size;

    err = session_recv_message_timeout(session, &type, &list, sizeof(list), &size, 5000);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS || type != MSG_PLAYER_LIST) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }
    
    ui_display_player_list(&list);
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
    session_t* session = client_state_get_session();
    
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
    msg_challenge_response_t response;
    memset(&response, 0, sizeof(response));
    snprintf(response.challenger, MAX_PSEUDO_LEN, "%s", selected_challenger);
    
    message_type_t msg_type = is_decline ? MSG_DECLINE_CHALLENGE : MSG_ACCEPT_CHALLENGE;
    error_code_t err = session_send_message(session, msg_type, &response, sizeof(response));
    
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
