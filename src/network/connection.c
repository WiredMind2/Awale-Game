#include "../../include/network/connection.h"
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

error_code_t connection_init(connection_t* conn) {
    if (!conn) return ERR_INVALID_PARAM;
    
    conn->sockfd = -1;
    memset(&conn->addr, 0, sizeof(conn->addr));
    conn->connected = false;
    conn->sequence = 0;
    
    return SUCCESS;
}

error_code_t connection_create_server(connection_t* conn, int port) {
    if (!conn) return ERR_INVALID_PARAM;
    
    conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(conn->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    conn->addr.sin_port = htons(port);
    
    if (bind(conn->sockfd, (struct sockaddr*)&conn->addr, sizeof(conn->addr)) < 0) {
        close(conn->sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    // Listen
    if (listen(conn->sockfd, 5) < 0) {
        close(conn->sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    conn->connected = true;
    return SUCCESS;
}

error_code_t connection_connect(connection_t* conn, const char* host, int port) {
    if (!conn || !host) return ERR_INVALID_PARAM;
    
    conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    struct hostent* server = gethostbyname(host);
    if (!server) {
        close(conn->sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    memset(&conn->addr, 0, sizeof(conn->addr));
    conn->addr.sin_family = AF_INET;
    memcpy(&conn->addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    conn->addr.sin_port = htons(port);
    
    if (connect(conn->sockfd, (struct sockaddr*)&conn->addr, sizeof(conn->addr)) < 0) {
        close(conn->sockfd);
        return ERR_NETWORK_ERROR;
    }
    
    conn->connected = true;
    return SUCCESS;
}

error_code_t connection_accept(connection_t* server, connection_t* client) {
    if (!server || !client) return ERR_INVALID_PARAM;
    
    socklen_t addr_len = sizeof(client->addr);
    client->sockfd = accept(server->sockfd, (struct sockaddr*)&client->addr, &addr_len);
    
    if (client->sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    client->connected = true;
    client->sequence = 0;
    
    return SUCCESS;
}

error_code_t connection_close(connection_t* conn) {
    if (!conn) return ERR_INVALID_PARAM;
    
    if (conn->sockfd >= 0) {
        close(conn->sockfd);
        conn->sockfd = -1;
    }
    
    conn->connected = false;
    return SUCCESS;
}

error_code_t connection_send_raw(connection_t* conn, const void* data, size_t size) {
    if (!conn || !data || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    ssize_t sent = write(conn->sockfd, data, size);
    if (sent < 0 || (size_t)sent != size) {
        return ERR_NETWORK_ERROR;
    }
    
    return SUCCESS;
}

error_code_t connection_recv_raw(connection_t* conn, void* buffer, size_t size, size_t* received) {
    if (!conn || !buffer || size == 0) return ERR_INVALID_PARAM;
    if (!conn->connected) return ERR_NETWORK_ERROR;
    
    ssize_t n = read(conn->sockfd, buffer, size);
    if (n < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    if (received) *received = n;
    return SUCCESS;
}

error_code_t connection_send_timeout(connection_t* conn, const void* data, size_t size, int timeout_ms) {
    // Simplified: just use regular send for now
    (void)timeout_ms;  // Suppress unused parameter warning
    return connection_send_raw(conn, data, size);
}

error_code_t connection_recv_timeout(connection_t* conn, void* buffer, size_t size, size_t* received, int timeout_ms) {
    // Simplified: just use regular recv for now
    (void)timeout_ms;  // Suppress unused parameter warning
    return connection_recv_raw(conn, buffer, size, received);
}

bool connection_is_connected(const connection_t* conn) {
    return conn && conn->connected && conn->sockfd >= 0;
}

const char* connection_get_peer_ip(const connection_t* conn) {
    static char ip_str[INET_ADDRSTRLEN];
    if (!conn) return "unknown";
    
    inet_ntop(AF_INET, &conn->addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    return ip_str;
}
