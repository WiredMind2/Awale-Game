#include "../../include/network/connection.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

error_code_t connection_init(connection_t* conn) {
    if (!conn) return ERR_INVALID_PARAM;
    
    conn->read_sockfd = -1;
    conn->write_sockfd = -1;
    memset(&conn->addr, 0, sizeof(conn->addr));
    conn->connected = false;
    conn->sequence = 0;
    
    return SUCCESS;
}

error_code_t connection_create_server(connection_t* conn, int read_port, int write_port) {
    if (!conn) return ERR_INVALID_PARAM;
    
    /* Create read socket */
    conn->read_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->read_sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    int opt = 1;
    setsockopt(conn->read_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in read_addr;
    memset(&read_addr, 0, sizeof(read_addr));
    read_addr.sin_family = AF_INET;
    read_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    read_addr.sin_port = htons(read_port);
    
    if (bind(conn->read_sockfd, (struct sockaddr*)&read_addr, sizeof(read_addr)) < 0) {
        close(conn->read_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    if (listen(conn->read_sockfd, 5) < 0) {
        close(conn->read_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    /* Create write socket */
    conn->write_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->write_sockfd < 0) {
        close(conn->read_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    setsockopt(conn->write_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in write_addr;
    memset(&write_addr, 0, sizeof(write_addr));
    write_addr.sin_family = AF_INET;
    write_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    write_addr.sin_port = htons(write_port);
    
    if (bind(conn->write_sockfd, (struct sockaddr*)&write_addr, sizeof(write_addr)) < 0) {
        close(conn->read_sockfd);
        close(conn->write_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    if (listen(conn->write_sockfd, 5) < 0) {
        close(conn->read_sockfd);
        close(conn->write_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    conn->addr = read_addr;
    conn->connected = true;
    return SUCCESS;
}

error_code_t connection_connect(connection_t* conn, const char* host, int read_port, int write_port) {
    if (!conn || !host) return ERR_INVALID_PARAM;
    
    struct hostent* server = gethostbyname(host);
    if (!server) {
        return ERR_NETWORK_ERROR;
    }
    
    /* Create and connect read socket (client reads from server's write port) */
    conn->read_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->read_sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    struct sockaddr_in read_addr;
    memset(&read_addr, 0, sizeof(read_addr));
    read_addr.sin_family = AF_INET;
    memcpy(&read_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    read_addr.sin_port = htons(write_port); /* Connect to server's write port */
    
    if (connect(conn->read_sockfd, (struct sockaddr*)&read_addr, sizeof(read_addr)) < 0) {
        close(conn->read_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    /* Create and connect write socket (client writes to server's read port) */
    conn->write_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->write_sockfd < 0) {
        close(conn->read_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    struct sockaddr_in write_addr;
    memset(&write_addr, 0, sizeof(write_addr));
    write_addr.sin_family = AF_INET;
    memcpy(&write_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    write_addr.sin_port = htons(read_port); /* Connect to server's read port */
    
    if (connect(conn->write_sockfd, (struct sockaddr*)&write_addr, sizeof(write_addr)) < 0) {
        close(conn->read_sockfd);
        close(conn->write_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    conn->addr = read_addr;
    conn->connected = true;
    return SUCCESS;
}

error_code_t connection_accept(connection_t* server_read, connection_t* server_write, connection_t* client) {
    if (!server_read || !server_write || !client) return ERR_INVALID_PARAM;
    
    socklen_t addr_len = sizeof(client->addr);
    
    /* Accept connection on read socket */
    client->read_sockfd = accept(server_read->read_sockfd, (struct sockaddr*)&client->addr, &addr_len);
    if (client->read_sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    /* Accept connection on write socket */
    client->write_sockfd = accept(server_write->write_sockfd, NULL, NULL);
    if (client->write_sockfd < 0) {
        close(client->read_sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    client->connected = true;
    client->sequence = 0;
    
    return SUCCESS;
}

error_code_t connection_close(connection_t* conn) {
    if (!conn) return ERR_INVALID_PARAM;
    
    /* Close read socket */
    if (conn->read_sockfd >= 0) {
        close(conn->read_sockfd);
        conn->read_sockfd = -1;
    }
    
    /* Close write socket only if it's different from read socket */
    if (conn->write_sockfd >= 0 && conn->write_sockfd != conn->read_sockfd) {
        close(conn->write_sockfd);
    }
    conn->write_sockfd = -1;
    
    conn->connected = false;
    return SUCCESS;
}

error_code_t connection_send_raw(connection_t* conn, const void* data, size_t size) {
    if (!conn || !data || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    const char* ptr = (const char*)data;
    size_t remaining = size;
    while (remaining > 0) {
        ssize_t sent = write(conn->write_sockfd, ptr, remaining);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return ERR_NETWORK_ERROR;
        }
        if (sent == 0) {
            /* Peer closed connection */
            return ERR_NETWORK_ERROR;
        }
        ptr += sent;
        remaining -= (size_t)sent;
    }
    
    return SUCCESS;
}

error_code_t connection_recv_raw(connection_t* conn, void* buffer, size_t size, size_t* received) {
    if (!conn || !buffer || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    char* ptr = (char*)buffer;
    size_t remaining = size;
    size_t total = 0;

    while (remaining > 0) {
        ssize_t n = read(conn->read_sockfd, ptr, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            return ERR_NETWORK_ERROR;
        }
        if (n == 0) {
            /* EOF / peer closed connection before we got expected bytes */
            if (received) *received = total;
            return ERR_NETWORK_ERROR;
        }
        ptr += n;
        remaining -= (size_t)n;
        total += (size_t)n;
    }

    if (received) *received = total;
    return SUCCESS;
}

error_code_t connection_send_timeout(connection_t* conn, const void* data, size_t size, int timeout_ms) {
    // Simplified: just use regular send for now
    (void)timeout_ms;  // Suppress unused parameter warning
    return connection_send_raw(conn, data, size);
}

error_code_t connection_recv_timeout(connection_t* conn, void* buffer, size_t size, size_t* received, int timeout_ms) {
    if (!conn || !buffer || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    /* Set up select() with timeout */
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(conn->read_sockfd, &read_fds);
    
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ret = select(conn->read_sockfd + 1, &read_fds, NULL, NULL, &timeout);
    
    if (ret < 0) {
        if (errno == EINTR) return ERR_NETWORK_ERROR;
        return ERR_NETWORK_ERROR;
    }
    
    if (ret == 0) {
        /* Timeout occurred */
        return ERR_TIMEOUT;
    }
    
    /* Data is available, proceed with read */
    char* ptr = (char*)buffer;
    size_t remaining = size;
    size_t total = 0;

    while (remaining > 0) {
        ssize_t n = read(conn->read_sockfd, ptr, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            return ERR_NETWORK_ERROR;
        }
        if (n == 0) {
            /* EOF / peer closed connection */
            if (received) *received = total;
            return ERR_NETWORK_ERROR;
        }
        ptr += n;
        remaining -= (size_t)n;
        total += (size_t)n;
    }

    if (received) *received = total;
    return SUCCESS;
}

bool connection_is_connected(const connection_t* conn) {
    return conn && conn->connected && conn->read_sockfd >= 0 && conn->write_sockfd >= 0;
}

const char* connection_get_peer_ip(const connection_t* conn) {
    static char ip_str[INET_ADDRSTRLEN];
    if (!conn) return "unknown";
    
    inet_ntop(AF_INET, &conn->addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    return ip_str;
}

/* ========== Select-based Multiplexing ========== */

error_code_t select_context_init(select_context_t* ctx, int timeout_sec, int timeout_usec) {
    if (!ctx) return ERR_INVALID_PARAM;
    
    FD_ZERO(&ctx->read_fds);
    FD_ZERO(&ctx->write_fds);
    FD_ZERO(&ctx->except_fds);
    ctx->max_fd = -1;
    ctx->timeout.tv_sec = timeout_sec;
    ctx->timeout.tv_usec = timeout_usec;
    
    return SUCCESS;
}

void select_context_add_read(select_context_t* ctx, int fd) {
    if (!ctx || fd < 0) return;
    
    FD_SET(fd, &ctx->read_fds);
    if (fd > ctx->max_fd) {
        ctx->max_fd = fd;
    }
}

void select_context_add_write(select_context_t* ctx, int fd) {
    if (!ctx || fd < 0) return;
    
    FD_SET(fd, &ctx->write_fds);
    if (fd > ctx->max_fd) {
        ctx->max_fd = fd;
    }
}

bool select_context_is_readable(select_context_t* ctx, int fd) {
    if (!ctx || fd < 0) return false;
    return FD_ISSET(fd, &ctx->read_fds);
}

bool select_context_is_writable(select_context_t* ctx, int fd) {
    if (!ctx || fd < 0) return false;
    return FD_ISSET(fd, &ctx->write_fds);
}

error_code_t select_wait(select_context_t* ctx, int* ready_count) {
    if (!ctx) return ERR_INVALID_PARAM;
    
    fd_set read_fds = ctx->read_fds;
    fd_set write_fds = ctx->write_fds;
    fd_set except_fds = ctx->except_fds;
    struct timeval timeout = ctx->timeout;
    
    int result = select(ctx->max_fd + 1, &read_fds, &write_fds, &except_fds, &timeout);
    
    if (result < 0) {
        if (errno == EINTR) {
            if (ready_count) *ready_count = 0;
            return SUCCESS;
        }
        return ERR_NETWORK_ERROR;
    }
    
    /* Update the sets with the results */
    ctx->read_fds = read_fds;
    ctx->write_fds = write_fds;
    ctx->except_fds = except_fds;
    
    if (ready_count) *ready_count = result;
    return SUCCESS;
}

/* Find a single free port */
error_code_t connection_find_free_port(int* port) {
    if (!port) return ERR_INVALID_PARAM;
    
    int test_sock = -1;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    /* Create socket and bind to any port */
    test_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (test_sock < 0) return ERR_NETWORK_ERROR;
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;  /* Let OS choose */
    
    if (bind(test_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(test_sock);
        return ERR_NETWORK_ERROR;
    }
    
    if (getsockname(test_sock, (struct sockaddr*)&addr, &addr_len) < 0) {
        close(test_sock);
        return ERR_NETWORK_ERROR;
    }
    
    *port = ntohs(addr.sin_port);
    
    /* Close test socket - port will be reused immediately */
    close(test_sock);
    
    return SUCCESS;
}

/* Create a single-socket discovery server (traditional server) */
error_code_t connection_create_discovery_server(connection_t* conn, int port) {
    if (!conn) return ERR_INVALID_PARAM;
    
    conn->read_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->read_sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    int opt = 1;
    setsockopt(conn->read_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    
    if (bind(conn->read_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(conn->read_sockfd);
        conn->read_sockfd = -1;
        return ERR_NETWORK_ERROR;
    }
    
    if (listen(conn->read_sockfd, 5) < 0) {
        close(conn->read_sockfd);
        conn->read_sockfd = -1;
        return ERR_NETWORK_ERROR;
    }
    
    conn->write_sockfd = -1;  /* Not used in discovery phase */
    conn->connected = true;
    
    return SUCCESS;
}

/* Accept connection on discovery server (single socket) */
error_code_t connection_accept_discovery(connection_t* server, connection_t* client) {
    if (!server || !client) return ERR_INVALID_PARAM;
    if (server->read_sockfd < 0) return ERR_NETWORK_ERROR;
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_sockfd = accept(server->read_sockfd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    client->read_sockfd = client_sockfd;
    client->write_sockfd = client_sockfd;  /* Same socket for both directions in discovery */
    client->addr = client_addr;
    client->connected = true;
    client->sequence = 0;
    
    return SUCCESS;
}

/* Client: Broadcast UDP discovery request and wait for server response */
error_code_t connection_broadcast_discovery(discovery_response_t* response, int timeout_sec) {
    if (!response) return ERR_INVALID_PARAM;
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return ERR_NETWORK_ERROR;
    
    /* Enable broadcast */
    int broadcast_enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        close(sock);
        return ERR_NETWORK_ERROR;
    }
    
    /* Set receive timeout */
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    /* Broadcast on port 12346 */
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(12346);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    
    /* Send discovery request */
    const char* request = "AWALE_DISCOVERY";
    if (sendto(sock, request, strlen(request), 0, 
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        close(sock);
        return ERR_NETWORK_ERROR;
    }
    
    /* Wait for response */
    char buffer[128];
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    
    ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr*)&server_addr, &addr_len);
    
    if (received < 0) {
        close(sock);
        return ERR_NETWORK_ERROR;  /* Timeout or error */
    }
    
    buffer[received] = '\0';
    
    /* Parse response: "AWALE_SERVER:<port>" */
    if (strncmp(buffer, "AWALE_SERVER:", 13) == 0) {
        response->discovery_port = atoi(buffer + 13);
        inet_ntop(AF_INET, &server_addr.sin_addr, response->server_ip, sizeof(response->server_ip));
        close(sock);
        return SUCCESS;
    }
    
    close(sock);
    return ERR_NETWORK_ERROR;
}

/* Server: Listen for UDP broadcast discovery requests */
error_code_t connection_listen_for_discovery(int discovery_port, int broadcast_port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return ERR_NETWORK_ERROR;
    
    /* Enable broadcast */
    int broadcast_enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
    
    /* Bind to broadcast port */
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(broadcast_port);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        close(sock);
        return ERR_NETWORK_ERROR;
    }
    
    /* Listen for discovery requests in a loop */
    while (1) {
        char buffer[128];
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                    (struct sockaddr*)&client_addr, &addr_len);
        
        if (received > 0) {
            buffer[received] = '\0';
            
            /* Check if it's a discovery request */
            if (strcmp(buffer, "AWALE_DISCOVERY") == 0) {
                /* Send response with discovery port */
                char response[64];
                snprintf(response, sizeof(response), "AWALE_SERVER:%d", discovery_port);
                
                sendto(sock, response, strlen(response), 0,
                       (struct sockaddr*)&client_addr, addr_len);
            }
        }
    }
    
    close(sock);
    return SUCCESS;
}
