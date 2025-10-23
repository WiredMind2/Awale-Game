/* TCP Connection Operations
 * Handles TCP socket creation, connection, accepting, and data transfer
 */

#include "../../include/network/connection.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/select.h>

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
