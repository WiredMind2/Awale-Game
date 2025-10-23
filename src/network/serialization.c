#include "../../include/network/serialization.h"
#include <string.h>
#include <arpa/inet.h>

void serialize_buffer_init(serialize_buffer_t* buffer) {
    if (!buffer) return;
    memset(buffer->data, 0, MAX_MESSAGE_SIZE);
    buffer->size = 0;
    buffer->position = 0;
}

void serialize_buffer_reset(serialize_buffer_t* buffer) {
    if (!buffer) return;
    buffer->position = 0;
}

error_code_t serialize_int32(serialize_buffer_t* buffer, int32_t value) {
    if (!buffer) return ERR_INVALID_PARAM;
    if (buffer->position + sizeof(int32_t) > MAX_MESSAGE_SIZE) return ERR_SERIALIZATION;
    
    int32_t net_value = htonl(value);
    memcpy(buffer->data + buffer->position, &net_value, sizeof(int32_t));
    buffer->position += sizeof(int32_t);
    if (buffer->position > buffer->size) buffer->size = buffer->position;
    
    return SUCCESS;
}

error_code_t serialize_uint32(serialize_buffer_t* buffer, uint32_t value) {
    if (!buffer) return ERR_INVALID_PARAM;
    if (buffer->position + sizeof(uint32_t) > MAX_MESSAGE_SIZE) return ERR_SERIALIZATION;
    
    uint32_t net_value = htonl(value);
    memcpy(buffer->data + buffer->position, &net_value, sizeof(uint32_t));
    buffer->position += sizeof(uint32_t);
    if (buffer->position > buffer->size) buffer->size = buffer->position;
    
    return SUCCESS;
}

error_code_t serialize_bool(serialize_buffer_t* buffer, bool value) {
    if (!buffer) return ERR_INVALID_PARAM;
    if (buffer->position + 1 > MAX_MESSAGE_SIZE) return ERR_SERIALIZATION;
    
    buffer->data[buffer->position] = value ? 1 : 0;
    buffer->position++;
    if (buffer->position > buffer->size) buffer->size = buffer->position;
    
    return SUCCESS;
}

error_code_t serialize_string(serialize_buffer_t* buffer, const char* str, size_t max_len) {
    if (!buffer || !str) return ERR_INVALID_PARAM;
    
    size_t len = strlen(str);
    if (len >= max_len) len = max_len - 1;
    
    if (buffer->position + len + 1 > MAX_MESSAGE_SIZE) return ERR_SERIALIZATION;
    
    memcpy(buffer->data + buffer->position, str, len);
    buffer->data[buffer->position + len] = '\0';
    buffer->position += max_len;  // Fixed size for alignment
    if (buffer->position > buffer->size) buffer->size = buffer->position;
    
    return SUCCESS;
}

error_code_t serialize_bytes(serialize_buffer_t* buffer, const void* data, size_t size) {
    if (!buffer || !data) return ERR_INVALID_PARAM;
    if (buffer->position + size > MAX_MESSAGE_SIZE) return ERR_SERIALIZATION;
    
    memcpy(buffer->data + buffer->position, data, size);
    buffer->position += size;
    if (buffer->position > buffer->size) buffer->size = buffer->position;
    
    return SUCCESS;
}

error_code_t deserialize_int32(serialize_buffer_t* buffer, int32_t* value) {
    if (!buffer || !value) return ERR_INVALID_PARAM;
    if (buffer->position + sizeof(int32_t) > (size_t)buffer->size) return ERR_SERIALIZATION;
    
    int32_t net_value;
    memcpy(&net_value, buffer->data + buffer->position, sizeof(int32_t));
    *value = ntohl(net_value);
    buffer->position += sizeof(int32_t);
    
    return SUCCESS;
}

error_code_t deserialize_uint32(serialize_buffer_t* buffer, uint32_t* value) {
    if (!buffer || !value) return ERR_INVALID_PARAM;
    if (buffer->position + sizeof(uint32_t) > (size_t)buffer->size) return ERR_SERIALIZATION;
    
    uint32_t net_value;
    memcpy(&net_value, buffer->data + buffer->position, sizeof(uint32_t));
    *value = ntohl(net_value);
    buffer->position += sizeof(uint32_t);
    
    return SUCCESS;
}

error_code_t deserialize_bool(serialize_buffer_t* buffer, bool* value) {
    if (!buffer || !value) return ERR_INVALID_PARAM;
    if (buffer->position + 1 > buffer->size) return ERR_SERIALIZATION;
    
    *value = buffer->data[buffer->position] != 0;
    buffer->position++;
    
    return SUCCESS;
}

error_code_t deserialize_string(serialize_buffer_t* buffer, char* str, size_t max_len) {
    if (!buffer || !str) return ERR_INVALID_PARAM;
    if (buffer->position + max_len > (size_t)buffer->size) return ERR_SERIALIZATION;
    
    memcpy(str, buffer->data + buffer->position, max_len);
    str[max_len - 1] = '\0';  // Ensure null termination
    buffer->position += max_len;
    
    return SUCCESS;
}

error_code_t deserialize_bytes(serialize_buffer_t* buffer, void* data, size_t size) {
    if (!buffer || !data) return ERR_INVALID_PARAM;
    if (buffer->position + size > (size_t)buffer->size) return ERR_SERIALIZATION;
    
    memcpy(data, buffer->data + buffer->position, size);
    buffer->position += size;
    
    return SUCCESS;
}

error_code_t serialize_header(serialize_buffer_t* buffer, const message_header_t* header) {
    if (!buffer || !header) return ERR_INVALID_PARAM;
    
    error_code_t err;
    if ((err = serialize_uint32(buffer, header->type)) != SUCCESS) return err;
    if ((err = serialize_uint32(buffer, header->length)) != SUCCESS) return err;
    if ((err = serialize_uint32(buffer, header->sequence)) != SUCCESS) return err;
    if ((err = serialize_uint32(buffer, header->reserved)) != SUCCESS) return err;
    
    return SUCCESS;
}

error_code_t deserialize_header(serialize_buffer_t* buffer, message_header_t* header) {
    if (!buffer || !header) return ERR_INVALID_PARAM;
    
    error_code_t err;
    if ((err = deserialize_uint32(buffer, &header->type)) != SUCCESS) return err;
    if ((err = deserialize_uint32(buffer, &header->length)) != SUCCESS) return err;
    if ((err = deserialize_uint32(buffer, &header->sequence)) != SUCCESS) return err;
    if ((err = deserialize_uint32(buffer, &header->reserved)) != SUCCESS) return err;
    
    return SUCCESS;
}

error_code_t serialize_message(message_type_t type, const void* payload, size_t payload_size, 
                               void* output, size_t* output_size) {
    if (!output || !output_size) return ERR_INVALID_PARAM;
    if (payload_size > MAX_PAYLOAD_SIZE) return ERR_SERIALIZATION;
    
    serialize_buffer_t buffer;
    serialize_buffer_init(&buffer);
    
    // Create header
    message_header_t header;
    header.type = type;
    header.length = payload_size;
    header.sequence = 0;
    header.reserved = 0;
    
    // Serialize header
    error_code_t err = serialize_header(&buffer, &header);
    if (err != SUCCESS) return err;
    
    // Serialize payload
    if (payload && payload_size > 0) {
        err = serialize_bytes(&buffer, payload, payload_size);
        if (err != SUCCESS) return err;
    }
    
    memcpy(output, buffer.data, buffer.size);
    *output_size = buffer.size;
    
    return SUCCESS;
}

error_code_t deserialize_message(const void* input, size_t input_size, 
                                 message_type_t* type, void* payload, size_t max_payload_size) {
    if (!input || !type) return ERR_INVALID_PARAM;
    if (input_size < HEADER_SIZE) return ERR_SERIALIZATION;
    
    serialize_buffer_t buffer;
    serialize_buffer_init(&buffer);
    memcpy(buffer.data, input, input_size);
    buffer.size = input_size;
    buffer.position = 0;
    
    // Deserialize header
    message_header_t header;
    error_code_t err = deserialize_header(&buffer, &header);
    if (err != SUCCESS) return err;
    
    *type = (message_type_t)header.type;
    
    // Deserialize payload
    if (header.length > 0) {
        if (!payload || header.length > max_payload_size) return ERR_SERIALIZATION;
        err = deserialize_bytes(&buffer, payload, header.length);
        if (err != SUCCESS) return err;
    }
    
    return SUCCESS;
}

// Message-specific serializers (simplified binary format)
error_code_t serialize_connect(serialize_buffer_t* buffer, const msg_connect_t* msg) {
    if (!buffer || !msg) return ERR_INVALID_PARAM;
    error_code_t err;
    if ((err = serialize_string(buffer, msg->pseudo, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    if ((err = serialize_string(buffer, msg->version, 16)) != SUCCESS) return err;
    return SUCCESS;
}

error_code_t deserialize_connect(serialize_buffer_t* buffer, msg_connect_t* msg) {
    if (!buffer || !msg) return ERR_INVALID_PARAM;
    error_code_t err;
    if ((err = deserialize_string(buffer, msg->pseudo, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    if ((err = deserialize_string(buffer, msg->version, 16)) != SUCCESS) return err;
    return SUCCESS;
}

error_code_t serialize_board_state(serialize_buffer_t* buffer, const msg_board_state_t* msg) {
    if (!buffer || !msg) return ERR_INVALID_PARAM;
    error_code_t err;
    
    if ((err = serialize_bool(buffer, msg->exists)) != SUCCESS) return err;
    if ((err = serialize_string(buffer, msg->game_id, MAX_GAME_ID_LEN)) != SUCCESS) return err;
    if ((err = serialize_string(buffer, msg->player_a, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    if ((err = serialize_string(buffer, msg->player_b, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    
    for (int i = 0; i < NUM_PITS; i++) {
        if ((err = serialize_int32(buffer, msg->pits[i])) != SUCCESS) return err;
    }
    
    if ((err = serialize_int32(buffer, msg->score_a)) != SUCCESS) return err;
    if ((err = serialize_int32(buffer, msg->score_b)) != SUCCESS) return err;
    if ((err = serialize_int32(buffer, msg->current_player)) != SUCCESS) return err;
    if ((err = serialize_int32(buffer, msg->state)) != SUCCESS) return err;
    if ((err = serialize_int32(buffer, msg->winner)) != SUCCESS) return err;
    
    return SUCCESS;
}

error_code_t deserialize_board_state(serialize_buffer_t* buffer, msg_board_state_t* msg) {
    if (!buffer || !msg) return ERR_INVALID_PARAM;
    error_code_t err;
    
    if ((err = deserialize_bool(buffer, &msg->exists)) != SUCCESS) return err;
    if ((err = deserialize_string(buffer, msg->game_id, MAX_GAME_ID_LEN)) != SUCCESS) return err;
    if ((err = deserialize_string(buffer, msg->player_a, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    if ((err = deserialize_string(buffer, msg->player_b, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    
    for (int i = 0; i < NUM_PITS; i++) {
        if ((err = deserialize_int32(buffer, &msg->pits[i])) != SUCCESS) return err;
    }
    
    if ((err = deserialize_int32(buffer, &msg->score_a)) != SUCCESS) return err;
    if ((err = deserialize_int32(buffer, &msg->score_b)) != SUCCESS) return err;
    if ((err = deserialize_int32(buffer, (int32_t*)&msg->current_player)) != SUCCESS) return err;
    if ((err = deserialize_int32(buffer, (int32_t*)&msg->state)) != SUCCESS) return err;
    if ((err = deserialize_int32(buffer, (int32_t*)&msg->winner)) != SUCCESS) return err;
    
    return SUCCESS;
}

error_code_t serialize_move(serialize_buffer_t* buffer, const msg_play_move_t* msg) {
    if (!buffer || !msg) return ERR_INVALID_PARAM;
    error_code_t err;
    
    if ((err = serialize_string(buffer, msg->game_id, MAX_GAME_ID_LEN)) != SUCCESS) return err;
    if ((err = serialize_string(buffer, msg->player, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    if ((err = serialize_int32(buffer, msg->pit_index)) != SUCCESS) return err;
    
    return SUCCESS;
}

error_code_t deserialize_move(serialize_buffer_t* buffer, msg_play_move_t* msg) {
    if (!buffer || !msg) return ERR_INVALID_PARAM;
    error_code_t err;
    
    if ((err = deserialize_string(buffer, msg->game_id, MAX_GAME_ID_LEN)) != SUCCESS) return err;
    if ((err = deserialize_string(buffer, msg->player, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    if ((err = deserialize_int32(buffer, &msg->pit_index)) != SUCCESS) return err;
    
    return SUCCESS;
}

error_code_t serialize_challenge(serialize_buffer_t* buffer, const msg_challenge_t* msg) {
    if (!buffer || !msg) return ERR_INVALID_PARAM;
    error_code_t err;
    
    if ((err = serialize_string(buffer, msg->challenger, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    if ((err = serialize_string(buffer, msg->opponent, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    
    return SUCCESS;
}

error_code_t deserialize_challenge(serialize_buffer_t* buffer, msg_challenge_t* msg) {
    if (!buffer || !msg) return ERR_INVALID_PARAM;
    error_code_t err;
    
    if ((err = deserialize_string(buffer, msg->challenger, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    if ((err = deserialize_string(buffer, msg->opponent, MAX_PSEUDO_LEN)) != SUCCESS) return err;
    
    return SUCCESS;
}
