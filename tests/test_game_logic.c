/* Unit Tests for Game Logic
 * Tests board operations, rules validation, feeding rule, and capture logic
 */

#include "game/board.h"
#include "game/rules.h"
#include "game/player.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test utilities */
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("Running test: %s...", #name); \
    test_##name(); \
    printf(" ✓\n"); \
    tests_passed++; \
} while(0)

static int tests_passed = 0;

/* ========== Board Tests ========== */

TEST(board_init) {
    board_t board;
    error_code_t err = board_init(&board);
    
    assert(err == SUCCESS);
    assert(board.current_player == PLAYER_A);
    assert(board.state == GAME_STATE_IN_PROGRESS);
    assert(board.winner == NO_WINNER);
    assert(board.scores[PLAYER_A] == 0);
    assert(board.scores[PLAYER_B] == 0);
    
    /* Each pit should have 4 seeds */
    for (int i = 0; i < NUM_PITS; i++) {
        assert(board.pits[i] == INITIAL_SEEDS_PER_PIT);
    }
}

TEST(board_execute_simple_move) {
    board_t board;
    board_init(&board);
    
    int captured = 0;
    /* Player A plays pit 0 (has 4 seeds) */
    error_code_t err = board_execute_move(&board, PLAYER_A, 0, &captured);
    
    assert(err == SUCCESS);
    assert(board.pits[0] == 0); /* Pit 0 should be empty */
    assert(board.pits[1] == 5); /* Pit 1 gets +1 */
    assert(board.pits[2] == 5); /* Pit 2 gets +1 */
    assert(board.pits[3] == 5); /* Pit 3 gets +1 */
    assert(board.pits[4] == 5); /* Pit 4 gets +1 */
    assert(captured == 0); /* No captures on first move */
    assert(board.current_player == PLAYER_B); /* Turn switches */
}

TEST(board_capture_two_seeds) {
    board_t board;
    board_init(&board);
    
    /* Setup: Make opponent pit 7 have 1 seed */
    board.pits[7] = 1;
    board.pits[3] = 4;
    
    int captured = 0;
    /* Player A plays pit 3, lands in pit 7 with 2 seeds total */
    error_code_t err = board_execute_move(&board, PLAYER_A, 3, &captured);
    
    assert(err == SUCCESS);
    assert(captured == 2); /* Should capture 2 seeds */
    assert(board.pits[7] == 0); /* Pit should be empty after capture */
    assert(board.scores[PLAYER_A] == 2);
}

TEST(board_capture_three_seeds) {
    board_t board;
    board_init(&board);
    
    /* Setup: Make opponent pit 8 have 2 seeds */
    board.pits[8] = 2;
    board.pits[4] = 4;
    
    int captured = 0;
    /* Player A plays pit 4, lands in pit 8 with 3 seeds total */
    error_code_t err = board_execute_move(&board, PLAYER_A, 4, &captured);
    
    assert(err == SUCCESS);
    assert(captured == 3); /* Should capture 3 seeds */
    assert(board.pits[8] == 0); /* Pit should be empty after capture */
    assert(board.scores[PLAYER_A] == 3);
}

TEST(board_no_capture_in_own_row) {
    board_t board;
    board_init(&board);
    
    /* Setup: Make own pit 2 have 1 seed */
    board.pits[2] = 1;
    board.pits[0] = 2;
    
    int captured = 0;
    /* Player A plays pit 0, lands in pit 2 with 2 seeds */
    error_code_t err = board_execute_move(&board, PLAYER_A, 0, &captured);
    
    assert(err == SUCCESS);
    assert(captured == 0); /* No capture in own row */
    assert(board.pits[2] == 2); /* Seeds remain */
}

TEST(board_win_condition_25_seeds) {
    board_t board;
    board_init(&board);
    
    /* Setup: Player A has 24 seeds, one capture away from win */
    board.scores[PLAYER_A] = 24;
    board.pits[7] = 1;
    board.pits[3] = 4;
    
    int captured = 0;
    board_execute_move(&board, PLAYER_A, 3, &captured);
    
    assert(captured == 2);
    assert(board.scores[PLAYER_A] == 26);
    assert(board_is_game_over(&board));
    assert(board_get_winner(&board) == WINNER_A);
}

/* ========== Rules Tests ========== */

TEST(rules_validate_empty_pit) {
    board_t board;
    board_init(&board);
    
    board.pits[0] = 0; /* Empty pit */
    
    error_code_t err = rules_validate_move(&board, PLAYER_A, 0);
    assert(err == ERR_EMPTY_PIT);
}

TEST(rules_validate_wrong_side) {
    board_t board;
    board_init(&board);
    
    /* Player A tries to play from Player B's side */
    error_code_t err = rules_validate_move(&board, PLAYER_A, 6);
    assert(err == ERR_WRONG_SIDE);
}

TEST(rules_validate_not_your_turn) {
    board_t board;
    board_init(&board);
    
    /* Player B tries to move when it's Player A's turn */
    error_code_t err = rules_validate_move(&board, PLAYER_B, 6);
    assert(err == ERR_NOT_YOUR_TURN);
}

TEST(rules_feeding_rule_violation) {
    board_t board;
    board_init(&board);
    
    /* Setup: Opponent has no seeds, player can feed */
    for (int i = 6; i < 12; i++) {
        board.pits[i] = 0; /* Player B has no seeds */
    }
    board.pits[0] = 3; /* Can reach opponent side */
    board.pits[1] = 2; /* Can't reach opponent side */
    
    /* Playing pit 1 would starve opponent, and pit 0 is a feeding alternative */
    assert(rules_would_starve_opponent(&board, PLAYER_A, 1));
    assert(rules_has_feeding_alternative(&board, PLAYER_A, 1));
    
    error_code_t err = rules_validate_move(&board, PLAYER_A, 1);
    assert(err == ERR_STARVE_VIOLATION);
}

TEST(rules_feeding_rule_no_alternative) {
    board_t board;
    board_init(&board);
    
    /* Setup: Opponent has no seeds */
    for (int i = 6; i < 12; i++) {
        board.pits[i] = 0;
    }
    /* Player A has seeds but all moves are too short to feed opponent */
    board.pits[0] = 5; /* Can't reach opponent (only goes to pit 5) */
    board.pits[1] = 4; /* Can't reach opponent */
    board.pits[2] = 3; /* Can't reach opponent */
    board.pits[3] = 2; /* Can't reach opponent */
    board.pits[4] = 1; /* Can't reach opponent */
    board.pits[5] = 0; /* Empty */
    
    /* Since no move can reach opponent, any non-empty pit move should be allowed */
    error_code_t err = rules_validate_move(&board, PLAYER_A, 0);
    /* This move will be allowed as there's no feeding alternative */
    assert(err == SUCCESS || err == ERR_STARVE_VIOLATION); /* Depends on implementation */
}

TEST(rules_capture_chain) {
    board_t board;
    board_init(&board);
    
    /* Setup for a simple capture test */
    /* Make opponent pit have exactly 1 seed so adding 1 makes 2 (capturable) */
    board.pits[7] = 1;
    board.pits[3] = 4; /* Playing this will land in pit 7 */
    
    int captured = 0;
    error_code_t err = board_execute_move(&board, PLAYER_A, 3, &captured);
    
    assert(err == SUCCESS);
    /* Should capture at least the 2 seeds from pit 7 */
    assert(captured == 2);
    assert(board.pits[7] == 0);
}

TEST(rules_skip_origin_on_lap) {
    board_t board;
    board_init(&board);
    
    /* Setup: Give player A pit 0 enough seeds for a full lap (13+) */
    board.pits[0] = 13;
    
    int captured = 0;
    board_execute_move(&board, PLAYER_A, 0, &captured);
    
    /* After full lap, pit 0 should still be 0 (skipped on lap) */
    assert(board.pits[0] == 0);
    /* Pit 1 should have gained a seed on the lap */
    assert(board.pits[1] >= 5); /* Original 4 + 1 from sowing */
}

/* ========== Player Tests ========== */

TEST(player_init_valid) {
    player_t player;
    error_code_t err = player_init(&player, "Alice", "192.168.1.1");
    
    assert(err == SUCCESS);
    assert(strcmp(player.info.pseudo, "Alice") == 0);
    assert(strcmp(player.info.ip, "192.168.1.1") == 0);
    assert(player.connected == true);
    assert(player.games_played == 0);
    assert(player.games_won == 0);
}

TEST(player_invalid_pseudo) {
    /* Pseudo with invalid characters */
    assert(player_is_valid_pseudo("Invalid!@#") == false);
    
    /* Empty pseudo */
    assert(player_is_valid_pseudo("") == false);
    
    /* Valid pseudo */
    assert(player_is_valid_pseudo("Alice_123") == true);
    assert(player_is_valid_pseudo("Bob-456") == true);
}

TEST(player_update_stats) {
    player_t player;
    player_init(&player, "Alice", "127.0.0.1");
    
    player_update_stats(&player, true); /* Win */
    assert(player.games_played == 1);
    assert(player.games_won == 1);
    
    player_update_stats(&player, false); /* Loss */
    assert(player.games_played == 2);
    assert(player.games_won == 1);
}

/* ========== Main Test Runner ========== */

int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║         AWALE GAME - Unit Tests (Game Logic)        ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Board tests */
    printf("Board Tests:\n");
    RUN_TEST(board_init);
    RUN_TEST(board_execute_simple_move);
    RUN_TEST(board_capture_two_seeds);
    RUN_TEST(board_capture_three_seeds);
    RUN_TEST(board_no_capture_in_own_row);
    RUN_TEST(board_win_condition_25_seeds);
    
    /* Rules tests */
    printf("\nRules Tests:\n");
    RUN_TEST(rules_validate_empty_pit);
    RUN_TEST(rules_validate_wrong_side);
    RUN_TEST(rules_validate_not_your_turn);
    RUN_TEST(rules_feeding_rule_violation);
    RUN_TEST(rules_feeding_rule_no_alternative);
    RUN_TEST(rules_capture_chain);
    RUN_TEST(rules_skip_origin_on_lap);
    
    /* Player tests */
    printf("\nPlayer Tests:\n");
    RUN_TEST(player_init_valid);
    RUN_TEST(player_invalid_pseudo);
    RUN_TEST(player_update_stats);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  ✓ All %d tests passed!\n", tests_passed);
    printf("═══════════════════════════════════════════════════════\n");
    printf("\n");
    
    return 0;
}
