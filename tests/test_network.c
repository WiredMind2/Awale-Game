/* Unit Tests for Network Layer
 * Tests message serialization, connection management, and protocol handling
 */

#include "network/serialization.h"
#include "network/connection.h"
#include "common/protocol.h"
#include "common/messages.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test utilities */
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("Running test: %s...", #name); \
    test_##name(); \
    printf(" OK\n"); \
    tests_passed++; \
} while(0)

static int tests_passed = 0;

/* ========== Serialization Tests ========== */

TEST(serialize_int32) {
    serialize_buffer_t buffer;
    serialize_buffer_init(&buffer);
    
    int32_t value = 12345;
    error_code_t err = serialize_int32(&buffer, value);
    
    assert(err == SUCCESS);
    assert(buffer.size == sizeof(int32_t));
    
    /* Deserialize and verify */
    serialize_buffer_reset(&buffer);
    int32_t result;
    err = deserialize_int32(&buffer, &result);
    assert(err == SUCCESS);
    assert(result == value);
}

TEST(serialize_string) {
    serialize_buffer_t buffer;
    serialize_buffer_init(&buffer);
    
    const char* text = "Hello";
    error_code_t err = serialize_string(&buffer, text, 50);
    
    assert(err == SUCCESS);
    assert(buffer.size == 50); /* Fixed size */
    
    /* Deserialize and verify */
    serialize_buffer_reset(&buffer);
    char result[50];
    err = deserialize_string(&buffer, result, 50);
    assert(err == SUCCESS);
    assert(strcmp(result, text) == 0);
}

TEST(serialize_bool) {
    serialize_buffer_t buffer;
    serialize_buffer_init(&buffer);
    
    error_code_t err = serialize_bool(&buffer, true);
    assert(err == SUCCESS);
    
    serialize_buffer_reset(&buffer);
    bool result;
    err = deserialize_bool(&buffer, &result);
    assert(err == SUCCESS);
    assert(result == true);
}

TEST(serialize_message_header) {
    serialize_buffer_t buffer;
    serialize_buffer_init(&buffer);
    
    message_header_t header;
    header.type = MSG_CONNECT;
    header.length = 100;
    header.sequence = 42;
    header.reserved = 0;
    
    error_code_t err = serialize_header(&buffer, &header);
    assert(err == SUCCESS);
    assert(buffer.size == sizeof(message_header_t));
    
    /* Deserialize and verify */
    serialize_buffer_reset(&buffer);
    message_header_t result;
    err = deserialize_header(&buffer, &result);
    assert(err == SUCCESS);
    assert(result.type == MSG_CONNECT);
    assert(result.length == 100);
    assert(result.sequence == 42);
}

TEST(serialize_full_message) {
    msg_connect_t connect_msg;
    strncpy(connect_msg.pseudo, "Alice", MAX_PSEUDO_LEN);
    strncpy(connect_msg.version, "1.0", 16);
    
    char output[MAX_MESSAGE_SIZE];
    size_t output_size;
    
    error_code_t err = serialize_message(MSG_CONNECT, &connect_msg, 
                                        sizeof(connect_msg), output, &output_size);
    
    assert(err == SUCCESS);
    assert(output_size == HEADER_SIZE + sizeof(connect_msg));
}

TEST(message_size_calculation) {
    /* Test that we correctly calculate message sizes */
    msg_player_list_t list;
    list.count = 3;
    
    /* Correct size: count + actual players */
    size_t correct_size = sizeof(list.count) + (3 * sizeof(player_info_t));
    
    /* Wrong size: entire struct */
    size_t wrong_size = sizeof(list);
    
    /* Verify the difference is significant */
    assert(wrong_size > correct_size);
    /* Note: exact size may vary due to struct padding, but it should be larger than correct_size */
}

/* ========== Connection Tests ========== */

TEST(connection_init) {
    connection_t conn;
    error_code_t err = connection_init(&conn);
    
    assert(err == SUCCESS);
    assert(conn.socket_fd == -1);
    assert(conn.connected == false);
    assert(conn.sequence == 0);
}

TEST(connection_is_connected) {
    connection_t conn;
    connection_init(&conn);
    
    assert(connection_is_connected(&conn) == false);
    
    /* Simulate connection */
    conn.socket_fd = 3;
    conn.connected = true;
    
    assert(connection_is_connected(&conn) == true);
}

/* ========== Select Context Tests ========== */

TEST(select_context_init) {
    select_context_t ctx;
    error_code_t err = select_context_init(&ctx, 5, 0);
    
    assert(err == SUCCESS);
    assert(ctx.max_fd == -1);
    assert(ctx.timeout.tv_sec == 5);
    assert(ctx.timeout.tv_usec == 0);
}

TEST(select_context_add_fd) {
    select_context_t ctx;
    select_context_init(&ctx, 1, 0);
    
    int test_fd = 5;
    select_context_add_read(&ctx, test_fd);
    
    assert(ctx.max_fd == test_fd);
    assert(FD_ISSET(test_fd, &ctx.read_fds));
}

/* ========== Protocol Tests ========== */

TEST(message_type_to_string) {
    assert(strcmp(message_type_to_string(MSG_CONNECT), "CONNECT") == 0);
    assert(strcmp(message_type_to_string(MSG_LIST_PLAYERS), "LIST_PLAYERS") == 0);
    assert(strcmp(message_type_to_string(MSG_PLAY_MOVE), "PLAY_MOVE") == 0);
}

TEST(error_to_string) {
    assert(strcmp(error_to_string(SUCCESS), "Success") == 0);
    assert(strcmp(error_to_string(ERR_INVALID_MOVE), "Invalid move") == 0);
    assert(strcmp(error_to_string(ERR_NETWORK_ERROR), "Network error") == 0);
}

/* ========== Message Structure Tests ========== */

TEST(msg_connect_structure) {
    msg_connect_t msg;
    strncpy(msg.pseudo, "TestUser", MAX_PSEUDO_LEN);
    strncpy(msg.version, PROTOCOL_VERSION, 16);
    
    assert(strlen(msg.pseudo) > 0);
    assert(strcmp(msg.version, "1.0") == 0);
}

TEST(msg_play_move_structure) {
    msg_play_move_t msg;
    strncpy(msg.game_id, "game123", MAX_GAME_ID_LEN);
    msg.pit_index = 5;
    
    assert(msg.pit_index >= 0 && msg.pit_index < NUM_PITS);
    assert(strlen(msg.game_id) > 0);
}

TEST(msg_board_state_structure) {
    msg_board_state_t msg;
    
    /* Initialize board state */
    for (int i = 0; i < NUM_PITS; i++) {
        msg.pits[i] = 4;
    }
    msg.score_a = 0;
    msg.score_b = 0;
    msg.current_player = PLAYER_A;
    msg.state = GAME_STATE_IN_PROGRESS;
    msg.winner = NO_WINNER;
    
    assert(msg.pits[0] == 4);
    assert(msg.current_player == PLAYER_A);
    assert(msg.state == GAME_STATE_IN_PROGRESS);
}

/* ========== Buffer Overflow Tests ========== */

TEST(serialization_buffer_overflow) {
    /* Test that serialization doesn't overflow buffers */
    serialize_buffer_t buffer;
    serialize_buffer_init(&buffer);

    /* Try to serialize a very long string larger than MAX_MESSAGE_SIZE to force overflow */
    char long_string[9000];
    memset(long_string, 'A', 8999);
    long_string[8999] = '\0';

    /* Pass a large max_len so serializer tries to reserve > MAX_MESSAGE_SIZE */
    error_code_t err = serialize_string(&buffer, long_string, 9000);
    assert(err != SUCCESS); /* Should fail due to buffer overflow */
}

TEST(message_size_limit) {
    /* Test message size validation */
    msg_player_list_t list;
    memset(&list, 0, sizeof(list));
    /* Compute the maximum number of player entries that fit in the payload */
    size_t max_allowed = (MAX_PAYLOAD_SIZE - sizeof(list.count)) / sizeof(player_info_t);

    /* Use the maximum allowed count and ensure it fits */
    list.count = (int)max_allowed;
    size_t msg_size = sizeof(list.count) + (list.count * sizeof(player_info_t));
    assert(msg_size <= MAX_PAYLOAD_SIZE);

    /* Using one more than allowed should overflow */
    list.count = (int)(max_allowed + 1);
    msg_size = sizeof(list.count) + (list.count * sizeof(player_info_t));
    assert(msg_size > MAX_PAYLOAD_SIZE);
}

/* ========== Error Handling Tests ========== */

TEST(protocol_error_handling) {
    /* Test invalid message types */
    assert(message_type_to_string(999) == NULL);
    assert(error_to_string(999) == NULL);
    
    /* Test valid message types */
    assert(message_type_to_string(MSG_CONNECT) != NULL);
    assert(message_type_to_string(MSG_PLAY_MOVE) != NULL);
    
    /* Test valid error codes */
    assert(error_to_string(SUCCESS) != NULL);
    assert(error_to_string(ERR_INVALID_PARAM) != NULL);
}

TEST(connection_error_states) {
    connection_t conn;
    
    /* Test uninitialized connection */
    memset(&conn, 0, sizeof(conn));
    assert(connection_is_connected(&conn) == false);
    
    /* Test after init */
    error_code_t err = connection_init(&conn);
    assert(err == SUCCESS);
    assert(connection_is_connected(&conn) == false);
    assert(conn.socket_fd == -1);
}

TEST(select_context_edge_cases) {
    select_context_t ctx;
    
    /* Test with invalid parameters */
    error_code_t err = select_context_init(NULL, 1, 0);
    assert(err != SUCCESS);
    
    /* Test valid initialization */
    err = select_context_init(&ctx, 1, 0);
    assert(err == SUCCESS);
    
    /* Test adding invalid fd */
    err = select_context_add_read(&ctx, -1);
    assert(err != SUCCESS);
    
    /* Test adding valid fd */
    err = select_context_add_read(&ctx, 0); /* stdin */
    assert(err == SUCCESS);
}

TEST(serialization_edge_cases) {
    /* Test serialization with edge case values */
    serialize_buffer_t buffer;
    serialize_buffer_init(&buffer);

    /* Zero-length string */
    error_code_t err = serialize_string(&buffer, "", 1);
    assert(err == SUCCESS);

    /* Maximum int32 value */
    int32_t max_int = 0x7FFFFFFF;
    err = serialize_int32(&buffer, max_int);
    assert(err == SUCCESS);

    /* Minimum int32 value */
    int32_t min_int = (int32_t)0x80000000u;
    err = serialize_int32(&buffer, min_int);
    assert(err == SUCCESS);

    /* Test boolean false */
    bool flag = false;
    err = serialize_bool(&buffer, flag);
    assert(err == SUCCESS);

    /* Test boolean true */
    flag = true;
    err = serialize_bool(&buffer, flag);
    assert(err == SUCCESS);
}

/* ========== Main Test Runner ========== */

int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║       AWALE GAME - Unit Tests (Network Layer)       ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Serialization tests */
    printf("Serialization Tests:\n");
    RUN_TEST(serialize_int32);
    RUN_TEST(serialize_string);
    RUN_TEST(serialize_bool);
    RUN_TEST(serialize_message_header);
    RUN_TEST(serialize_full_message);
    RUN_TEST(message_size_calculation);
    
    /* Connection tests */
    printf("\nConnection Tests:\n");
    RUN_TEST(connection_init);
    RUN_TEST(connection_is_connected);
    
    /* Select tests */
    printf("\nSelect Context Tests:\n");
    RUN_TEST(select_context_init);
    RUN_TEST(select_context_add_fd);
    
    /* Protocol tests */
    printf("\nProtocol Tests:\n");
    RUN_TEST(message_type_to_string);
    RUN_TEST(error_to_string);
    
    /* Message structure tests */
    printf("\nMessage Structure Tests:\n");
    RUN_TEST(msg_connect_structure);
    RUN_TEST(msg_play_move_structure);
    RUN_TEST(msg_board_state_structure);
    
    /* Buffer tests */
    printf("\nBuffer Safety Tests:\n");
    RUN_TEST(serialization_buffer_overflow);
    RUN_TEST(message_size_limit);
    
    /* Error handling tests */
    printf("\nError Handling Tests:\n");
    RUN_TEST(protocol_error_handling);
    RUN_TEST(connection_error_states);
    RUN_TEST(select_context_edge_cases);
    RUN_TEST(serialization_edge_cases);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  All %d tests passed!\n", tests_passed);
    printf("═══════════════════════════════════════════════════════\n");
    printf("\n");
    
    return 0;
}
