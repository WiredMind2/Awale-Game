#ifndef CONNECTION_H
#define CONNECTION_H

#include "../common/types.h"
#include "../common/protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>

/* Connection structure */
typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    bool connected;
    uint32_t sequence;  /* Message sequence counter */
} connection_t;

/* Connection management */
error_code_t connection_init(connection_t* conn);
error_code_t connection_create_server(connection_t* conn, int port);
error_code_t connection_connect(connection_t* conn, const char* host, int port);
error_code_t connection_accept(connection_t* server, connection_t* client);
error_code_t connection_close(connection_t* conn);

/* Send/receive raw data */
error_code_t connection_send_raw(connection_t* conn, const void* data, size_t size);
error_code_t connection_recv_raw(connection_t* conn, void* buffer, size_t size, size_t* received);

/* Send/receive with timeout */
error_code_t connection_send_timeout(connection_t* conn, const void* data, size_t size, int timeout_ms);
error_code_t connection_recv_timeout(connection_t* conn, void* buffer, size_t size, size_t* received, int timeout_ms);

/* Connection state */
bool connection_is_connected(const connection_t* conn);
const char* connection_get_peer_ip(const connection_t* conn);

#endif /* CONNECTION_H */
