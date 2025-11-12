/* Client UI - Display and User Interface Functions
 * Handles all screen output and user input operations
 */

#include "../../include/client/client_ui.h"
#include "../../include/client/client_state.h"
#include <stdio.h>
#include <string.h>

void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║            AWALE GAME - CLI Client                   ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void print_menu(void) {
    int pending = pending_challenges_count();
    int active = active_games_count();
    
    printf("\n");
    printf("═════════════════════════════════════════════════════════\n");
    printf("                    MAIN MENU                            \n");
    printf("═════════════════════════════════════════════════════════\n");
    printf("  1. List connected players\n");
    printf("  2. Challenge a player\n");
    printf("  3. View/Respond to challenges");
    if (pending > 0) {
    printf(" [%d pending]", pending);
    }
    printf("\n");
    printf("  4. Set your bio\n");
    printf("  5. View player bio\n");
    printf("  6. View player statistics\n");
    printf("  7. Quit\n");
    printf("  8. Play mode");
    if (active > 0) {
        printf(" [%d active game%s]", active, active > 1 ? "s" : "");
    }
    printf("\n");
    printf("  9. Chat\n");
    printf("  10. Spectator mode\n");
    printf("═════════════════════════════════════════════════════════\n");
    printf("Your choice: ");
}

void print_board(const msg_board_state_t* board) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("                    PLATEAU AWALE                          \n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Joueur B: %s (Score: %d)                    %s\n", 
           board->player_b, board->score_b,
           (board->current_player == PLAYER_B) ? "← À TOI!" : "");
    printf("\n");
    
    printf("   ┌────┬────┬────┬────┬────┬────┐\n");
    printf("   │%2d  │%2d  │%2d  │%2d  │%2d  │%2d  │\n",
           board->pits[11], board->pits[10], board->pits[9], 
           board->pits[8], board->pits[7], board->pits[6]);
    printf("   │ 11 │ 10 │ 9  │ 8  │ 7  │ 6  │\n");
    printf("   ├────┼────┼────┼────┼────┼────┤\n");
    printf("   │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │\n");
    printf("   │%2d  │%2d  │%2d  │%2d  │%2d  │%2d  │\n",
           board->pits[0], board->pits[1], board->pits[2], 
           board->pits[3], board->pits[4], board->pits[5]);
    printf("   └────┴────┴────┴────┴────┴────┘\n");
    printf("\n");
    printf("%s Joueur A: %s (Score: %d)\n", 
           (board->current_player == PLAYER_A) ? "À TOI! →" : "",
           board->player_a, board->score_a);
    printf("═══════════════════════════════════════════════════════════\n");
    
    if (board->state == GAME_STATE_FINISHED) {
    printf("PARTIE TERMINEE - ");
        if (board->winner == WINNER_A) {
            printf("%s gagne!\n", board->player_a);
        } else if (board->winner == WINNER_B) {
            printf("%s gagne!\n", board->player_b);
        } else {
            printf("Match nul!\n");
        }
    } else {
        printf("Tour du joueur: %s\n", 
               (board->current_player == PLAYER_A) ? board->player_a : board->player_b);
        
        if (board->current_player == PLAYER_A) {
            printf("%s peut jouer les fosses 0 à 5 (rangée du bas)\n", board->player_a);
        } else {
            printf("%s peut jouer les fosses 6 à 11 (rangée du haut)\n", board->player_b);
        }
    }
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
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
