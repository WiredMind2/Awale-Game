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
#include <fcntl.h>
#include <netinet/tcp.h>

error_code_t connection_create_server(connection_t* conn, int port) {
    if (!conn) return ERR_INVALID_PARAM;
    
    /* Create server socket */
    conn->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->socket_fd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    int opt = 1;
    setsockopt(conn->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    
    if (bind(conn->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(conn->socket_fd);
        return ERR_NETWORK_ERROR;
    }
    
    if (listen(conn->socket_fd, 5) < 0) {
        close(conn->socket_fd);
        return ERR_NETWORK_ERROR;
    }
    
    conn->addr = server_addr;
    conn->connected = true;
    return SUCCESS;
}

error_code_t connection_connect(connection_t* conn, const char* host, int port) {
    if (!conn || !host) return ERR_INVALID_PARAM;
    
    struct hostent* server = gethostbyname(host);
    if (!server) {
        return ERR_NETWORK_ERROR;
    }
    
    /* Create and connect socket */
    conn->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->socket_fd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_addr.sin_port = htons(port);
    
    if (connect(conn->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(conn->socket_fd);
        return ERR_NETWORK_ERROR;
    }
    
    conn->addr = server_addr;
    conn->connected = true;
    
    /* Enable TCP keepalive to detect broken connections */
    connection_enable_keepalive(conn);
    
    return SUCCESS;
}

error_code_t connection_accept(connection_t* server, connection_t* client) {
    if (!server || !client) return ERR_INVALID_PARAM;
    
    socklen_t addr_len = sizeof(client->addr);
    
    /* Accept connection */
    client->socket_fd = accept(server->socket_fd, (struct sockaddr*)&client->addr, &addr_len);
    if (client->socket_fd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    client->connected = true;
    client->sequence = 0;
    
    /* Enable TCP keepalive to detect broken connections */
    connection_enable_keepalive(client);
    
    return SUCCESS;
}

error_code_t connection_send_raw(connection_t* conn, const void* data, size_t size) {
    if (!conn || !data || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    const char* ptr = (const char*)data;
    size_t remaining = size;
    while (remaining > 0) {
        ssize_t sent = send(conn->socket_fd, ptr, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            /* Detect broken connection */
            if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EBADF) {
                conn->connected = false;
            }
            return ERR_NETWORK_ERROR;
        }
        if (sent == 0) {
            /* Peer closed connection */
            conn->connected = false;
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
        ssize_t n = read(conn->socket_fd, ptr, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            /* Detect broken connection */
            if (errno == ECONNRESET || errno == ENOTCONN || errno == EBADF) {
                conn->connected = false;
            }
            if (received) *received = total;
            return ERR_NETWORK_ERROR;
        }
        if (n == 0) {
            /* EOF / peer closed connection before we got expected bytes */
            conn->connected = false;
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
    if (!conn || !data || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    const char* ptr = (const char*)data;
    size_t remaining = size;
    
    while (remaining > 0) {
        /* Use select() to wait for socket to be writable */
        fd_set write_fds;
        struct timeval timeout;
        
        FD_ZERO(&write_fds);
        FD_SET(conn->socket_fd, &write_fds);
        
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        int ret = select(conn->socket_fd + 1, NULL, &write_fds, NULL, &timeout);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            conn->connected = false;
            return ERR_NETWORK_ERROR;
        }
        
        if (ret == 0) {
            /* Timeout - socket not ready for writing */
            return ERR_TIMEOUT;
        }
        
        /* Socket is writable, attempt to send */
        ssize_t sent = send(conn->socket_fd, ptr, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;  /* Retry */
            /* Detect broken connection */
            if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EBADF) {
                conn->connected = false;
            }
            return ERR_NETWORK_ERROR;
        }
        if (sent == 0) {
            conn->connected = false;
            return ERR_NETWORK_ERROR;
        }
        
        ptr += sent;
        remaining -= (size_t)sent;
    }
    
    return SUCCESS;
}

error_code_t connection_recv_timeout(connection_t* conn, void* buffer, size_t size, size_t* received, int timeout_ms) {
    if (!conn || !buffer || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    /* Set up select() with timeout */
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(conn->socket_fd, &read_fds);
    
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ret = select(conn->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    
    if (ret < 0) {
        if (errno == EINTR) return ERR_NETWORK_ERROR;
        conn->connected = false;  /* Mark as disconnected */
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
        ssize_t n = read(conn->socket_fd, ptr, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            /* Network error - mark connection as closed */
            conn->connected = false;
            if (received) *received = total;
            return ERR_NETWORK_ERROR;
        }
        if (n == 0) {
            /* EOF / peer closed connection */
            conn->connected = false;
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

error_code_t connection_recv_peek(connection_t* conn, void* buffer, size_t size, size_t* received, int timeout_ms) {
    if (!conn || !buffer || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;

    /* Set up select() with timeout */
    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(conn->socket_fd, &read_fds);

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(conn->socket_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (ret < 0) {
        if (errno == EINTR) return ERR_NETWORK_ERROR;
        conn->connected = false;  /* Mark as disconnected */
        return ERR_NETWORK_ERROR;
    }

    if (ret == 0) {
        /* Timeout occurred */
        return ERR_TIMEOUT;
    }

    /* Data is available, proceed with peek */
    char* ptr = (char*)buffer;
    size_t remaining = size;
    size_t total = 0;

    while (remaining > 0) {
        ssize_t n = recv(conn->socket_fd, ptr, remaining, MSG_PEEK);
        if (n < 0) {
            if (errno == EINTR) continue;
            /* Network error - mark connection as closed */
            conn->connected = false;
            if (received) *received = total;
            return ERR_NETWORK_ERROR;
        }
        if (n == 0) {
            /* EOF / peer closed connection */
            conn->connected = false;
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

/* Enable TCP keepalive to detect broken connections */
error_code_t connection_enable_keepalive(connection_t* conn) {
    if (!conn || conn->socket_fd < 0) return ERR_INVALID_PARAM;
    
    int keepalive = 1;
    if (setsockopt(conn->socket_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    /* Set keepalive parameters (Linux-specific) */
    int keepidle = 30;   /* Start keepalive probes after 30 seconds of idle */
    int keepintvl = 10;  /* Send probes every 10 seconds */
    int keepcnt = 3;     /* Drop connection after 3 failed probes */
    
    setsockopt(conn->socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(conn->socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    setsockopt(conn->socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
    
    return SUCCESS;
}

/* Set socket to non-blocking or blocking mode */
error_code_t connection_set_nonblocking(connection_t* conn, bool enable) {
    if (!conn || conn->socket_fd < 0) return ERR_INVALID_PARAM;
    
    int flags = fcntl(conn->socket_fd, F_GETFL, 0);
    if (flags < 0) return ERR_NETWORK_ERROR;
    
    if (enable) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(conn->socket_fd, F_SETFL, flags) < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    return SUCCESS;
}

/* Check if connection is still alive by sending TCP probe */
error_code_t connection_check_alive(connection_t* conn) {
    if (!conn || conn->socket_fd < 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    /* Use send() with MSG_NOSIGNAL and zero-length to check connection */
    /* This doesn't actually send data but checks if connection is alive */
    char dummy = 0;
    ssize_t result = send(conn->socket_fd, &dummy, 0, MSG_NOSIGNAL | MSG_DONTWAIT);
    
    if (result < 0) {
        if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EBADF) {
            /* Connection is broken */
            conn->connected = false;
            return ERR_NETWORK_ERROR;
        }
        /* EAGAIN/EWOULDBLOCK is OK for non-blocking check */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SUCCESS;
        }
    }
    
    return SUCCESS;
}
