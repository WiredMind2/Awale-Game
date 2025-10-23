#ifndef CONNECTION_H
#define CONNECTION_H

#include "../common/types.h"
#include "../common/protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

/* Bidirectional connection structure with dual sockets */
typedef struct {
    int read_sockfd;    /* Socket for reading data */
    int write_sockfd;   /* Socket for writing data */
    struct sockaddr_in addr;
    bool connected;
    uint32_t sequence;  /* Message sequence counter */
} connection_t;

/* Connection management - All functions now use bidirectional sockets */
error_code_t connection_init(connection_t* conn);
error_code_t connection_create_server(connection_t* conn, int read_port, int write_port);
error_code_t connection_connect(connection_t* conn, const char* host, int read_port, int write_port);
error_code_t connection_accept(connection_t* server_read, connection_t* server_write, connection_t* client);
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

/* Port discovery for bidirectional negotiation */
error_code_t connection_find_free_port(int* port);
error_code_t connection_create_discovery_server(connection_t* conn, int port);
error_code_t connection_accept_discovery(connection_t* server, connection_t* client);

/* UDP broadcast discovery */
typedef struct {
    char server_ip[64];
    int discovery_port;
} discovery_response_t;

error_code_t connection_broadcast_discovery(discovery_response_t* response, int timeout_sec);
error_code_t connection_listen_for_discovery(int discovery_port, int broadcast_port);

/* Select-based multiplexing */
typedef struct {
    fd_set read_fds;
    fd_set write_fds;
    fd_set except_fds;
    int max_fd;
    struct timeval timeout;
} select_context_t;

error_code_t select_context_init(select_context_t* ctx, int timeout_sec, int timeout_usec);
void select_context_add_read(select_context_t* ctx, int fd);
void select_context_add_write(select_context_t* ctx, int fd);
bool select_context_is_readable(select_context_t* ctx, int fd);
bool select_context_is_writable(select_context_t* ctx, int fd);
error_code_t select_wait(select_context_t* ctx, int* ready_count);

#endif /* CONNECTION_H */
