/* Client UI - Display and User Interface Functions
 * Handles all screen output and user input operations
 */

#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include "../common/messages.h"
#include "../game/board.h"
#include <stddef.h>

/* Banner and menu functions */
void print_banner(void);
void print_menu(void);

/* Board display */
void print_board(const msg_board_state_t* board);
void ui_display_board_simple(const board_t* board);
void ui_display_board_detailed(const board_t* board, const char* player_a_name, const char* player_b_name);

/* Input utilities */
void clear_input(void);
char* read_line(char* buffer, size_t size);

/* New UI display functions */
void ui_display_player_list(const msg_player_list_t* list);
void ui_display_challenge_sent(const char* opponent);
void ui_display_challenge_error(const char* error_msg);
void ui_display_pending_challenges(int count);
void ui_display_challenge_response(const char* challenger, bool accepted);
void ui_prompt_bio(msg_set_bio_t* bio_msg);
void ui_display_bio_updated(int lines);
void ui_display_bio(const msg_bio_response_t* response);
void ui_display_player_stats(const msg_player_stats_t* response);
void ui_display_chat_mode(const char* recipient);
void ui_display_chat_error(const char* error);
void ui_display_challenge_received(const msg_challenge_received_t* notif);
void ui_display_game_started(const msg_game_started_t* start);
void ui_display_spectator_joined(const msg_spectator_joined_t* notif);
void ui_display_game_over(const msg_game_over_t* game_over);
void ui_display_chat_message(const msg_chat_message_t* chat);
void ui_display_connection_lost(void);
void ui_display_network_error(const char* error, int attempt, int max);

/* Play mode UI */
void ui_display_turn_info(int is_your_turn, const int* legal_moves, int count);
void ui_display_waiting_for_opponent(void);
void ui_display_play_error(const char* error);

#endif /* CLIENT_UI_H */
