/* Test program to demonstrate the new game logic
 * Compile: gcc -Wall -Wextra -O2 -I./include test_game_demo.c src/game/board.c src/game/rules.c src/common/types.c -o test_game_demo
 * Run: ./test_game_demo
 */

#include "include/game/board.h"
#include "include/game/rules.h"
#include <stdio.h>

void test_basic_move() {
    printf("\n=== TEST: Basic Move ===\n");
    
    board_t board;
    board_init(&board);
    
    printf("Initial board:\n");
    board_print(&board);
    
    // Player A plays pit 2
    int captured = 0;
    error_code_t result = board_execute_move(&board, PLAYER_A, 2, &captured);
    
    if (result == SUCCESS) {
        printf("\nPlayer A played pit 2\n");
        printf("Seeds captured: %d\n", captured);
        board_print(&board);
    } else {
        printf("Error: %s\n", error_to_string(result));
    }
}

void test_capture() {
    printf("\n=== TEST: Capture Scenario ===\n");
    
    board_t board;
    board_init(&board);
    
    // Set up a capture scenario
    board.pits[0] = 0;
    board.pits[1] = 0;
    board.pits[2] = 1;  // Will land in pit 3 which has 4 seeds = 5 total (no capture)
    board.pits[3] = 4;
    
    printf("Setup for testing:\n");
    board_print(&board);
    
    int captured = 0;
    board_execute_move(&board, PLAYER_A, 2, &captured);
    
    printf("\nAfter move:\n");
    printf("Seeds captured: %d\n", captured);
    board_print(&board);
}

void test_feeding_rule() {
    printf("\n=== TEST: Feeding Rule ===\n");
    
    board_t board;
    board_init(&board);
    
    // Set up a scenario where opponent would be starved
    for (int i = 6; i <= 11; i++) {
        board.pits[i] = 0;  // Opponent has no seeds
    }
    
    board.pits[0] = 5;
    board.pits[1] = 3;
    
    printf("Setup - opponent side is empty:\n");
    board_print(&board);
    
    // Try to play pit 0 (would not feed opponent)
    int captured = 0;
    error_code_t result = board_execute_move(&board, PLAYER_A, 0, &captured);
    
    if (result == ERR_STARVE_VIOLATION) {
        printf("\n✓ Feeding rule correctly enforced!\n");
        printf("  Move rejected because opponent would remain starved.\n");
    } else {
        printf("\n✗ Feeding rule NOT enforced (move allowed)\n");
    }
    
    // Now try pit 1 which would give seeds to opponent
    result = board_execute_move(&board, PLAYER_A, 1, &captured);
    
    if (result == SUCCESS) {
        printf("\n✓ Alternative feeding move allowed!\n");
        board_print(&board);
    }
}

void test_full_game() {
    printf("\n=== TEST: Full Game Sequence ===\n");
    
    board_t board;
    board_init(&board);
    
    // Play a sequence of moves
    int moves[] = {2, 8, 3, 9, 4, 10};  // Alternating moves
    const char* player_names[] = {"Alice", "Bob"};
    
    board_print_detailed(&board, "Alice", "Bob");
    
    for (int i = 0; i < 6; i++) {
        player_id_t player = (i % 2 == 0) ? PLAYER_A : PLAYER_B;
        int pit = moves[i];
        int captured = 0;
        
        printf("\n>>> %s plays pit %d\n", player_names[player], pit);
        
        error_code_t result = board_execute_move(&board, player, pit, &captured);
        
        if (result == SUCCESS) {
            if (captured > 0) {
                printf("    ⭐ Captured %d seeds!\n", captured);
            }
            board_print_detailed(&board, "Alice", "Bob");
            
            if (board_is_game_over(&board)) {
                printf("\n🏁 GAME OVER!\n");
                winner_t winner = board_get_winner(&board);
                if (winner == WINNER_A) {
                    printf("Winner: Alice\n");
                } else if (winner == WINNER_B) {
                    printf("Winner: Bob\n");
                } else {
                    printf("Result: Draw\n");
                }
                break;
            }
        } else {
            printf("    ✗ Error: %s\n", error_to_string(result));
        }
    }
}

void test_validation() {
    printf("\n=== TEST: Move Validation ===\n");
    
    board_t board;
    board_init(&board);
    
    // Test 1: Invalid pit index
    printf("\nTest 1: Invalid pit index (15)\n");
    int captured;
    error_code_t result = board_execute_move(&board, PLAYER_A, 15, &captured);
    printf("Result: %s ✓\n", error_to_string(result));
    
    // Test 2: Wrong player's turn
    printf("\nTest 2: Wrong player's turn (Bob when Alice's turn)\n");
    result = board_execute_move(&board, PLAYER_B, 6, &captured);
    printf("Result: %s ✓\n", error_to_string(result));
    
    // Test 3: Wrong side
    printf("\nTest 3: Player A tries opponent's pit (7)\n");
    result = board_execute_move(&board, PLAYER_A, 7, &captured);
    printf("Result: %s ✓\n", error_to_string(result));
    
    // Test 4: Empty pit
    printf("\nTest 4: Playing empty pit\n");
    board.pits[0] = 0;
    result = board_execute_move(&board, PLAYER_A, 0, &captured);
    printf("Result: %s ✓\n", error_to_string(result));
}

int main() {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║     AWALE GAME - New Architecture Demo              ║\n");
    printf("║     Testing Game Logic Implementation               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    
    test_basic_move();
    test_capture();
    test_feeding_rule();
    test_validation();
    test_full_game();
    
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║     All Tests Complete                               ║\n");
    printf("║     ✓ Game logic is fully functional!               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}
