/* UDP Broadcast Discovery and Port Negotiation
 * Handles server discovery via UDP broadcast and dynamic port allocation
 */

#include "../../include/network/connection.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>

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
    
    conn->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->socket_fd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    int opt = 1;
    setsockopt(conn->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    
    if (bind(conn->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(conn->socket_fd);
        conn->socket_fd = -1;
        return ERR_NETWORK_ERROR;
    }
    
    if (listen(conn->socket_fd, 5) < 0) {
        close(conn->socket_fd);
        conn->socket_fd = -1;
        return ERR_NETWORK_ERROR;
    }
    
    conn->addr = addr;
    conn->connected = true;
    return SUCCESS;
}

/* Accept connection on discovery server (single socket) */
error_code_t connection_accept_discovery(connection_t* server, connection_t* client) {
    if (!server || !client) return ERR_INVALID_PARAM;
    if (server->socket_fd < 0) return ERR_NETWORK_ERROR;
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_sockfd = accept(server->socket_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sockfd < 0) {
        return ERR_NETWORK_ERROR;
    }
    
    client->socket_fd = client_sockfd;
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
