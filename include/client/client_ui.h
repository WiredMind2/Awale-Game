/* Client UI - Display and User Interface Functions
 * Handles all screen output and user input operations
 */

#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include "../common/messages.h"
#include <stddef.h>

/* Banner and menu functions */
void print_banner(void);
void print_menu(void);

/* Board display */
void print_board(const msg_board_state_t* board);

/* Input utilities */
void clear_input(void);
char* read_line(char* buffer, size_t size);

#endif /* CLIENT_UI_H */
