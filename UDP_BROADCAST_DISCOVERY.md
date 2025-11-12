# UDP Broadcast Discovery - Implementation Summary

## Overview

The Awale client-server system now uses **automatic network discovery** via UDP broadcast, eliminating the need for manual IP address configuration.

## 🎯 Key Features

### Automatic Server Discovery
- Client broadcasts discovery request on local network (UDP port 12346)
- Server responds with its IP and discovery port
- **No manual IP configuration needed!**

### Peer-to-Peer Port Negotiation
- Client finds a free port and tells server: "I'm listening on port X"
- Server finds a free port and tells client: "I'm listening on port Y"
- **Each side controls its own listening port**

### Bidirectional Communication
- Client writes to server's port (client's write socket)
- Server writes to client's port (server's write socket)
- Each reads from their own listening port
- **True full-duplex communication**

## 🔄 Connection Flow

```
CLIENT                                 SERVER
  │                                        │
  │  1. UDP Broadcast: "AWALE_DISCOVERY"   │
  ├───────────────────────────────────────>│ (port 12346)
  │                                        │
  │  2. UDP Response: "AWALE_SERVER:12345" │
  │<───────────────────────────────────────┤
  │   (includes server IP + discovery port)|
  │                                     │
  │  3. TCP Connect to discovery port   │
  ├─────────────────────────────────────>│ (port 12345)
  │                                     │
  │  4. "I'm listening on port 54321"   │
  ├─────────────────────────────────────>│
  │                                     │
  │  5. "I'm listening on port 54322"   │
  │<─────────────────────────────────────┤
  │                                     │
  │  6. Close discovery connection      │
  ├─────────────────────────────────────┤
  │                                     │
  │  7. Server connects to 54321 ───────>│
  │     (client's read socket)          │
  │                                     │
  │  8. Client connects to 54322 ───────>│
  │     (server's read socket)          │
  │                                     │
  │  9. Bidirectional communication     │
  │<═══════════════════════════════════>│
```

## 📦 New Functions

### Client-Side: `connection_broadcast_discovery()`

```c
error_code_t connection_broadcast_discovery(discovery_response_t* response, int timeout_sec);
```

**Purpose:** Broadcasts UDP discovery request and waits for server response.

**Parameters:**
- `response`: Structure to fill with server IP and port
- `timeout_sec`: How long to wait for response (e.g., 5 seconds)

**Returns:** `SUCCESS` if server found, `ERR_NETWORK_ERROR` if timeout

**Example:**
```c
discovery_response_t discovery;
if (connection_broadcast_discovery(&discovery, 5) == SUCCESS) {
    printf("Found server at %s:%d\n", 
           discovery.server_ip, discovery.discovery_port);
}
```

### Server-Side: `connection_listen_for_discovery()`

```c
error_code_t connection_listen_for_discovery(int discovery_port, int broadcast_port);
```

**Purpose:** Listens for UDP broadcast requests and responds with server info.

**Parameters:**
- `discovery_port`: TCP discovery port to advertise (e.g., 12345)
- `broadcast_port`: UDP port to listen on (e.g., 12346)

**Note:** Runs in infinite loop - use in separate thread!

**Example:**
```c
void* udp_discovery_thread(void* arg) {
    connection_listen_for_discovery(12345, 12346);
    return NULL;
}

pthread_t thread;
pthread_create(&thread, NULL, udp_discovery_thread, NULL);
pthread_detach(thread);
```

### Port Allocation: `connection_find_free_port()`

```c
error_code_t connection_find_free_port(int* port);
```

**Purpose:** Finds a single free port dynamically.

**Returns:** Port number in ephemeral range (typically 32768-60999)

**Example:**
```c
int my_port;
connection_find_free_port(&my_port);
printf("I'll listen on port %d\n", my_port);
```

## 📝 Updated Message Structure

### MSG_PORT_NEGOTIATION

```c
typedef struct {
    int32_t my_port;     /* Port that sender is listening on */
} msg_port_negotiation_t;
```

**Changed from:** Two ports (read_port, write_port)  
**Changed to:** One port (my_port)

**Why?** Each side manages its own listening port independently.

## 💻 Command-Line Usage

### Server
```bash
# Start server - UDP discovery enabled automatically
./build/awale_server

# Custom discovery port
./build/awale_server 8080

# Using Makefile
make run-server
```

**Output:**
```
╔══════════════════════════════════════════════════════╗
║         AWALE SERVER (New Architecture)              ║
╚══════════════════════════════════════════════════════╝
Discovery Port: 12345 (TCP)
Broadcast Port: 12346 (UDP)
✓ UDP broadcast discovery listening on port 12346
✓ Discovery server listening on port 12345
🎮 Server ready! Waiting for connections...
```

### Client
```bash
# Connect - server discovered automatically!
./build/awale_client Alice

# Using Makefile
make run-client PSEUDO=Alice
```

**Output:**
```
Player: Alice
🔍 Broadcasting discovery request on local network...
✓ Server discovered at 192.168.1.100:12345
✓ Connected to server
   Client will listen on port: 54321
   Server listening on port: 54322
   Connected to server's port
✓ Bidirectional connection established
```

## 🔧 Implementation Details

### Server Main Loop

```c
int main(int argc, char** argv) {
    g_discovery_port = 12345;
    
    /* Start UDP broadcast listener in background thread */
    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL, udp_discovery_thread, NULL);
    pthread_detach(udp_thread);
    
    /* Create TCP discovery server */
    connection_t discovery_server;
    connection_create_discovery_server(&discovery_server, g_discovery_port);
    
    while (running) {
        /* Accept discovery connection */
        connection_t disc_conn;
        connection_accept_discovery(&discovery_server, &disc_conn);
        
        /* Receive client's listening port */
        msg_port_negotiation_t client_msg;
        connection_recv_raw(&disc_conn, &client_msg, sizeof(client_msg), &received);
        int client_port = ntohl(client_msg.my_port);
        
        /* Find server's listening port */
        int server_port;
        connection_find_free_port(&server_port);
        
        /* Send server's listening port */
        msg_port_negotiation_t server_msg;
        server_msg.my_port = htonl(server_port);
        connection_send_raw(&disc_conn, &server_msg, sizeof(server_msg));
        
        /* Close discovery connection */
        connection_close(&disc_conn);
        
        /* Create server listening socket */
        connection_t server_listen;
        connection_create_discovery_server(&server_listen, server_port);
        
        /* Connect to client's listening port (write socket) */
        client_conn.write_sockfd = socket(...);
        connect(client_conn.write_sockfd, client_ip, client_port);
        
        /* Accept client's connection (read socket) */
        connection_accept_discovery(&server_listen, &temp_conn);
        client_conn.read_sockfd = temp_conn.read_sockfd;
        
        /* Bidirectional connection established! */
    }
}
```

### Client Main Flow

```c
int main(int argc, char** argv) {
    /* Step 1: Broadcast discovery */
    discovery_response_t discovery;
    connection_broadcast_discovery(&discovery, 5);
    
    /* Step 2: Connect to discovery port */
    connection_t disc_conn;
    // ... connect to discovery.server_ip:discovery.discovery_port
    
    /* Step 3: Find client's listening port */
    int client_port;
    connection_find_free_port(&client_port);
    
    /* Step 4: Send client's port to server */
    msg_port_negotiation_t client_msg;
    client_msg.my_port = htonl(client_port);
    connection_send_raw(&disc_conn, &client_msg, sizeof(client_msg));
    
    /* Step 5: Receive server's port */
    msg_port_negotiation_t server_msg;
    connection_recv_raw(&disc_conn, &server_msg, sizeof(server_msg), &received);
    int server_port = ntohl(server_msg.my_port);
    
    /* Step 6: Close discovery connection */
    connection_close(&disc_conn);
    
    /* Step 7: Create client listening socket */
    connection_t client_listen;
    connection_create_discovery_server(&client_listen, client_port);
    
    /* Step 8: Connect to server's listening port (write socket) */
    conn.write_sockfd = socket(...);
    connect(conn.write_sockfd, discovery.server_ip, server_port);
    
    /* Step 9: Accept server's connection (read socket) */
    connection_accept_discovery(&client_listen, &temp_conn);
    conn.read_sockfd = temp_conn.read_sockfd;
    
    /* Bidirectional connection established! */
}
```

## ✅ Benefits

1. **Zero Configuration:** No IP addresses needed - just run and play!
2. **Automatic Discovery:** Works on any local network (LAN, WiFi)
3. **Symmetric Design:** Client and server use same pattern (listen + connect)
4. **No Port Conflicts:** Each connection uses unique dynamically-allocated ports
5. **Firewall Friendly:** Uses standard ephemeral port range

## 🧪 Testing

**All 33 tests pass:**
```bash
make test
# Game Logic: 16/16 ✓
# Network:    17/17 ✓
```

**Manual Test:**
```bash
# Terminal 1: Start server
make run-server

# Terminal 2: Start client (no IP address!)
make run-client PSEUDO=Alice

# Terminal 3: Another client
make run-client PSEUDO=Bob
```

## 🔐 Network Requirements

- **UDP Port 12346:** Must be accessible for broadcast (server listening)
- **TCP Port 12345:** Discovery port (configurable)
- **Ephemeral Ports:** Client and server need ability to bind to dynamic ports
- **Same Network:** UDP broadcast only works on local network (same subnet)

## 📚 Files Modified

### Core Implementation
- `include/network/connection.h` - Added UDP broadcast functions
- `src/network/connection.c` - Implemented broadcast discovery
- `include/common/messages.h` - Updated msg_port_negotiation_t
- `src/server/main.c` - Implemented peer-to-peer negotiation (server side)
- `src/client/main.c` - Implemented broadcast discovery (client side)

### Build System
- `Makefile` - Updated run targets (client no longer needs IP)

### Documentation
- `CLEANUP_SUMMARY.md` - Updated usage examples
- `UDP_BROADCAST_DISCOVERY.md` - This file (new)

## 🚀 What's Next?

- [ ] Add mDNS/Bonjour support for service discovery
- [ ] Implement server list (multiple servers on network)
- [ ] Add manual IP entry as fallback option
- [ ] Support IPv6
- [ ] Add encryption for port negotiation
