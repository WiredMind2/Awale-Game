/* Client UI - Display and User Interface Functions
 * Handles all screen output and user input operations
 */

#include "../../include/client/client_ui.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_logging.h"
#include "../../include/client/client_notifications.h"
#include "../../include/client/ansi_colors.h"
#include "../../include/common/types.h"
#include "../../include/game/board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "client/client_ui_strings.h"
void print_banner(void) {
    /* Banner in cyan for visibility */
    printf(ANSI_COLOR_BRIGHT_CYAN);
    printf(CLIENT_UI_BANNER_LINE1);
    printf(CLIENT_UI_BANNER_LINE2);
    printf(CLIENT_UI_BANNER_LINE3);
    printf(CLIENT_UI_BANNER_LINE4);
    printf(CLIENT_UI_BANNER_LINE5);
    printf(ANSI_COLOR_RESET);
}

void print_menu(void) {
    system("clear");
    int pending = pending_challenges_count();
    int active = active_games_count();
    /* Menu header in bright cyan */
    printf(ANSI_COLOR_BRIGHT_CYAN);
    printf(CLIENT_UI_MENU_LINE1);
    printf(CLIENT_UI_MENU_LINE2);
    printf(CLIENT_UI_MENU_LINE3);
    printf(CLIENT_UI_MENU_LINE4);
    printf("%-32s%-24s\n", MENU_OPTION_1, MENU_OPTION_7);
    printf("%-32s%-24s", MENU_OPTION_2, MENU_OPTION_8);
    if (active > 0) {
        printf(UI_ACTIVE_GAMES, active, active > 1 ? "s" : "");
    }
    printf("\n");
    printf("%-32s", MENU_OPTION_3);
    if (pending > 0) {
        printf(UI_PENDING_CHALLENGES, pending);
    }
    printf("%-24s\n", MENU_OPTION_9);
    printf("%-32s%-24s\n", MENU_OPTION_4, MENU_OPTION_10);
    printf("%-32s%-24s\n", MENU_OPTION_5, MENU_OPTION_11);
    printf("%-32s%-24s\n", MENU_OPTION_6, MENU_OPTION_12);
    printf("%-32s\n", MENU_OPTION_13);
    printf(CLIENT_UI_MENU_LINE7);
    /* Reset color before prompt so user input isn't colored */
    printf(ANSI_COLOR_RESET);
    printf(CLIENT_UI_MENU_PROMPT);
}

void print_menu_highlighted(int selected_option) {
    system("clear");
    int pending = pending_challenges_count();
    int active = active_games_count();

    printf(CLIENT_UI_MENU_LINE1);
    printf(CLIENT_UI_MENU_LINE2);
    printf(CLIENT_UI_MENU_LINE3);
    printf(CLIENT_UI_MENU_LINE4);

    printf("%s%-32s%s%s%-24s%s\n",
            selected_option == 1 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_1,
            selected_option == 1 ? ANSI_COLOR_RESET : "",
            selected_option == 7 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_7,
            selected_option == 7 ? ANSI_COLOR_RESET : "");

    printf("%s%-32s%s%s%-24s%s",
            selected_option == 2 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_2,
            selected_option == 2 ? ANSI_COLOR_RESET : "",
            selected_option == 8 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_8,
            selected_option == 8 ? ANSI_COLOR_RESET : "");
    if (active > 0) {
        printf(UI_ACTIVE_GAMES, active, active > 1 ? "s" : "");
    }
    printf("\n");

    printf("%s%-32s%s",
            selected_option == 3 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_3,
            selected_option == 3 ? ANSI_COLOR_RESET : "");
    if (pending > 0) {
        printf(UI_PENDING_CHALLENGES, pending);
    }
    printf("%s%-24s%s\n",
            selected_option == 9 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_9,
            selected_option == 9 ? ANSI_COLOR_RESET : "");

    printf("%s%-32s%s%s%-24s%s\n",
            selected_option == 4 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_4,
            selected_option == 4 ? ANSI_COLOR_RESET : "",
            selected_option == 10 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_10,
            selected_option == 10 ? ANSI_COLOR_RESET : "");

    printf("%s%-32s%s%s%-24s%s\n",
            selected_option == 5 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_5,
            selected_option == 5 ? ANSI_COLOR_RESET : "",
            selected_option == 11 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_11,
            selected_option == 11 ? ANSI_COLOR_RESET : "");

    printf("%s%-32s%s%s%-24s%s\n",
            selected_option == 6 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_6,
            selected_option == 6 ? ANSI_COLOR_RESET : "",
            selected_option == 12 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_12,
            selected_option == 12 ? ANSI_COLOR_RESET : "");

    printf("%s%-32s%s\n",
            selected_option == 13 ? ANSI_COLOR_GREEN ANSI_COLOR_BOLD : "",
            MENU_OPTION_13,
            selected_option == 13 ? ANSI_COLOR_RESET : "");

    printf(CLIENT_UI_MENU_LINE7);
    printf(UI_NAVIGATION_PROMPT);
    fflush(stdout);
}

void print_board(const msg_board_state_t* board) {
    /* Color the board display subtly to improve readability */
    printf(ANSI_COLOR_BRIGHT_MAGENTA);
    printf(CLIENT_UI_BOARD_LINE1);
    printf(CLIENT_UI_BOARD_LINE2);
    printf(CLIENT_UI_BOARD_LINE3);
    printf(CLIENT_UI_BOARD_LINE4);
    printf(CLIENT_UI_BOARD_PLAYER_B, 
           board->player_b, board->score_b,
           (board->current_player == PLAYER_B) ? "← À TOI!" : "");
    printf(CLIENT_UI_BOARD_LINE5);
    
    printf(CLIENT_UI_BOARD_TOP_ROW);
    printf(CLIENT_UI_BOARD_TOP_PITS,
           board->pits[11], board->pits[10], board->pits[9], 
           board->pits[8], board->pits[7], board->pits[6]);
    printf(CLIENT_UI_BOARD_TOP_LABELS);
    printf(CLIENT_UI_BOARD_MIDDLE);
    printf(CLIENT_UI_BOARD_BOTTOM_LABELS);
    printf(CLIENT_UI_BOARD_BOTTOM_PITS,
           board->pits[0], board->pits[1], board->pits[2], 
           board->pits[3], board->pits[4], board->pits[5]);
    printf(CLIENT_UI_BOARD_BOTTOM_ROW);
    printf(CLIENT_UI_BOARD_LINE6);
    printf(CLIENT_UI_BOARD_PLAYER_A, 
           (board->current_player == PLAYER_A) ? "À TOI! →" : "",
           board->player_a, board->score_a);
    printf(CLIENT_UI_BOARD_LINE7);
    
    if (board->state == GAME_STATE_FINISHED) {
    printf(CLIENT_UI_BOARD_GAME_OVER);
        if (board->winner == WINNER_A) {
            printf(CLIENT_UI_BOARD_WINNER_A, board->player_a);
        } else if (board->winner == WINNER_B) {
            printf(CLIENT_UI_BOARD_WINNER_B, board->player_b);
        } else {
            printf(CLIENT_UI_BOARD_DRAW);
        }
    } else {
        printf(CLIENT_UI_BOARD_CURRENT_TURN, 
               (board->current_player == PLAYER_A) ? board->player_a : board->player_b);
        
        if (board->current_player == PLAYER_A) {
            printf(CLIENT_UI_BOARD_PLAYER_A_HINT, board->player_a);
        } else {
            printf(CLIENT_UI_BOARD_PLAYER_B_HINT, board->player_b);
        }
    }
    printf(CLIENT_UI_BOARD_LINE8);
    printf(CLIENT_UI_BOARD_LINE9);
    printf(ANSI_COLOR_RESET);
}

void clear_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

char* read_line(char* buffer, size_t size) {
    if (fgets(buffer, size, stdin) == NULL) {
        return NULL;
    }
    
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    return buffer;
}

/* New UI display implementations */

void ui_display_player_list(const msg_player_list_t* list) {
    printf(CLIENT_UI_PLAYER_LIST_HEADER, list->count);
    printf(CLIENT_UI_PLAYER_LIST_SEPARATOR);
    for (int i = 0; i < list->count; i++) {
        printf(CLIENT_UI_PLAYER_LIST_ITEM, i + 1, list->players[i].pseudo, list->players[i].ip);
    }
    printf(CLIENT_UI_PLAYER_LIST_FOOTER);
}

void ui_display_challenge_sent(const char* opponent) {
    printf(CLIENT_UI_CHALLENGE_SENT, opponent);
    printf(CLIENT_UI_CHALLENGE_SENT_INFO);
}

void ui_display_challenge_error(const char* error_msg) {
    printf(CLIENT_UI_CHALLENGE_ERROR, error_msg);
}

void ui_display_pending_challenges(int count) {
    system("clear");
    if (count == 0) {
        printf(CLIENT_UI_NO_CHALLENGES);
        return;
    }

    printf(CLIENT_UI_CHALLENGES_HEADER, count);
    printf(CLIENT_UI_CHALLENGES_SEPARATOR);

    /* Display challenges */
    for (int i = 0; i < count; i++) {
        pending_challenge_t* challenge = pending_challenges_get(i);
        if (challenge) {
            printf(CLIENT_UI_CHALLENGE_ITEM, i + 1, challenge->challenger);
        }
    }

    printf(CLIENT_UI_CHALLENGES_FOOTER);
    printf(CLIENT_UI_CHALLENGES_OPTIONS);
    printf(CLIENT_UI_CHALLENGES_ACCEPT);
    printf(CLIENT_UI_CHALLENGES_DECLINE);
    printf(CLIENT_UI_CHALLENGES_CANCEL);
    printf(CLIENT_UI_CHALLENGES_PROMPT);
}

void ui_display_challenge_response(const char* challenger, bool accepted) {
    if (accepted) {
        printf(CLIENT_UI_CHALLENGE_ACCEPTED, challenger);
    } else {
        printf(CLIENT_UI_CHALLENGE_DECLINED, challenger);
    }
}

void ui_prompt_bio(msg_set_bio_t* bio_msg) {
    printf(CLIENT_UI_BIO_PROMPT_HEADER);
    printf(CLIENT_UI_BIO_PROMPT_INSTRUCTIONS);

    char line[256];
    int line_count = 0;

    while (line_count < 10) {
        printf(CLIENT_UI_BIO_PROMPT_LINE, line_count + 1);
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
        snprintf(bio_msg->bio[line_count], 256, "%s", line);
        bio_msg->bio[line_count][255] = '\0';
        line_count++;
    }

    bio_msg->bio_lines = line_count;
}

void ui_display_bio_updated(int lines) {
    printf(CLIENT_UI_BIO_UPDATED, lines);
}

void ui_display_bio(const msg_bio_response_t* response) {
    if (!response->success) {
        printf(CLIENT_UI_BIO_DISPLAY_ERROR, response->message);
        return;
    }

    printf(CLIENT_UI_BIO_DISPLAY_HEADER, response->player);
    printf(CLIENT_UI_BIO_DISPLAY_SEPARATOR);

    if (response->bio_lines == 0) {
        printf(CLIENT_UI_BIO_DISPLAY_EMPTY);
    } else {
        for (int i = 0; i < response->bio_lines; i++) {
            printf(CLIENT_UI_BIO_DISPLAY_LINE, response->bio[i]);
        }
    }
    printf(CLIENT_UI_BIO_DISPLAY_FOOTER);
}

void ui_display_player_stats(const msg_player_stats_t* response) {
    if (!response->success) {
        printf(CLIENT_UI_STATS_DISPLAY_ERROR, response->message);
        return;
    }

    printf(CLIENT_UI_STATS_HEADER, response->player);
    printf(CLIENT_UI_STATS_SEPARATOR);
    printf(CLIENT_UI_STATS_GAMES_PLAYED, response->games_played);
    printf(CLIENT_UI_STATS_GAMES_WON, response->games_won);
    printf(CLIENT_UI_STATS_GAMES_LOST, response->games_lost);
    printf(CLIENT_UI_STATS_TOTAL_SCORE, response->total_score);
    printf(CLIENT_UI_STATS_WIN_RATE,
           response->games_played > 0 ?
           (float)response->games_won / response->games_played * 100 : 0);
    printf(CLIENT_UI_STATS_FOOTER);
}

void ui_display_chat_mode(const char* recipient) {
    printf(CLIENT_UI_CHAT_MODE_HEADER);
    printf(CLIENT_UI_CHAT_MODE_INSTRUCTIONS);
    printf(CLIENT_UI_CHAT_MODE_RECIPIENT);
    // Note: input handling is in the caller
    if (strlen(recipient) == 0) {
        printf(CLIENT_UI_CHAT_MODE_GLOBAL);
    } else {
        printf(CLIENT_UI_CHAT_MODE_PRIVATE, recipient);
    }
}

void ui_display_chat_error(const char* error) {
    printf(CLIENT_UI_CHAT_ERROR, error);
}

void ui_display_challenge_received(const msg_challenge_received_t* notif) {
    printf(CLIENT_UI_NOTIFICATION_HEADER);
    printf(CLIENT_UI_NOTIFICATION_CHALLENGE);
    printf(CLIENT_UI_NOTIFICATION_MESSAGE, notif->message);
    printf(CLIENT_UI_NOTIFICATION_SEPARATOR);
    printf(UI_CHALLENGE_HINT);
    printf(CLIENT_UI_NOTIFICATION_SEPARATOR);
    fflush(stdout);
}

void ui_display_game_started(const msg_game_started_t* start) {
    printf(CLIENT_UI_GAME_STARTED_HEADER);
    printf(CLIENT_UI_GAME_STARTED_TITLE);
    printf(CLIENT_UI_GAME_STARTED_ID, start->game_id);
    printf(CLIENT_UI_GAME_STARTED_PLAYERS, start->player_a, start->player_b);
    printf(CLIENT_UI_GAME_STARTED_YOUR_SIDE, (start->your_side == PLAYER_A) ? BOARD_PLAYER_A : BOARD_PLAYER_B);
    printf(CLIENT_UI_GAME_STARTED_HINT);
    printf(CLIENT_UI_GAME_STARTED_SEPARATOR);
    printf(CLIENT_UI_GAME_STARTED_PROMPT);
    fflush(stdout);
}

void ui_display_spectator_joined(const msg_spectator_joined_t* notif) {
    printf(CLIENT_UI_SPECTATOR_JOINED_HEADER);
    printf(CLIENT_UI_SPECTATOR_JOINED_TITLE, notif->spectator);
    printf(CLIENT_UI_SPECTATOR_JOINED_ID, notif->game_id);
    printf(CLIENT_UI_SPECTATOR_JOINED_COUNT, notif->spectator_count);
    printf(CLIENT_UI_SPECTATOR_JOINED_SEPARATOR);
    fflush(stdout);
}

void ui_display_game_over(const msg_game_over_t* game_over) {
    printf(CLIENT_UI_GAME_OVER_HEADER);
    printf(CLIENT_UI_GAME_OVER_TITLE);
    printf(CLIENT_UI_GAME_OVER_MESSAGE, game_over->message);
    printf(CLIENT_UI_GAME_OVER_SEPARATOR);
    printf(CLIENT_UI_GAME_OVER_PROMPT);
    fflush(stdout);
}

void ui_display_chat_message(const msg_chat_message_t* chat) {
    printf(CLIENT_UI_CHAT_MESSAGE_HEADER);
    if (strlen(chat->recipient) == 0) {
        printf(CLIENT_UI_CHAT_MESSAGE_GLOBAL, chat->sender);
    } else {
        printf(CLIENT_UI_CHAT_MESSAGE_PRIVATE, chat->sender);
    }
    printf(CLIENT_UI_CHAT_MESSAGE_TEXT, chat->message);
    printf(CLIENT_UI_CHAT_MESSAGE_SEPARATOR);
    printf(CLIENT_UI_CHAT_MESSAGE_PROMPT);
    fflush(stdout);
}

void ui_display_connection_lost(void) {
    printf(CLIENT_UI_CONNECTION_LOST);
}

void ui_display_network_error(const char* error, int attempt, int max) {
    printf(CLIENT_UI_NETWORK_ERROR, error, attempt, max);
    if (attempt >= max) {
        printf(CLIENT_UI_NETWORK_ERROR_MAX);
    }
}

/* Play mode UI implementations */

void ui_display_turn_info(int is_your_turn, const int* legal_moves, int count) {
    if (is_your_turn) {
        printf(CLIENT_UI_TURN_YOUR_TURN);
        printf(CLIENT_UI_TURN_LEGAL_MOVES);
        if (count > 0) {
            for (int i = 0; i < count; i++) {
                printf(CLIENT_UI_TURN_MOVE_SEPARATOR, legal_moves[i]);
            }
        } else {
            printf(CLIENT_UI_TURN_NO_MOVES);
        }
        printf(CLIENT_UI_TURN_LINE1);
        printf(CLIENT_UI_TURN_PROMPT);
    } else {
        ui_display_waiting_for_opponent();
    }
}

void ui_display_waiting_for_opponent(void) {
    printf(CLIENT_UI_WAITING_OPPONENT);
    printf(CLIENT_UI_WAITING_HINT);
    printf(CLIENT_UI_WAITING_PROMPT);
}

void ui_display_play_error(const char* error) {
    printf(CLIENT_UI_PLAY_ERROR, error);
}

void ui_display_board_simple(const board_t* board) {
    if (!board) return;
    
    printf(CLIENT_UI_BOARD_SIMPLE_LINE1);
    printf(CLIENT_UI_BOARD_SIMPLE_PLAYER_B,
           board->pits[11], board->pits[10], board->pits[9],
           board->pits[8], board->pits[7], board->pits[6],
           board->scores[1]);
    printf(CLIENT_UI_BOARD_SIMPLE_PLAYER_A,
           board->pits[0], board->pits[1], board->pits[2],
           board->pits[3], board->pits[4], board->pits[5],
           board->scores[0]);
    printf(CLIENT_UI_BOARD_SIMPLE_TURN, (board->current_player == PLAYER_A) ? 'A' : 'B');
    printf(CLIENT_UI_BOARD_SIMPLE_LINE2);
}

void ui_display_board_detailed(const board_t* board, const char* player_a_name, const char* player_b_name) {
    if (!board) return;

    printf(CLIENT_UI_BOARD_DETAILED_LINE1);
    printf(CLIENT_UI_BOARD_DETAILED_LINE2);
    printf(CLIENT_UI_BOARD_DETAILED_LINE3);
    printf(CLIENT_UI_BOARD_DETAILED_LINE4);
    printf(CLIENT_UI_BOARD_DETAILED_PLAYER_B,
            player_b_name ? player_b_name : BOARD_PLAYER_B,
            board->scores[1],
            (board->current_player == PLAYER_B) ? BOARD_ARROW_LEFT : "");
    printf(CLIENT_UI_BOARD_DETAILED_LINE5);

    printf(CLIENT_UI_BOARD_DETAILED_TOP_ROW);
    printf(CLIENT_UI_BOARD_DETAILED_TOP_PITS,
           board->pits[11], board->pits[10], board->pits[9],
           board->pits[8], board->pits[7], board->pits[6]);
    printf(CLIENT_UI_BOARD_DETAILED_TOP_LABELS);
    printf(CLIENT_UI_BOARD_DETAILED_MIDDLE);
    printf(CLIENT_UI_BOARD_DETAILED_BOTTOM_LABELS);
    printf(CLIENT_UI_BOARD_DETAILED_BOTTOM_PITS,
           board->pits[0], board->pits[1], board->pits[2],
           board->pits[3], board->pits[4], board->pits[5]);
    printf(CLIENT_UI_BOARD_DETAILED_BOTTOM_ROW);
    printf(CLIENT_UI_BOARD_DETAILED_LINE6);
    printf(CLIENT_UI_BOARD_DETAILED_PLAYER_A,
            (board->current_player == PLAYER_A) ? BOARD_ARROW_RIGHT : "",
            player_a_name ? player_a_name : BOARD_PLAYER_A,
            board->scores[0]);
    printf(CLIENT_UI_BOARD_DETAILED_LINE7);

    if (board->state == GAME_STATE_FINISHED) {
        printf(CLIENT_UI_BOARD_DETAILED_GAME_OVER);
        if (board->winner == WINNER_A) {
            printf(CLIENT_UI_BOARD_DETAILED_WINNER_A, player_a_name ? player_a_name : BOARD_PLAYER_A);
        } else if (board->winner == WINNER_B) {
            printf(CLIENT_UI_BOARD_DETAILED_WINNER_B, player_b_name ? player_b_name : BOARD_PLAYER_B);
        } else {
            printf(CLIENT_UI_BOARD_DETAILED_DRAW);
        }
    } else {
        printf(CLIENT_UI_BOARD_DETAILED_CURRENT_TURN,
                (board->current_player == PLAYER_A) ?
                (player_a_name ? player_a_name : BOARD_PLAYER_A) :
                (player_b_name ? player_b_name : BOARD_PLAYER_B));
    }
    printf(CLIENT_UI_BOARD_DETAILED_LINE8);
    printf(CLIENT_UI_BOARD_DETAILED_LINE9);
}

/* Profile management UI functions */
void ui_display_profile_menu(void) {
    system("clear");
    printf(CLIENT_UI_PROFILE_MENU_HEADER);
    printf(CLIENT_UI_PROFILE_MENU_SEPARATOR);
    printf(CLIENT_UI_PROFILE_MENU_OPTION1);
    printf(CLIENT_UI_PROFILE_MENU_OPTION2);
    printf(CLIENT_UI_PROFILE_MENU_OPTION3);
    printf(CLIENT_UI_PROFILE_MENU_OPTION4);
    printf(CLIENT_UI_PROFILE_MENU_SEPARATOR);
    printf(CLIENT_UI_PROFILE_MENU_PROMPT);
}

void ui_display_profile_menu_highlighted(int selected_option) {
    system("clear");
    printf(CLIENT_UI_PROFILE_MENU_HEADER);
    printf(CLIENT_UI_PROFILE_MENU_SEPARATOR);

    /* Option 1 */
    if (selected_option == 1) printf(ANSI_COLOR_GREEN ANSI_COLOR_BOLD);
    printf(CLIENT_UI_PROFILE_MENU_OPTION1);
    if (selected_option == 1) printf(ANSI_COLOR_RESET);

    /* Option 2 */
    if (selected_option == 2) printf(ANSI_COLOR_GREEN ANSI_COLOR_BOLD);
    printf(CLIENT_UI_PROFILE_MENU_OPTION2);
    if (selected_option == 2) printf(ANSI_COLOR_RESET);

    /* Option 3 */
    if (selected_option == 3) printf(ANSI_COLOR_GREEN ANSI_COLOR_BOLD);
    printf(CLIENT_UI_PROFILE_MENU_OPTION3);
    if (selected_option == 3) printf(ANSI_COLOR_RESET);

    /* Option 4 */
    if (selected_option == 4) printf(ANSI_COLOR_GREEN ANSI_COLOR_BOLD);
    printf(CLIENT_UI_PROFILE_MENU_OPTION4);
    if (selected_option == 4) printf(ANSI_COLOR_RESET);

    printf(CLIENT_UI_PROFILE_MENU_SEPARATOR);
    printf(UI_NAVIGATION_PROMPT);
    fflush(stdout);
}

/* Friend management UI functions */
void ui_display_friend_menu(void) {
    system("clear");
    printf(CLIENT_UI_FRIEND_MENU_HEADER);
    printf(CLIENT_UI_FRIEND_MENU_SEPARATOR);
    printf(CLIENT_UI_FRIEND_MENU_OPTION1);
    printf(CLIENT_UI_FRIEND_MENU_OPTION2);
    printf(CLIENT_UI_FRIEND_MENU_OPTION3);
    printf(CLIENT_UI_FRIEND_MENU_OPTION4);
    printf(CLIENT_UI_FRIEND_MENU_SEPARATOR);
    printf(CLIENT_UI_FRIEND_MENU_PROMPT);
}

void ui_display_friend_menu_highlighted(int selected_option) {
    system("clear");
    printf(CLIENT_UI_FRIEND_MENU_HEADER);
    printf(CLIENT_UI_FRIEND_MENU_SEPARATOR);

    /* Option 1 */
    if (selected_option == 1) printf(ANSI_COLOR_GREEN ANSI_COLOR_BOLD);
    printf(CLIENT_UI_FRIEND_MENU_OPTION1);
    if (selected_option == 1) printf(ANSI_COLOR_RESET);

    /* Option 2 */
    if (selected_option == 2) printf(ANSI_COLOR_GREEN ANSI_COLOR_BOLD);
    printf(CLIENT_UI_FRIEND_MENU_OPTION2);
    if (selected_option == 2) printf(ANSI_COLOR_RESET);

    /* Option 3 */
    if (selected_option == 3) printf(ANSI_COLOR_GREEN ANSI_COLOR_BOLD);
    printf(CLIENT_UI_FRIEND_MENU_OPTION3);
    if (selected_option == 3) printf(ANSI_COLOR_RESET);

    /* Option 4 */
    if (selected_option == 4) printf(ANSI_COLOR_GREEN ANSI_COLOR_BOLD);
    printf(CLIENT_UI_FRIEND_MENU_OPTION4);
    if (selected_option == 4) printf(ANSI_COLOR_RESET);

    printf(CLIENT_UI_FRIEND_MENU_SEPARATOR);
    printf(UI_NAVIGATION_PROMPT);
    fflush(stdout);
}

void ui_display_friend_list(const msg_list_friends_t* friends) {
    printf(CLIENT_UI_FRIEND_LIST_HEADER, friends->count);
    printf(CLIENT_UI_FRIEND_LIST_SEPARATOR);

    if (friends->count == 0) {
        printf(CLIENT_UI_FRIEND_LIST_EMPTY);
    } else {
        for (int i = 0; i < friends->count; i++) {
            printf(CLIENT_UI_FRIEND_LIST_ITEM, i + 1, friends->friends[i]);
        }
    }
    printf(CLIENT_UI_FRIEND_LIST_FOOTER);
}

/* Input handling functions for arrow navigation */

int getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

int read_arrow_input(int* current_selection, int min_option, int max_option) {
    int ch = getch();

    if (ch == 27) { /* ESC - check for arrow keys */
        /* Set non-blocking to check for more input */
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

        /* Brief pause for arrow key sequence (non-blocking read should handle this) */

        int ch2 = getchar();
        if (ch2 != EOF && ch2 == 91) { /* [ */
            int ch3 = getchar();
            if (ch3 == 65 && *current_selection > min_option) { /* Up arrow */
                (*current_selection)--;
            } else if (ch3 == 66 && *current_selection < max_option) { /* Down arrow */
                (*current_selection)++;
            }
            /* Consume any remaining chars */
            while (getchar() != EOF);
        } else {
            /* Not an arrow key, put back the char if any */
            if (ch2 != EOF) ungetc(ch2, stdin);
            /* Restore terminal and return ESC */
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            fcntl(STDIN_FILENO, F_SETFL, oldf);
            return -1;
        }

        /* Restore terminal */
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);

    } else if (ch == 10 || ch == 13) { /* Enter */
        return *current_selection;
    } else if (ch >= '0' && ch <= '9') { /* Number input */
        int num = ch - '0';
        if (num >= min_option && num <= max_option) {
            *current_selection = num;
            return num;
        }
    }

    return 0; /* Continue */
}
