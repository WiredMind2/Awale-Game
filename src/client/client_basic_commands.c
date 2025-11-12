/* Client Basic Commands - Implementation
 * Simple command handlers for basic client operations
 */

#define _POSIX_C_SOURCE 200809L

#include "../../include/client/client_basic_commands.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_ui.h"
#include "../../include/common/messages.h"
#include "../../include/common/protocol.h"
#include "../../include/network/session.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* List connected players */
void cmd_list_players(void) {
    session_t* session = client_state_get_session();
    
    printf("\n📋 Listing connected players...\n");
    
    error_code_t err = session_send_message(session, MSG_LIST_PLAYERS, NULL, 0);
    if (err != SUCCESS) {
        printf("❌ Error sending request: %s\n", error_to_string(err));
        return;
    }
    
    message_type_t type;
    msg_player_list_t list;
    size_t size;
    
    err = session_recv_message(session, &type, &list, sizeof(list), &size);
    if (err != SUCCESS || type != MSG_PLAYER_LIST) {
        printf("❌ Error receiving response\n");
        return;
    }
    
    printf("\n✓ Connected players (%d):\n", list.count);
    printf("─────────────────────────────\n");
    for (int i = 0; i < list.count; i++) {
        printf("  %d. %s (%s)\n", i + 1, list.players[i].pseudo, list.players[i].ip);
    }
    printf("─────────────────────────────\n");
}

/* Challenge a player */
void cmd_challenge_player(void) {
    session_t* session = client_state_get_session();
    const char* my_pseudo = client_state_get_pseudo();
    char opponent[MAX_PSEUDO_LEN];
    
    printf("\n⚔️  Challenge a player\n");
    printf("Enter opponent's pseudo: ");
    
    if (read_line(opponent, MAX_PSEUDO_LEN) == NULL) {
        printf("❌ Invalid input\n");
        return;
    }
    
    if (strlen(opponent) == 0) {
        printf("❌ Pseudo cannot be empty\n");
        return;
    }
    
    if (strcmp(opponent, my_pseudo) == 0) {
        printf("❌ You cannot challenge yourself!\n");
        return;
    }
    
    msg_challenge_t challenge;
    memset(&challenge, 0, sizeof(challenge));
    snprintf(challenge.challenger, MAX_PSEUDO_LEN, "%s", my_pseudo);
    snprintf(challenge.opponent, MAX_PSEUDO_LEN, "%s", opponent);
    
    error_code_t err = session_send_message(session, MSG_CHALLENGE, &challenge, sizeof(challenge));
    if (err != SUCCESS) {
        printf("❌ Error sending challenge: %s\n", error_to_string(err));
        return;
    }
    
    /* Wait for acknowledgment */
    message_type_t type;
    char response[MAX_MESSAGE_SIZE];
    size_t size;
    
    err = session_recv_message_timeout(session, &type, response, MAX_MESSAGE_SIZE, &size, 5000);
    if (err == ERR_TIMEOUT) {
        printf("❌ Timeout: Server did not respond\n");
        return;
    }
    if (err != SUCCESS) {
        printf("❌ Error receiving response: %s\n", error_to_string(err));
        return;
    }
    
    if (type == MSG_CHALLENGE_SENT) {
        printf("✓ Challenge sent to %s!\n", opponent);
        printf("💡 They will receive a notification. Wait for them to accept or decline.\n");
    } else if (type == MSG_ERROR) {
        msg_error_t* error = (msg_error_t*)response;
        printf("❌ Error: %s\n", error->error_msg);
    }
}

/* View and respond to challenges */
void cmd_view_challenges(void) {
    session_t* session = client_state_get_session();
    
    int count = pending_challenges_count();
    
    if (count == 0) {
        printf("\n✓ No pending challenges\n");
        return;
    }
    
    printf("\n📨 Pending challenges (%d):\n", count);
    printf("═══════════════════════════════════════════════════\n");
    
    /* Display challenges */
    for (int i = 0; i < count; i++) {
        pending_challenge_t* challenge = pending_challenges_get(i);
        if (challenge) {
            printf("  %d. %s wants to play!\n", i + 1, challenge->challenger);
        }
    }
    
    printf("═══════════════════════════════════════════════════\n");
    printf("\nChoose an option:\n");
    printf("  [number] Accept challenge\n");
    printf("  d[number] Decline challenge\n");
    printf("  0 Cancel\n");
    printf("Your choice: ");
    
    char choice[32];
    if (read_line(choice, 32) == NULL || strlen(choice) == 0) {
        return;
    }
    
    /* Parse choice */
    bool is_decline = (choice[0] == 'd' || choice[0] == 'D');
    int index = atoi(is_decline ? choice + 1 : choice);
    
    if (index == 0) {
        printf("Cancelled.\n");
        return;
    }
    
    if (index < 1 || index > count) {
        printf("❌ Invalid choice\n");
        return;
    }
    
    /* Find the selected challenge */
    pending_challenge_t* selected = pending_challenges_get(index - 1);
    if (!selected) {
        printf("❌ Challenge not found\n");
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
        printf("❌ Error sending response: %s\n", error_to_string(err));
        return;
    }
    
    /* Remove from pending */
    pending_challenges_remove(selected_challenger);
    
    if (is_decline) {
        printf("✓ Challenge from %s declined\n", selected_challenger);
    } else {
        printf("✓ Challenge from %s accepted! Game will start shortly...\n", selected_challenger);
    }
}

/* Set player bio */
void cmd_set_bio(void) {
    session_t* session = client_state_get_session();
    
    printf("\n📝 Setting your bio (max 10 lines, 255 chars each)\n");
    printf("Enter your bio lines. Enter an empty line to finish:\n");
    
    msg_set_bio_t bio_msg;
    memset(&bio_msg, 0, sizeof(bio_msg));
    
    char line[256];
    int line_count = 0;
    
    while (line_count < 10) {
        printf("Line %d: ", line_count + 1);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }
        
        /* Empty line ends input */
        if (len == 0) {
            break;
        }
        
        /* Copy line to bio */
        snprintf(bio_msg.bio[line_count], 256, "%s", line);
        bio_msg.bio[line_count][255] = '\0';
        line_count++;
    }
    
    bio_msg.bio_lines = line_count;
    
    if (line_count == 0) {
        printf("❌ No bio entered\n");
        return;
    }
    
    error_code_t err = session_send_message(session, MSG_SET_BIO, &bio_msg, sizeof(bio_msg));
    if (err != SUCCESS) {
        printf("❌ Error sending bio: %s\n", error_to_string(err));
        return;
    }
    
    message_type_t type;
    char dummy[1];
    size_t size;
    
    err = session_recv_message(session, &type, dummy, sizeof(dummy), &size);
    if (err != SUCCESS) {
        printf("❌ Error receiving response\n");
        return;
    }
    
    printf("✓ Bio updated successfully (%d lines)\n", line_count);
}

/* View player bio */
void cmd_view_bio(void) {
    session_t* session = client_state_get_session();
    
    printf("\n📖 View player bio\n");
    printf("Enter player name: ");
    
    char target_player[MAX_PSEUDO_LEN];
    if (!fgets(target_player, sizeof(target_player), stdin)) {
        printf("❌ Error reading input\n");
        return;
    }
    
    /* Remove newline */
    size_t len = strlen(target_player);
    if (len > 0 && target_player[len - 1] == '\n') {
        target_player[len - 1] = '\0';
    }
    
    if (strlen(target_player) == 0) {
        printf("❌ No player name entered\n");
        return;
    }
    
    msg_get_bio_t bio_req;
    memset(&bio_req, 0, sizeof(bio_req));
    snprintf(bio_req.target_player, MAX_PSEUDO_LEN, "%s", target_player);
    
    error_code_t err = session_send_message(session, MSG_GET_BIO, &bio_req, sizeof(bio_req));
    if (err != SUCCESS) {
        printf("❌ Error sending request: %s\n", error_to_string(err));
        return;
    }
    
    message_type_t type;
    msg_bio_response_t response;
    size_t size;
    
    err = session_recv_message(session, &type, &response, sizeof(response), &size);
    if (err != SUCCESS || type != MSG_BIO_RESPONSE) {
        printf("❌ Error receiving response\n");
        return;
    }
    
    if (!response.success) {
        printf("❌ %s\n", response.message);
        return;
    }
    
    printf("\n📖 Bio for %s:\n", response.player);
    printf("─────────────────────────────\n");
    
    if (response.bio_lines == 0) {
        printf("  (No bio set)\n");
    } else {
        for (int i = 0; i < response.bio_lines; i++) {
            printf("  %s\n", response.bio[i]);
        }
    }
    printf("─────────────────────────────\n");
}

/* View player statistics */
void cmd_view_player_stats(void) {
    session_t* session = client_state_get_session();
    
    printf("\n📊 View player statistics\n");
    printf("Enter player name (or press Enter for your own stats): ");
    
    char target_player[MAX_PSEUDO_LEN];
    if (!fgets(target_player, sizeof(target_player), stdin)) {
        printf("❌ Error reading input\n");
        return;
    }
    
    /* Remove newline */
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
        printf("❌ Error sending request: %s\n", error_to_string(err));
        return;
    }
    
    message_type_t type;
    msg_player_stats_t response;
    size_t size;
    
    err = session_recv_message(session, &type, &response, sizeof(response), &size);
    if (err != SUCCESS || type != MSG_PLAYER_STATS) {
        printf("❌ Error receiving response\n");
        return;
    }
    
    if (!response.success) {
        printf("❌ %s\n", response.message);
        return;
    }
    
    printf("\n📊 Statistics for %s:\n", response.player);
    printf("─────────────────────────────\n");
    printf("  Games played: %d\n", response.games_played);
    printf("  Games won:    %d\n", response.games_won);
    printf("  Games lost:   %d\n", response.games_lost);
    printf("  Total score:  %d\n", response.total_score);
    printf("  Win rate:     %.1f%%\n", 
           response.games_played > 0 ? 
           (float)response.games_won / response.games_played * 100 : 0);
    printf("  First seen:   %s", ctime(&response.first_seen));
    printf("  Last seen:    %s", ctime(&response.last_seen));
    printf("─────────────────────────────\n");
}

/* Send chat message - Interactive mode */
void cmd_chat(void) {
    session_t* session = client_state_get_session();
    const char* my_pseudo = client_state_get_pseudo();

    printf("\n💬 Interactive Chat Mode\n");
    printf("Select recipient: 'all' for global chat or enter a player name for private chat\n");
    printf("Recipient: ");

    char recipient_input[MAX_PSEUDO_LEN];
    if (read_line(recipient_input, sizeof(recipient_input)) == NULL) {
        printf("❌ Invalid input\n");
        return;
    }

    char recipient[MAX_PSEUDO_LEN];
    memset(recipient, 0, sizeof(recipient));

    if (strcmp(recipient_input, "all") == 0) {
        /* Global chat - leave recipient empty */
        printf("✓ Global chat mode selected. Type your messages below.\n");
    } else {
        /* Private chat */
        snprintf(recipient, MAX_PSEUDO_LEN, "%s", recipient_input);
        if (strlen(recipient) == 0) {
            printf("❌ Invalid recipient name\n");
            return;
        }
        if (strcmp(recipient, my_pseudo) == 0) {
            printf("❌ You cannot send private messages to yourself\n");
            return;
        }
        printf("✓ Private chat mode selected. Sending messages to %s.\n", recipient);
    }

    printf("Type your message (or 'exit'/'quit' to leave chat mode):\n");

    /* Interactive chat loop */
    while (1) {
        printf("> ");
        fflush(stdout);

        char message[MAX_CHAT_LEN];
        memset(message, 0, sizeof(message));

        if (read_line(message, sizeof(message)) == NULL) {
            printf("❌ Invalid input\n");
            break;
        }

        /* Check for exit commands */
        if (strcmp(message, "exit") == 0 || strcmp(message, "quit") == 0 ||
            strcmp(message, "") == 0) {
            printf("✓ Exited chat mode\n");
            break;
        }

        if (strlen(message) == 0) {
            printf("❌ Message cannot be empty\n");
            continue;
        }

        /* Prepare message */
        msg_send_chat_t chat_msg;
        memset(&chat_msg, 0, sizeof(chat_msg));
        snprintf(chat_msg.recipient, MAX_PSEUDO_LEN, "%s", recipient);
        snprintf(chat_msg.message, MAX_CHAT_LEN, "%s", message);

        error_code_t err = session_send_message(session, MSG_SEND_CHAT, &chat_msg, sizeof(chat_msg));
        if (err != SUCCESS) {
            printf("❌ Error sending chat message: %s\n", error_to_string(err));
            continue;
        }

        /* Wait for acknowledgment */
        message_type_t type;
        char response[MAX_MESSAGE_SIZE];
        size_t size;

        err = session_recv_message_timeout(session, &type, response, MAX_MESSAGE_SIZE, &size, 5000);
        if (err == ERR_TIMEOUT) {
            printf("❌ Timeout: Server did not respond\n");
            continue;
        }
        if (err != SUCCESS) {
            printf("❌ Error receiving response: %s\n", error_to_string(err));
            continue;
        }

        if (type == MSG_ERROR) {
            msg_error_t* error = (msg_error_t*)response;
            printf("❌ Error: %s\n", error->error_msg);
        } else {
            /* Message sent successfully - the notification handler will show it */
            /* Continue to next message */
        }
    }
}
