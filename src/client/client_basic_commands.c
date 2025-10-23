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
