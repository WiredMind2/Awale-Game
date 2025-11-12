#include "../../include/network/session.h"
#include "../../include/network/serialization.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>

error_code_t session_init(session_t* session) {
    if (!session) return ERR_INVALID_PARAM;
    
    connection_init(&session->conn);
    memset(session->pseudo, 0, MAX_PSEUDO_LEN);
    memset(session->session_id, 0, 64);
    session->authenticated = false;
    session->created_at = time(NULL);
    session->last_activity = time(NULL);
    
    return SUCCESS;
}

error_code_t session_create(session_t* session, connection_t* conn, const char* pseudo) {
    if (!session || !conn || !pseudo) return ERR_INVALID_PARAM;
    
    memcpy(&session->conn, conn, sizeof(connection_t));
    strncpy(session->pseudo, pseudo, MAX_PSEUDO_LEN - 1);
    session->pseudo[MAX_PSEUDO_LEN - 1] = '\0';
    
    // Generate session ID
    snprintf(session->session_id, 64, "%s-%ld", pseudo, time(NULL));
    
    session->authenticated = true;
    session->created_at = time(NULL);
    session->last_activity = time(NULL);
    
    return SUCCESS;
}

error_code_t session_close(session_t* session) {
    if (!session) return ERR_INVALID_PARAM;
    
    connection_close(&session->conn);
    session->authenticated = false;
    
    return SUCCESS;
}

error_code_t session_send_message(session_t* session, message_type_t type, const void* payload, size_t payload_size) {
    if (!session) return ERR_INVALID_PARAM;
    
    char buffer[MAX_MESSAGE_SIZE];
    size_t total_size;
    
    error_code_t err = serialize_message(type, payload, payload_size, buffer, &total_size);
    if (err != SUCCESS) return err;
    
    err = connection_send_raw(&session->conn, buffer, total_size);
    if (err != SUCCESS) return err;
    
    session_touch_activity(session);
    return SUCCESS;
}

error_code_t session_recv_message(session_t* session, message_type_t* type, void* payload, size_t max_payload_size, size_t* actual_size) {
    if (!session || !type) return ERR_INVALID_PARAM;
    
    // First, read the header
    message_header_t header;
    size_t received;
    
    error_code_t err = connection_recv_raw(&session->conn, &header, sizeof(header), &received);
    if (err != SUCCESS || received != sizeof(header)) return ERR_NETWORK_ERROR;
    
    // Convert from network byte order
    header.type = ntohl(header.type);
    header.length = ntohl(header.length);
    header.sequence = ntohl(header.sequence);
    
    *type = (message_type_t)header.type;
    
    // Read payload if present
    if (header.length > 0) {
        if (!payload || header.length > max_payload_size) return ERR_SERIALIZATION;
        
        err = connection_recv_raw(&session->conn, payload, header.length, &received);
        if (err != SUCCESS || received != header.length) return ERR_NETWORK_ERROR;
        
        if (actual_size) *actual_size = header.length;
    } else {
        if (actual_size) *actual_size = 0;
    }
    
    session_touch_activity(session);
    return SUCCESS;
}

error_code_t session_recv_message_timeout(session_t* session, message_type_t* type, void* payload, size_t max_payload_size, size_t* actual_size, int timeout_ms) {
    if (!session || !type) return ERR_INVALID_PARAM;
    
    // First, read the header with timeout
    message_header_t header;
    size_t received;
    
    error_code_t err = connection_recv_timeout(&session->conn, &header, sizeof(header), &received, timeout_ms);
    if (err != SUCCESS) return err;
    if (received != sizeof(header)) return ERR_NETWORK_ERROR;
    
    // Convert from network byte order
    header.type = ntohl(header.type);
    header.length = ntohl(header.length);
    header.sequence = ntohl(header.sequence);
    
    *type = (message_type_t)header.type;
    
    // Read payload if present (also with timeout)
    if (header.length > 0) {
        if (!payload || header.length > max_payload_size) return ERR_SERIALIZATION;
        
        err = connection_recv_timeout(&session->conn, payload, header.length, &received, timeout_ms);
        if (err != SUCCESS) return err;
        if (received != header.length) return ERR_NETWORK_ERROR;
        
        if (actual_size) *actual_size = header.length;
    } else {
        if (actual_size) *actual_size = 0;
    }
    
    session_touch_activity(session);
    return SUCCESS;
}

error_code_t session_send_error(session_t* session, error_code_t error, const char* msg) {
    if (!session) return ERR_INVALID_PARAM;
    
    msg_error_t error_msg;
    error_msg.error_code = error;
    strncpy(error_msg.error_msg, msg ? msg : error_to_string(error), 255);
    error_msg.error_msg[255] = '\0';
    
    return session_send_message(session, MSG_ERROR, &error_msg, sizeof(error_msg));
}

error_code_t session_send_connect_ack(session_t* session, bool success, const char* msg) {
    if (!session) return ERR_INVALID_PARAM;
    
    msg_connect_ack_t ack;
    ack.success = success;
    strncpy(ack.message, msg ? msg : (success ? "Connected" : "Failed"), 255);
    ack.message[255] = '\0';
    strncpy(ack.session_id, session->session_id, 63);
    ack.session_id[63] = '\0';
    
    return session_send_message(session, MSG_CONNECT_ACK, &ack, sizeof(ack));
}

error_code_t session_send_message_connect_ack(connection_t conn, const char* msg) {   
    msg_connect_ack_t ack;
    session_t temp_session;
    temp_session.conn = conn;
    strncpy(ack.message, msg ? msg : ("Failed"), 255);
    ack.message[255] = '\0';
    return session_send_message(&temp_session, MSG_CONNECT_ACK, &ack, sizeof(ack));
}

error_code_t session_send_board_state(session_t* session, const msg_board_state_t* board) {
    if (!session || !board) return ERR_INVALID_PARAM;
    return session_send_message(session, MSG_BOARD_STATE, board, sizeof(msg_board_state_t));
}

error_code_t session_send_move_result(session_t* session, const msg_move_result_t* result) {
    if (!session || !result) return ERR_INVALID_PARAM;
    return session_send_message(session, MSG_MOVE_RESULT, result, sizeof(msg_move_result_t));
}

bool session_is_active(const session_t* session) {
    return session && session->authenticated && connection_is_connected(&session->conn);
}

bool session_is_authenticated(const session_t* session) {
    return session && session->authenticated;
}

void session_touch_activity(session_t* session) {
    if (session) {
        session->last_activity = time(NULL);
    }
}
