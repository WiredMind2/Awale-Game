/* Connection Core Functions
 * Basic connection initialization, cleanup, and utility functions
 */

#include "../../include/network/connection.h"
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

error_code_t connection_init(connection_t* conn) {
    if (!conn) return ERR_INVALID_PARAM;
    
    conn->read_sockfd = -1;
    conn->write_sockfd = -1;
    memset(&conn->addr, 0, sizeof(conn->addr));
    conn->connected = false;
    conn->sequence = 0;
    
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

bool connection_is_connected(const connection_t* conn) {
    return conn && conn->connected && conn->read_sockfd >= 0 && conn->write_sockfd >= 0;
}

const char* connection_get_peer_ip(const connection_t* conn) {
    static char ip_str[INET_ADDRSTRLEN];
    if (!conn) return "unknown";
    
    inet_ntop(AF_INET, &conn->addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    return ip_str;
}
