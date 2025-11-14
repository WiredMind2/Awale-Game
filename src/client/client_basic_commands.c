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
#include "../../include/game/board.h"
#include "../../include/game/rules.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* List connected players */
void cmd_list_players(void) {
    session_t* session = client_state_get_session();

    client_log_info(CLIENT_LOG_LISTING_PLAYERS);

    error_code_t err = session_send_message(session, MSG_LIST_PLAYERS, NULL, 0);
    if (err != SUCCESS) {
        int ret = system("clear");
        (void)ret; // Suppress unused variable warning
        printf("Error sending request: %s\n", error_to_string(err));
        printf("\nPress Enter to return to menu...");
        fflush(stdout);
        char dummy[2];
        read_line(dummy, 2);
        return;
    }
    message_type_t type;
    msg_player_list_t* list = calloc(1, sizeof(msg_player_list_t));
    if (!list) {
        client_log_error("Memory allocation failed");
        return;
    }
    size_t size;

    err = session_recv_message_timeout(session, &type, list, sizeof(*list), &size, 10000, NULL, 0);
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
    message_type_t expected[] = {MSG_CHALLENGE_SENT, MSG_ERROR};
    message_type_t type;
    char response[MAX_MESSAGE_SIZE];
    size_t size;

    err = session_recv_message_timeout(session, &type, response, MAX_MESSAGE_SIZE, &size, 5000, expected, 2);
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

    printf("\nPress Enter to return to menu...");
    fflush(stdout);
    char dummy[2];
    read_line(dummy, 2);
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

    err = session_recv_message_timeout(session, &type, dummy, sizeof(dummy), &size, 5000, NULL, 0);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }
    ui_display_bio_updated(bio_msg.bio_lines);

    printf("\nPress Enter to return to menu...");
    fflush(stdout);
    char pause_dummy[2];
    read_line(pause_dummy, 2);
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

    err = session_recv_message_timeout(session, &type, &response, sizeof(response), &size, 5000, NULL, 0);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS || type != MSG_BIO_RESPONSE) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }
    
    ui_display_bio(&response);

    printf("\nPress Enter to return to menu...");
    fflush(stdout);
    char dummy[2];
    read_line(dummy, 2);
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

    err = session_recv_message_timeout(session, &type, &response, sizeof(response), &size, 5000, NULL, 0);
    if (err == ERR_TIMEOUT) {
        client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
        return;
    }
    if (err != SUCCESS || type != MSG_PLAYER_STATS) {
        client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
        return;
    }
    
    ui_display_player_stats(&response);

    printf("\nPress Enter to return to menu...");
    fflush(stdout);
    char dummy[2];
    read_line(dummy, 2);
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
    int current_selection = 1;

    while (client_state_is_running()) {
        ui_display_friend_menu_highlighted(current_selection);

        int choice = read_arrow_input(&current_selection, 1, 4);
        if (choice == -1) { /* ESC pressed */
            return; /* Back to main menu */
        } else if (choice >= 1 && choice <= 4) {
            /* Valid choice selected */
            current_selection = choice;
        } else {
            /* Continue loop for navigation */
            continue;
        }

        switch (current_selection) {
            case 1: {  /* Add friend */
                printf(CLIENT_UI_FRIEND_ADD_HEADER);
                printf(CLIENT_UI_FRIEND_ADD_PROMPT);

                char friend_name[MAX_PSEUDO_LEN];
                if (read_line(friend_name, MAX_PSEUDO_LEN) == NULL || strlen(friend_name) == 0) {
                    client_log_error(CLIENT_LOG_INVALID_INPUT);
                    break;
                }

                msg_add_friend_t add_msg;
                memset(&add_msg, 0, sizeof(add_msg));
                snprintf(add_msg.friend_pseudo, MAX_PSEUDO_LEN, "%s", friend_name);

                error_code_t err = session_send_message(session, MSG_ADD_FRIEND, &add_msg, sizeof(add_msg));
                if (err != SUCCESS) {
                    printf(CLIENT_UI_FRIEND_ADD_ERROR, error_to_string(err));
                    break;
                }

                message_type_t type;
                char response[MAX_MESSAGE_SIZE];
                size_t size;
                err = session_recv_message_timeout(session, &type, response, sizeof(response), &size, 5000, NULL, 0);
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
                    break;
                }

                msg_remove_friend_t remove_msg;
                memset(&remove_msg, 0, sizeof(remove_msg));
                snprintf(remove_msg.friend_pseudo, MAX_PSEUDO_LEN, "%s", friend_name);

                error_code_t err = session_send_message(session, MSG_REMOVE_FRIEND, &remove_msg, sizeof(remove_msg));
                if (err != SUCCESS) {
                    printf(CLIENT_UI_FRIEND_REMOVE_ERROR, error_to_string(err));
                    break;
                }

                message_type_t type;
                char response[MAX_MESSAGE_SIZE];
                size_t size;
                err = session_recv_message_timeout(session, &type, response, sizeof(response), &size, 5000, NULL, 0);
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
                    break;
                }

                message_type_t type;
                msg_list_friends_t friends;
                size_t size;
                err = session_recv_message_timeout(session, &type, &friends, sizeof(friends), &size, 5000, NULL, 0);
                if (err == ERR_TIMEOUT) {
                    client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
                    break;
                }
                if (err != SUCCESS || type != MSG_LIST_FRIENDS) {
                    client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
                    break;
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

/* Start a game against AI */
__attribute__((unused)) void cmd_start_ai_game(void) {
    session_t* session = client_state_get_session();

    client_log_info("Starting game against AI...");

    error_code_t err = session_send_message(session, MSG_START_AI_GAME, NULL, 0);
    if (err != SUCCESS) {
        client_log_error("Error sending AI game request: %s", error_to_string(err));
        printf("\nPress Enter to return to menu...");
        fflush(stdout);
        char dummy[2];
        read_line(dummy, 2);
        return;
    }

    /* Wait for game started notification - it will be handled by the notification listener */
    /* The game will start automatically when MSG_GAME_STARTED is received */

    printf("AI game request sent. Waiting for game to start...\n");
    printf("\nPress Enter to return to menu...");
    fflush(stdout);
    char dummy[2];
    read_line(dummy, 2);
}

/* Tutorial mode - Interactive step-by-step guide */
__attribute__((unused)) void cmd_tutorial(void) {
    printf("\n═════════════════════════════════════════════════════════\n");
    printf("                    AWALE TUTORIAL                      \n");
    printf("═════════════════════════════════════════════════════════\n");
    printf("Welcome to the Awale tutorial! This will teach you the basics.\n");
    printf("Press Enter to continue through each step, or 'q' to quit.\n\n");

    int step = 0;
    char input[10];

    while (step < 10) {
        switch (step) {
            case 0: {  /* Introduction */
                printf("Step 1: Introduction to Awale\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("Awale (also called Oware) is a two-player strategy game from Africa.\n");
                printf("The goal is to capture more seeds than your opponent.\n");
                printf("Each player controls 6 pits on their side of the board.\n\n");
                break;
            }
            case 1: {  /* Board setup */
                printf("Step 2: Board Setup\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("The board has 12 pits arranged in 2 rows of 6.\n");
                printf("Each pit starts with 4 seeds.\n");
                printf("Total: 48 seeds on the board.\n\n");

                board_t tutorial_board;
                board_init(&tutorial_board);
                ui_display_board_detailed(&tutorial_board, "Player A", "Player B");
                printf("\nPlayer A controls pits 0-5 (bottom row).\n");
                printf("Player B controls pits 6-11 (top row).\n\n");
                break;
            }
            case 2: {  /* Basic move */
                printf("Step 3: Making a Move\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("On your turn, pick all seeds from one of your pits.\n");
                printf("Sow them one-by-one counterclockwise around the board.\n");
                printf("Skip the pit you picked from on subsequent laps.\n\n");

                board_t move_board;
                board_init(&move_board);
                printf("Example: Player A picks pit 0 (4 seeds):\n");
                ui_display_board_detailed(&move_board, "Player A", "Player B");

                // Simulate move from pit 0
                board_t after_move;
                int captured;
                rules_simulate_move(&move_board, PLAYER_A, 0, &after_move, &captured);
                printf("\nAfter sowing the 4 seeds:\n");
                ui_display_board_detailed(&after_move, "Player A", "Player B");
                printf("Seeds move counterclockwise, pit 0 is now empty.\n\n");
                break;
            }
            case 3: {  /* Capturing */
                printf("Step 4: Capturing Seeds\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("If your last seed lands in an opponent's pit with 2 or 3 seeds,\n");
                printf("you capture those seeds. Continue capturing backwards if possible.\n\n");

                // Set up a capture scenario
                board_t capture_board;
                board_init(&capture_board);
                // Manually set up capture position
                capture_board.pits[5] = 1;  // Player A pit 5
                capture_board.pits[6] = 2;  // Player B pit 6 (will be captured)
                capture_board.pits[7] = 3;  // Player B pit 7 (will be captured)
                capture_board.pits[8] = 1;  // Player B pit 8
                capture_board.current_player = PLAYER_A;

                printf("Example: Player A picks pit 5 (1 seed):\n");
                ui_display_board_detailed(&capture_board, "Player A", "Player B");

                board_t after_capture;
                int captured;
                rules_simulate_move(&capture_board, PLAYER_A, 5, &after_capture, &captured);
                printf("\nAfter move - last seed in pit 6 (2 seeds) → captured!\n");
                printf("Check backwards: pit 7 has 3 seeds → also captured!\n");
                ui_display_board_detailed(&after_capture, "Player A", "Player B");
                printf("Player A captured 5 seeds total.\n\n");
                break;
            }
            case 4: {  /* Feeding rule */
                printf("Step 5: The Feeding Rule\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("IMPORTANT: You cannot make a move that leaves your opponent\n");
                printf("with no seeds to play. If you have another move that feeds them,\n");
                printf("you must play that instead.\n\n");

                board_t starve_board;
                board_init(&starve_board);
                // Set up starvation scenario
                starve_board.pits[0] = 1; starve_board.pits[1] = 0; starve_board.pits[2] = 0;
                starve_board.pits[3] = 0; starve_board.pits[4] = 0; starve_board.pits[5] = 0;
                starve_board.pits[6] = 0; starve_board.pits[7] = 0; starve_board.pits[8] = 0;
                starve_board.pits[9] = 0; starve_board.pits[10] = 0; starve_board.pits[11] = 1;
                starve_board.current_player = PLAYER_A;

                printf("Example: Only pit 0 has seeds. Moving it would starve opponent:\n");
                ui_display_board_detailed(&starve_board, "Player A", "Player B");

                printf("This move is illegal! You must leave opponent with playable seeds.\n");
                printf("(In tournament play, this ends the game with seed collection.)\n\n");
                break;
            }
            case 5: {  /* Winning */
                printf("Step 6: Winning the Game\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("• First to 25+ captured seeds wins immediately.\n");
                printf("• If a player can't move and opponent can't feed them,\n");
                printf("  the game ends and remaining seeds are collected.\n");
                printf("• Winner is the one with more captured seeds.\n\n");
                break;
            }
            case 6: {  /* Interactive practice */
                printf("Step 7: Practice Move\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("Now let's practice! Here's a board position:\n");

                board_t practice_board;
                board_init(&practice_board);
                practice_board.pits[6] = 2; practice_board.pits[7] = 3; // Set up capture
                practice_board.current_player = PLAYER_A;
                ui_display_board_detailed(&practice_board, "Player A", "Player B");

                printf("Your turn (Player A). Enter pit number (0-5) to move: ");
                fflush(stdout);

                if (fgets(input, sizeof(input), stdin) == NULL) break;
                int pit = atoi(input);
                if (pit < 0 || pit > 5) {
                    printf("Invalid pit. Let's continue to next step.\n\n");
                    break;
                }

                board_t result;
                int captured;
                rules_simulate_move(&practice_board, PLAYER_A, pit, &result, &captured);
                printf("After your move:\n");
                ui_display_board_detailed(&result, "Player A", "Player B");
                if (captured > 0) {
                    printf("You captured %d seeds!\n", captured);
                }
                printf("\n");
                break;
            }
            case 7: {  /* More practice */
                printf("Step 8: Another Practice\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("Try this position. Can you capture?\n");

                board_t practice2_board;
                board_init(&practice2_board);
                practice2_board.pits[0] = 1; practice2_board.pits[1] = 2; practice2_board.pits[2] = 0;
                practice2_board.pits[3] = 0; practice2_board.pits[4] = 0; practice2_board.pits[5] = 0;
                practice2_board.pits[6] = 0; practice2_board.pits[7] = 0; practice2_board.pits[8] = 2;
                practice2_board.pits[9] = 3; practice2_board.pits[10] = 1; practice2_board.pits[11] = 0;
                practice2_board.current_player = PLAYER_A;
                ui_display_board_detailed(&practice2_board, "Player A", "Player B");

                printf("Your turn (Player A). Enter pit number (0-5): ");
                fflush(stdout);

                if (fgets(input, sizeof(input), stdin) == NULL) break;
                int pit = atoi(input);
                if (pit < 0 || pit > 5) {
                    printf("Invalid pit. Moving on...\n\n");
                    break;
                }

                board_t result;
                int captured;
                rules_simulate_move(&practice2_board, PLAYER_A, pit, &result, &captured);
                printf("Result:\n");
                ui_display_board_detailed(&result, "Player A", "Player B");
                if (captured > 0) {
                    printf("Captured %d seeds!\n", captured);
                } else {
                    printf("No capture this time.\n");
                }
                printf("\n");
                break;
            }
            case 8: {  /* Summary */
                printf("Step 9: Summary\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("Key points to remember:\n");
                printf("• Pick seeds from your pits (0-5 for Player A, 6-11 for Player B)\n");
                printf("• Sow counterclockwise, skip starting pit on laps\n");
                printf("• Capture 2-3 seeds in opponent's pits\n");
                printf("• Don't starve your opponent if you can avoid it\n");
                printf("• First to 25+ seeds wins, or most seeds when game ends\n\n");
                break;
            }
            case 9: {  /* End */
                printf("Tutorial Complete!\n");
                printf("═════════════════════════════════════════════════════════\n");
                printf("You now know the basics of Awale. Try playing a real game!\n");
                printf("Use option 2 in the main menu to challenge another player.\n\n");
                printf("Press Enter to return to main menu.");
                fflush(stdout);
                if (fgets(input, sizeof(input), stdin) == NULL) {}
                return;
            }
        }

        if (step < 9) {
            printf("Press Enter for next step, or 'q' to quit tutorial: ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin) == NULL) break;
            if (input[0] == 'q' || input[0] == 'Q') {
                printf("Tutorial exited.\n\n");
                return;
            }
        }
        step++;
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

    err = session_recv_message_timeout(session, &type, &list, sizeof(list), &size, 5000, NULL, 0);
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
    } else {
        for (int i = 0; i < list.count; i++) {
            game_info_t* game = &list.games[i];
            printf("%d. %s vs %s (%s)\n",
                   i + 1, game->player_a, game->player_b, game->game_id);
        }
    }

    printf("\nPress Enter to return to menu...");
    fflush(stdout);
    char dummy[2];
    read_line(dummy, 2);
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

    err = session_recv_message_timeout(session, &type, &list, sizeof(list), &size, 5000, NULL, 0);
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

    err = session_recv_message_timeout(session, &type, &board_state, sizeof(board_state), &size, 5000, NULL, 0);
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
    } else {
        /* Display the saved game board */
        printf("Saved Game: %s vs %s\n", board_state.player_a, board_state.player_b);
        ui_display_board_simple((const board_t*)&board_state.pits);  /* Cast to board_t for display */
    }

    printf("\nPress Enter to return to menu...");
    fflush(stdout);
    char dummy[2];
    read_line(dummy, 2);
}

/* Profile management submenu */
__attribute__((unused)) void cmd_profile(void)
{
    int current_selection = 1;
    while (client_state_is_running()) {
        ui_display_profile_menu_highlighted(current_selection);

        int choice = read_arrow_input(&current_selection, 1, 4);
        if (choice == -1) { /* ESC pressed */
            return; /* Back to main menu */
        } else if (choice >= 1 && choice <= 4) {
            /* Valid choice selected */
            current_selection = choice;
        } else {
            /* Continue loop for navigation */
            continue;
        }

        switch (current_selection) {
            case 1:  /* Set your bio */
                cmd_set_bio();
                break;

            case 2: {  /* View your bio */
                session_t* session = client_state_get_session();
                const char* my_pseudo = client_state_get_pseudo();

                msg_get_bio_t bio_req;
                memset(&bio_req, 0, sizeof(bio_req));
                snprintf(bio_req.target_player, MAX_PSEUDO_LEN, "%s", my_pseudo);

                error_code_t err = session_send_message(session, MSG_GET_BIO, &bio_req, sizeof(bio_req));
                if (err != SUCCESS) {
                    client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
                    break;
                }

                message_type_t type;
                msg_bio_response_t bio_response;
                size_t size;

                err = session_recv_message_timeout(session, &type, &bio_response, sizeof(bio_response), &size, 5000, NULL, 0);
                if (err == ERR_TIMEOUT) {
                    client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
                    break;
                }
                if (err != SUCCESS || type != MSG_BIO_RESPONSE) {
                    client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
                    break;
                }

                ui_display_bio(&bio_response);
                break;
            }

            case 3: {  /* View your stats */
                session_t* session = client_state_get_session();
                const char* my_pseudo = client_state_get_pseudo();

                msg_get_player_stats_t stats_req;
                memset(&stats_req, 0, sizeof(stats_req));
                snprintf(stats_req.target_player, MAX_PSEUDO_LEN, "%s", my_pseudo);

                error_code_t err = session_send_message(session, MSG_GET_PLAYER_STATS, &stats_req, sizeof(stats_req));
                if (err != SUCCESS) {
                    client_log_error(CLIENT_LOG_ERROR_SENDING_REQUEST, error_to_string(err));
                    break;
                }

                message_type_t type;
                msg_player_stats_t response;
                size_t size;

                err = session_recv_message_timeout(session, &type, &response, sizeof(response), &size, 5000, NULL, 0);
                if (err == ERR_TIMEOUT) {
                    client_log_error(CLIENT_LOG_TIMEOUT_SERVER);
                    break;
                }
                if (err != SUCCESS || type != MSG_PLAYER_STATS) {
                    client_log_error(CLIENT_LOG_ERROR_RECEIVING_RESPONSE);
                    break;
                }

                ui_display_player_stats(&response);
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
