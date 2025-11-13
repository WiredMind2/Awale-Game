/* Client Basic Commands - Simple command implementations
 * List players, challenge, view challenges
 */

#ifndef CLIENT_BASIC_COMMANDS_H
#define CLIENT_BASIC_COMMANDS_H

void cmd_list_players(void);
void cmd_challenge_player(void);
void cmd_view_challenges(void);
void cmd_set_bio(void);
void cmd_view_bio(void);
void cmd_view_player_stats(void);
void cmd_chat(void);
void cmd_friend_management(void);
void cmd_list_saved_games(void);
void cmd_view_saved_game(void);

#endif /* CLIENT_BASIC_COMMANDS_H */
