#ifndef SESSION_H
#define SESSION_H

#include "../common/types.h"
#include "../common/protocol.h"
#include "../common/messages.h"
#include "connection.h"

/* Session structure */
typedef struct {
    connection_t conn;
    char pseudo[MAX_PSEUDO_LEN];
    char session_id[64];
    bool authenticated;
    time_t created_at;
    time_t last_activity;
} session_t;

/* Session management */
error_code_t session_init(session_t* session);
error_code_t session_create(session_t* session, connection_t* conn, const char* pseudo);
error_code_t session_close(session_t* session);

/* Message sending (high-level) */
error_code_t session_send_message(session_t* session, message_type_t type, const void* payload, size_t payload_size);
error_code_t session_recv_message(session_t* session, message_type_t* type, void* payload, size_t max_payload_size, size_t* actual_size);
error_code_t session_recv_message_timeout(session_t* session, message_type_t* type, void* payload, size_t max_payload_size, size_t* actual_size, int timeout_ms);

/* Convenience functions for specific messages */
error_code_t session_send_error(session_t* session, error_code_t error, const char* msg);
error_code_t session_send_connect_ack(session_t* session, bool success, const char* msg);
error_code_t session_send_board_state(session_t* session, const msg_board_state_t* board);
error_code_t session_send_move_result(session_t* session, const msg_move_result_t* result);

/* Session validation */
bool session_is_active(const session_t* session);
bool session_is_authenticated(const session_t* session);
void session_touch_activity(session_t* session);

#endif /* SESSION_H */
