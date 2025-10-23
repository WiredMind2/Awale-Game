#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "../common/types.h"
#include "../common/protocol.h"
#include "../common/messages.h"
#include <stddef.h>

/* Buffer for serialized data */
typedef struct {
    char data[MAX_MESSAGE_SIZE];
    size_t size;
    size_t position;
} serialize_buffer_t;

/* Buffer initialization */
void serialize_buffer_init(serialize_buffer_t* buffer);
void serialize_buffer_reset(serialize_buffer_t* buffer);

/* Serialization primitives */
error_code_t serialize_int32(serialize_buffer_t* buffer, int32_t value);
error_code_t serialize_uint32(serialize_buffer_t* buffer, uint32_t value);
error_code_t serialize_bool(serialize_buffer_t* buffer, bool value);
error_code_t serialize_string(serialize_buffer_t* buffer, const char* str, size_t max_len);
error_code_t serialize_bytes(serialize_buffer_t* buffer, const void* data, size_t size);

/* Deserialization primitives */
error_code_t deserialize_int32(serialize_buffer_t* buffer, int32_t* value);
error_code_t deserialize_uint32(serialize_buffer_t* buffer, uint32_t* value);
error_code_t deserialize_bool(serialize_buffer_t* buffer, bool* value);
error_code_t deserialize_string(serialize_buffer_t* buffer, char* str, size_t max_len);
error_code_t deserialize_bytes(serialize_buffer_t* buffer, void* data, size_t size);

/* Message header serialization */
error_code_t serialize_header(serialize_buffer_t* buffer, const message_header_t* header);
error_code_t deserialize_header(serialize_buffer_t* buffer, message_header_t* header);

/* High-level message serialization */
error_code_t serialize_message(message_type_t type, const void* payload, size_t payload_size, 
                               void* output, size_t* output_size);
error_code_t deserialize_message(const void* input, size_t input_size, 
                                 message_type_t* type, void* payload, size_t max_payload_size);

/* Message-specific serializers */
error_code_t serialize_connect(serialize_buffer_t* buffer, const msg_connect_t* msg);
error_code_t serialize_board_state(serialize_buffer_t* buffer, const msg_board_state_t* msg);
error_code_t serialize_move(serialize_buffer_t* buffer, const msg_play_move_t* msg);
error_code_t serialize_challenge(serialize_buffer_t* buffer, const msg_challenge_t* msg);

/* Message-specific deserializers */
error_code_t deserialize_connect(serialize_buffer_t* buffer, msg_connect_t* msg);
error_code_t deserialize_board_state(serialize_buffer_t* buffer, msg_board_state_t* msg);
error_code_t deserialize_move(serialize_buffer_t* buffer, msg_play_move_t* msg);
error_code_t deserialize_challenge(serialize_buffer_t* buffer, msg_challenge_t* msg);

#endif /* SERIALIZATION_H */
