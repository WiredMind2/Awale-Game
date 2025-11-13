/* Client UI - Display and User Interface Functions
 * Handles all screen output and user input operations
 */

#include "../../include/client/client_ui.h"
#include "../../include/client/client_state.h"
#include "../../include/client/client_logging.h"
#include "../../include/common/types.h"
#include "../../include/game/board.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void print_banner(void) {
    printf(CLIENT_UI_BANNER_LINE1);
    printf(CLIENT_UI_BANNER_LINE2);
    printf(CLIENT_UI_BANNER_LINE3);
    printf(CLIENT_UI_BANNER_LINE4);
    printf(CLIENT_UI_BANNER_LINE5);
}

void print_menu(void) {
    int pending = pending_challenges_count();
    int active = active_games_count();
    
    printf(CLIENT_UI_MENU_LINE1);
    printf(CLIENT_UI_MENU_LINE2);
    printf(CLIENT_UI_MENU_LINE3);
    printf(CLIENT_UI_MENU_LINE4);
    printf(CLIENT_UI_MENU_OPTION1);
    printf(CLIENT_UI_MENU_OPTION2);
    printf(CLIENT_UI_MENU_OPTION3);
    if (pending > 0) {
    printf(CLIENT_UI_MENU_PENDING, pending);
    }
    printf(CLIENT_UI_MENU_LINE5);
    printf(CLIENT_UI_MENU_OPTION4);
    printf(CLIENT_UI_MENU_OPTION5);
    printf(CLIENT_UI_MENU_OPTION6);
    printf(CLIENT_UI_MENU_OPTION7);
    printf(CLIENT_UI_MENU_OPTION8);
    if (active > 0) {
        printf(CLIENT_UI_MENU_ACTIVE, active, active > 1 ? "s" : "");
    }
    printf(CLIENT_UI_MENU_LINE6);
    printf(CLIENT_UI_MENU_OPTION9);
    printf(CLIENT_UI_MENU_OPTION10);
    printf(CLIENT_UI_MENU_LINE7);
    printf(CLIENT_UI_MENU_PROMPT);
}

void print_board(const msg_board_state_t* board) {
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
    printf(CLIENT_UI_STATS_FIRST_SEEN, ctime(&response->first_seen));
    printf(CLIENT_UI_STATS_LAST_SEEN, ctime(&response->last_seen));
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
    printf(CLIENT_UI_NOTIFICATION_CHALLENGE_HINT);
    printf(CLIENT_UI_NOTIFICATION_SEPARATOR);
    printf(CLIENT_UI_NOTIFICATION_PROMPT);
    fflush(stdout);
}

void ui_display_game_started(const msg_game_started_t* start) {
    printf(CLIENT_UI_GAME_STARTED_HEADER);
    printf(CLIENT_UI_GAME_STARTED_TITLE);
    printf(CLIENT_UI_GAME_STARTED_ID, start->game_id);
    printf(CLIENT_UI_GAME_STARTED_PLAYERS, start->player_a, start->player_b);
    printf(CLIENT_UI_GAME_STARTED_YOUR_SIDE, (start->your_side == PLAYER_A) ? "Player A" : "Player B");
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
    printf(CLIENT_UI_SPECTATOR_JOINED_PROMPT);
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
           player_b_name ? player_b_name : "Player B", 
           board->scores[1],
           (board->current_player == PLAYER_B) ? "← À TOI!" : "");
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
           (board->current_player == PLAYER_A) ? "À TOI! →" : "",
           player_a_name ? player_a_name : "Player A",
           board->scores[0]);
    printf(CLIENT_UI_BOARD_DETAILED_LINE7);
    
    if (board->state == GAME_STATE_FINISHED) {
        printf(CLIENT_UI_BOARD_DETAILED_GAME_OVER);
        if (board->winner == WINNER_A) {
            printf(CLIENT_UI_BOARD_DETAILED_WINNER_A, player_a_name ? player_a_name : "Player A");
        } else if (board->winner == WINNER_B) {
            printf(CLIENT_UI_BOARD_DETAILED_WINNER_B, player_b_name ? player_b_name : "Player B");
        } else {
            printf(CLIENT_UI_BOARD_DETAILED_DRAW);
        }
    } else {
        printf(CLIENT_UI_BOARD_DETAILED_CURRENT_TURN, 
               (board->current_player == PLAYER_A) ? 
               (player_a_name ? player_a_name : "Player A") : 
               (player_b_name ? player_b_name : "Player B"));
    }
    printf(CLIENT_UI_BOARD_DETAILED_LINE8);
    printf(CLIENT_UI_BOARD_DETAILED_LINE9);
}
