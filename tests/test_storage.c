/* Unit Tests for Storage Layer
 * Tests data persistence, CRC validation, and file operations
 */

#include "server/storage.h"
#include "server/game_manager.h"
#include "server/matchmaking.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
 #include <stdlib.h>

/* Test utilities */
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("Running test: %s...", #name); \
    test_##name(); \
    printf(" OK\n"); \
    tests_passed++; \
} while(0)

static int tests_passed = 0;

/* ========== Storage Tests ========== */

TEST(storage_init_cleanup) {
    error_code_t err = storage_init();
    assert(err == SUCCESS);

    err = storage_cleanup();
    assert(err == SUCCESS);
}

TEST(storage_directory_creation) {
    /* Clean up any existing data */
    system("rm -rf ./data");

    error_code_t err = storage_init();
    assert(err == SUCCESS);

    /* Check that data directory was created */
    bool exists = false;
    err = storage_file_exists("./data", &exists);
    assert(err == SUCCESS);
    assert(exists == true);

    storage_cleanup();
}

TEST(game_save_load) {
    storage_init();

    /* Create a test game */
    game_instance_t game;
    memset(&game, 0, sizeof(game));
    snprintf(game.game_id, MAX_GAME_ID_LEN, "test-game-123");
    snprintf(game.player_a, MAX_PSEUDO_LEN, "Alice");
    snprintf(game.player_b, MAX_PSEUDO_LEN, "Bob");
    game.active = true;

    /* Initialize board */
    board_init(&game.board);
    game.board.scores[PLAYER_A] = 15;
    game.board.scores[PLAYER_B] = 12;
    game.board.current_player = PLAYER_B;

    /* Save game */
    error_code_t err = storage_save_game(&game);
    if (err != SUCCESS) {
        fprintf(stderr, "test_storage: storage_save_game failed with error %d\n", err);
        abort();
    }

    /* Load game */
    game_instance_t loaded_game;
    err = storage_load_game("test-game-123", &loaded_game);
    assert(err == SUCCESS);

    /* Verify data */
    assert(strcmp(loaded_game.game_id, "test-game-123") == 0);
    assert(strcmp(loaded_game.player_a, "Alice") == 0);
    assert(strcmp(loaded_game.player_b, "Bob") == 0);
    assert(loaded_game.active == true);
    assert(loaded_game.board.scores[PLAYER_A] == 15);
    assert(loaded_game.board.scores[PLAYER_B] == 12);
    assert(loaded_game.board.current_player == PLAYER_B);

    /* Clean up */
    storage_delete_game("test-game-123");
    storage_cleanup();
}

TEST(game_delete) {
    storage_init();

    /* Create and save a test game */
    game_instance_t game;
    memset(&game, 0, sizeof(game));
    snprintf(game.game_id, MAX_GAME_ID_LEN, "delete-test");
    snprintf(game.player_a, MAX_PSEUDO_LEN, "Player1");
    snprintf(game.player_b, MAX_PSEUDO_LEN, "Player2");
    game.active = true;
    board_init(&game.board);

    error_code_t err = storage_save_game(&game);
    assert(err == SUCCESS);

    /* Verify file exists */
    bool exists = false;
    err = storage_file_exists("./data/games.dat", &exists);
    assert(err == SUCCESS);
    assert(exists == true);

    /* Delete game */
    err = storage_delete_game("delete-test");
    assert(err == SUCCESS);

    /* Verify game file still exists but game is gone */
    err = storage_load_game("delete-test", &game);
    assert(err != SUCCESS); /* Should fail to load deleted game */

    storage_cleanup();
}

TEST(player_save_load) {
    storage_init();

    /* Create test matchmaking with players */
    matchmaking_t mm;
    memset(&mm, 0, sizeof(mm));
    matchmaking_init(&mm);

    /* Add test players */
    error_code_t err = matchmaking_add_player(&mm, "TestPlayer1", "192.168.1.1");
    assert(err == SUCCESS);
    err = matchmaking_add_player(&mm, "TestPlayer2", "192.168.1.2");
    assert(err == SUCCESS);

    /* Update player stats */
    err = matchmaking_update_player_stats(&mm, "TestPlayer1", true, 25);
    assert(err == SUCCESS);
    err = matchmaking_update_player_stats(&mm, "TestPlayer2", false, 18);
    assert(err == SUCCESS);

    /* Save players */
    err = storage_save_players(&mm);
    assert(err == SUCCESS);

    /* Load players into new matchmaking */
    matchmaking_t mm2;
    memset(&mm2, 0, sizeof(mm2));
    matchmaking_init(&mm2);

    err = storage_load_players(&mm2);
    assert(err == SUCCESS);

    /* Verify loaded data */
    player_info_t info;
    err = matchmaking_get_player_stats(&mm2, "TestPlayer1", &info);
    assert(err == SUCCESS);
    assert(info.games_played == 1);
    assert(info.games_won == 1);
    assert(info.games_lost == 0);
    assert(info.total_score == 25);

    err = matchmaking_get_player_stats(&mm2, "TestPlayer2", &info);
    assert(err == SUCCESS);
    assert(info.games_played == 1);
    assert(info.games_won == 0);
    assert(info.games_lost == 1);
    assert(info.total_score == 18);

    matchmaking_destroy(&mm);
    matchmaking_destroy(&mm2);
    storage_cleanup();
}

TEST(data_integrity_crc) {
    storage_init();

    /* Create a test game */
    game_instance_t game;
    memset(&game, 0, sizeof(game));
    snprintf(game.game_id, MAX_GAME_ID_LEN, "crc-test");
    snprintf(game.player_a, MAX_PSEUDO_LEN, "CRCPlayer");
    snprintf(game.player_b, MAX_PSEUDO_LEN, "CRCPlayer2");
    game.active = true;
    board_init(&game.board);

    /* Save game */
    error_code_t err = storage_save_game(&game);
    assert(err == SUCCESS);

    /* Manually corrupt the file */
    FILE* f = fopen("./data/games.dat", "r+b");
    if (f) {
        fseek(f, 10, SEEK_SET); /* Skip header */
        uint8_t bad_byte = 0xFF;
        fwrite(&bad_byte, 1, 1, f);
        fclose(f);
    }

    /* Try to load - should fail due to CRC mismatch */
    game_instance_t loaded_game;
    err = storage_load_game("crc-test", &loaded_game);
    assert(err != SUCCESS); /* Should fail due to corruption */

    storage_cleanup();
}

TEST(player_bio_functionality) {
    storage_init();

    matchmaking_t mm;
    memset(&mm, 0, sizeof(mm));
    matchmaking_init(&mm);

    /* Add player */
    error_code_t err = matchmaking_add_player(&mm, "BioTestPlayer", "127.0.0.1");
    assert(err == SUCCESS);

    /* Set bio via matchmaking API */
    {
        const char bio_lines[2][256] = {"This is line 1 of my bio","This is line 2 of my bio"};
        err = matchmaking_set_player_bio(&mm, "BioTestPlayer", bio_lines, 2);
        assert(err == SUCCESS);
    }

    /* Update stats (this will trigger save) */
    err = matchmaking_update_player_stats(&mm, "BioTestPlayer", true, 10);
    assert(err == SUCCESS);

    /* Save and reload (redundant but explicit) */
    err = storage_save_players(&mm);
    assert(err == SUCCESS);

    matchmaking_t mm2;
    memset(&mm2, 0, sizeof(mm2));
    matchmaking_init(&mm2);
    err = storage_load_players(&mm2);
    assert(err == SUCCESS);

    /* Verify bio was preserved */
    player_info_t info;
    err = matchmaking_get_player_stats(&mm2, "BioTestPlayer", &info);
    assert(err == SUCCESS);
    assert(info.bio_lines == 2);
    assert(strcmp(info.bio[0], "This is line 1 of my bio") == 0);
    assert(strcmp(info.bio[1], "This is line 2 of my bio") == 0);

    matchmaking_destroy(&mm);
    matchmaking_destroy(&mm2);
    storage_cleanup();
}

/* ========== Main Test Runner ========== */

int main() {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║        AWALE GAME - Unit Tests (Storage Layer)      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\n");

    /* Storage Tests */
    RUN_TEST(storage_init_cleanup);
    RUN_TEST(storage_directory_creation);
    RUN_TEST(game_save_load);
    RUN_TEST(game_delete);
    RUN_TEST(player_save_load);
    RUN_TEST(data_integrity_crc);
    RUN_TEST(player_bio_functionality);

    printf("\n═══════════════════════════════════════════════════════\n");
    printf("  All %d tests passed!\n", tests_passed);
    printf("═══════════════════════════════════════════════════════\n");

    return 0;
}