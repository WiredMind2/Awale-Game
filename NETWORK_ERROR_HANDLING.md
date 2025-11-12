# Network Error Handling - Comprehensive Guide

## Overview

This document describes the comprehensive network error handling mechanisms implemented in the Awale game to eliminate freezing, detect broken connections early, and provide graceful recovery from all network failures.

## Key Improvements

### 1. **TCP Keepalive for Early Detection**

**Problem:** Broken connections (network cable unplugged, router failure, peer crash) are not detected until the next read/write attempt, which can take minutes.

**Solution:** TCP keepalive probes detect broken connections automatically.

**Implementation:**
```c
error_code_t connection_enable_keepalive(connection_t* conn);
```

**Parameters:**
- Keepalive idle: 30 seconds (starts probing after 30s of idle)
- Keepalive interval: 10 seconds (sends probe every 10s)
- Keepalive count: 3 probes (drops connection after 3 failed probes)

**Result:** Broken connections detected within ~60 seconds maximum, even if no traffic.

**Automatically enabled on:**
- All client connections to server
- All server accepted connections from clients

---

### 2. **Non-Blocking Send with Timeout**

**Problem:** `send()` can block indefinitely if the peer stops reading (full TCP buffer), causing application freeze.

**Solution:** Use `select()` to wait for socket writability with timeout before sending.

**Implementation:**
```c
error_code_t connection_send_timeout(connection_t* conn, const void* data, 
                                    size_t size, int timeout_ms);
```

**Behavior:**
- Waits up to `timeout_ms` for socket to become writable
- Returns `ERR_TIMEOUT` if socket not ready within timeout
- Returns `ERR_NETWORK_ERROR` if connection broken (EPIPE, ECONNRESET)
- Automatically marks connection as disconnected on error

**Used by:** `session_send_message()` with 5-second timeout

**Result:** Send operations never freeze indefinitely - timeout after 5 seconds maximum.

---

### 3. **Connection State Tracking**

**Problem:** After network error, connection object still appeared "connected" but was actually broken.

**Solution:** Automatically mark connection as disconnected when errors detected.

**Implementation:**
- All `send()`/`recv()` functions check errno for connection errors
- Detects: `EPIPE`, `ECONNRESET`, `ENOTCONN`, `EBADF`
- Immediately sets `conn->connected = false` on these errors
- All subsequent operations return `ERR_NETWORK_ERROR` immediately

**Result:** No repeated attempts on broken connections - fail fast.

---

### 4. **Connection Alive Check**

**Problem:** Need to proactively check if connection is still alive without waiting for read/write.

**Solution:** Non-blocking zero-length send probe.

**Implementation:**
```c
error_code_t connection_check_alive(connection_t* conn);
```

**Behavior:**
- Sends zero-length message with `MSG_NOSIGNAL | MSG_DONTWAIT`
- Returns `SUCCESS` if connection alive
- Returns `ERR_NETWORK_ERROR` if connection broken
- Does not actually transmit data (kernel-level check)

**Used by:** Server connection handler every 60 seconds

**Result:** Stale client connections detected proactively.

---

### 5. **Session Layer Validation**

**Problem:** Session layer didn't validate connection state before operations.

**Solution:** Check connection health at session entry points.

**Implementation:**

**Before Send:**
```c
error_code_t session_send_message(session_t* session, ...) {
    // Check connection before attempting send
    if (!connection_is_connected(&session->conn)) {
        session->authenticated = false;
        return ERR_NETWORK_ERROR;
    }
    
    // Use send with timeout (5 seconds)
    err = connection_send_timeout(&session->conn, buffer, size, 5000);
    
    // Mark session disconnected on error
    if (err == ERR_NETWORK_ERROR) {
        session->authenticated = false;
    }
    
    return err;
}
```

**Before Receive:**
```c
error_code_t session_recv_message_timeout(session_t* session, ...) {
    // Check connection before attempting receive
    if (!connection_is_connected(&session->conn)) {
        session->authenticated = false;
        return ERR_NETWORK_ERROR;
    }
    
    // Validate message header size
    if (header.length > MAX_PAYLOAD_SIZE) {
        session->authenticated = false;
        return ERR_SERIALIZATION;
    }
    
    // Mark session disconnected on any error
    if (err != SUCCESS) {
        session->authenticated = false;
    }
    
    return err;
}
```

**Result:** Invalid sessions rejected immediately, no cascading failures.

---

### 6. **Client Notification Listener Resilience**

**Problem:** Notification listener thread crashed on any network error, leaving client unresponsive.

**Solution:** Robust error handling with retry logic and graceful degradation.

**Implementation:**
```c
void* notification_listener(void* arg) {
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 3;
    
    while (client_state_is_running()) {
        err = session_recv_message_timeout(session, &type, payload, 
                                          MAX_PAYLOAD_SIZE, &size, 1000);
        
        if (err == ERR_TIMEOUT) {
            consecutive_errors = 0;  // Reset on normal timeout
            continue;
        }
        
        if (err != SUCCESS) {
            consecutive_errors++;
            
            if (err == ERR_NETWORK_ERROR) {
                // Fatal error - connection lost
                printf("Connection lost - server disconnected\n");
                break;
            }
            
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                printf("Too many consecutive errors - disconnecting\n");
                break;
            }
            
            // Brief delay before retry
            nanosleep(&(struct timespec){0, 500000000}, NULL);
            continue;
        }
        
        consecutive_errors = 0;  // Reset on success
        // Handle notification...
    }
}
```

**Features:**
- Distinguishes between fatal errors (connection lost) and transient errors
- Allows up to 3 consecutive errors before giving up
- 500ms delay between retries to avoid tight loop
- Resets error counter on successful receive

**Result:** Notification listener survives transient errors, exits gracefully on fatal errors.

---

### 7. **Client Play Mode Retry Logic**

**Problem:** Single timeout or packet loss caused play mode to fail or freeze.

**Solution:** Retry logic with maximum attempts.

**Implementation:**

**Board Request with Error Reporting:**
```c
static error_code_t request_board(const char* player_a, const char* player_b) {
    error_code_t err = session_send_message(...);
    
    if (err == ERR_NETWORK_ERROR) {
    printf("Failed to send board request - connection lost\n");
    } else if (err == ERR_TIMEOUT) {
    printf("Network slow - request timeout\n");
    }
    
    return err;
}
```

**Board Receive with Retry:**
```c
static error_code_t receive_board(msg_board_state_t* board) {
    int retries = 0;
    const int MAX_RETRIES = 2;
    
    while (retries <= MAX_RETRIES) {
        err = session_recv_message_timeout(..., 5000);
        
        if (err == ERR_TIMEOUT) {
            retries++;
            if (retries <= MAX_RETRIES) {
                printf("Server response timeout (attempt %d/%d) - retrying...\n", 
                       retries, MAX_RETRIES + 1);
                continue;
            }
            printf("Server not responding after %d attempts\n", MAX_RETRIES + 1);
            return err;
        }
        
        if (err == ERR_NETWORK_ERROR) {
            printf("Connection lost - please restart client\n");
            return err;  // Fatal - don't retry
        }
        
        // Success or other error
        return err;
    }
}
```

**Result:** Play mode survives single packet loss or timeout, exits gracefully on fatal errors.

---

### 8. **Server-Side Connection Monitoring**

**Problem:** Server kept threads alive for crashed/disconnected clients, wasting resources.

**Solution:** Periodic connection health checks on server side.

**Implementation:**
```c
void* client_handler(void* arg) {
    time_t last_check = time(NULL);
    const time_t CHECK_INTERVAL = 60;
    
    while (*g_running && session_is_active(&session)) {
        // Check connection health every 60 seconds
        time_t now = time(NULL);
        if (now - last_check >= CHECK_INTERVAL) {
            if (connection_check_alive(&session.conn) != SUCCESS) {
                printf("Client %s connection check failed - disconnecting\n", 
                       session.pseudo);
                break;
            }
            last_check = now;
        }
        
        // Receive with timeout to allow periodic checking
        err = session_recv_message_timeout(&session, ..., 5000);
        
        if (err == ERR_TIMEOUT) {
            continue;  // Normal timeout - check connection health on next iteration
        }
        
        if (err != SUCCESS) {
            // Handle error and exit
            break;
        }
        
        // Handle message...
    }
    
cleanup:
    // Clean up all resources for this client
    session_registry_remove(&session);
    matchmaking_remove_player(g_matchmaking, session.pseudo);
    
    // Remove from all games as spectator
    for (int i = 0; i < g_game_manager->game_count; i++) {
        if (g_game_manager->games[i].active) {
            game_manager_remove_spectator(g_game_manager, 
                                         g_game_manager->games[i].game_id, 
                                         session.pseudo);
        }
    }
    
    session_close(&session);
}
```

**Features:**
- Checks connection alive every 60 seconds
- Uses timeout on receive to allow periodic checking
- Comprehensive cleanup on disconnect (session, matchmaking, spectators)

**Result:** Dead clients detected and cleaned up automatically, no resource leaks.

---

## Error Code Distinctions

### `ERR_TIMEOUT`
**Meaning:** Operation timed out waiting for peer response.

**Causes:**
- Network delay (slow connection)
- Peer temporarily busy
- Single packet loss

**Recovery:** Retry operation

**Example:** Board state request times out → retry up to 3 times

---

### `ERR_NETWORK_ERROR`
**Meaning:** Fatal network error - connection is broken.

**Causes:**
- Peer disconnected (closed socket)
- Network cable unplugged
- Router/firewall blocked connection
- TCP reset received

**Recovery:** Exit gracefully, inform user to restart

**Example:** `send()` returns `EPIPE` → connection is dead, don't retry

---

## Timeout Values

| Operation | Timeout | Rationale |
|-----------|---------|-----------|
| `session_send_message()` | 5 seconds | Prevents freeze if peer stops reading |
| `session_recv_message_timeout()` | 1-5 seconds | Varies by context (1s for notifications, 5s for requests) |
| `connection_check_alive()` | Non-blocking | Immediate check, no wait |
| Server connection check interval | 60 seconds | Balance between responsiveness and overhead |
| TCP keepalive idle | 30 seconds | Standard value for detecting broken connections |
| TCP keepalive interval | 10 seconds | Standard probe interval |
| TCP keepalive count | 3 probes | Standard retry count (30s total) |

---

## Testing Scenarios

### Scenario 1: Client Crashes
**What happens:**
1. Client process killed
2. TCP connection closed immediately
3. Server `recv()` returns 0 (EOF)
4. Server marks connection as closed
5. Server thread exits and cleans up resources

**Result:** Server detects immediately, no resource leak

---

### Scenario 2: Network Cable Unplugged
**What happens:**
1. TCP connection remains "established" (no RST)
2. Next `send()`/`recv()` blocks indefinitely
3. **TCP keepalive** sends probes after 30s idle
4. No response after 3 probes (30s)
5. Connection marked as broken
6. Application detects via `connection_check_alive()` or next I/O

**Result:** Detected within ~60 seconds maximum

---

### Scenario 3: Slow Network (High Latency)
**What happens:**
1. `send()` completes (data in local buffer)
2. `recv()` blocks waiting for response
3. Timeout fires after 5 seconds
4. Returns `ERR_TIMEOUT`
5. Application retries (up to 3 times)
6. Either succeeds or gives up gracefully

**Result:** No freeze, graceful handling with user feedback

---

### Scenario 4: Peer Stops Reading (TCP Buffer Full)
**What happens:**
1. `send()` blocks (TCP buffer full)
2. `select()` waits for writability with 5s timeout
3. Timeout fires
4. Returns `ERR_TIMEOUT`
5. Application handles gracefully

**Result:** No freeze, timeout after 5 seconds

---

### Scenario 5: Rapid Network Errors (Flaky Connection)
**What happens:**
1. Multiple consecutive `recv()` errors
2. Notification listener counts consecutive errors
3. Retries with 500ms delay between attempts
4. After 3 consecutive errors, exits gracefully

**Result:** Survives transient errors, exits on persistent failure

---

## API Reference

### Connection Layer

```c
/* Enable TCP keepalive (called automatically on connect/accept) */
error_code_t connection_enable_keepalive(connection_t* conn);

/* Send data with timeout (prevent freeze) */
error_code_t connection_send_timeout(connection_t* conn, const void* data, 
                                    size_t size, int timeout_ms);

/* Receive data with timeout */
error_code_t connection_recv_timeout(connection_t* conn, void* buffer, 
                                    size_t size, size_t* received, int timeout_ms);

/* Check if connection is still alive (non-blocking probe) */
error_code_t connection_check_alive(connection_t* conn);

/* Check connection state */
bool connection_is_connected(const connection_t* conn);
```

### Session Layer

```c
/* Send message (uses timeout internally) */
error_code_t session_send_message(session_t* session, message_type_t type, 
                                  const void* payload, size_t payload_size);

/* Receive message with timeout */
error_code_t session_recv_message_timeout(session_t* session, message_type_t* type, 
                                         void* payload, size_t max_payload_size, 
                                         size_t* actual_size, int timeout_ms);

/* Check if session is active (authenticated + connected) */
bool session_is_active(const session_t* session);
```

---

## Best Practices

### For Application Code

1. **Always check return codes**
   ```c
   error_code_t err = session_send_message(...);
   if (err != SUCCESS) {
       // Handle error appropriately
   }
   ```

2. **Distinguish timeout from fatal errors**
   ```c
   if (err == ERR_TIMEOUT) {
       // Retry
   } else if (err == ERR_NETWORK_ERROR) {
       // Exit gracefully
   }
   ```

3. **Use timeouts on all blocking operations**
   ```c
   // Good
   session_recv_message_timeout(session, ..., 5000);
   
   // Bad - can freeze indefinitely
   session_recv_message(session, ...);
   ```

4. **Provide user feedback on errors**
   ```c
    printf("Server response timeout - retrying...\n");
    printf("Connection lost - please restart client\n");
   ```

5. **Clean up on exit**
   ```c
   if (err == ERR_NETWORK_ERROR) {
       session_close(&session);
       return;
   }
   ```

---

## Performance Impact

### TCP Keepalive
- **Overhead:** Negligible (<1% CPU)
- **Network traffic:** ~1 byte every 10 seconds during idle
- **Benefit:** Broken connections detected in ~60 seconds

### Connection Health Checks
- **Overhead:** One non-blocking send() per minute per client
- **Network traffic:** Zero (kernel-level check)
- **Benefit:** Proactive detection of stale connections

### Send/Receive Timeouts
- **Overhead:** One `select()` call per operation (~microseconds)
- **Latency added:** <1ms in normal operation
- **Benefit:** No freezing, guaranteed timeout

---

## Verification

### Build and Test
```bash
make clean && make
make test
```

### Manual Testing
1. **Client crash test:** Kill client with `Ctrl+C` → Server should detect immediately
2. **Network disconnect test:** Unplug cable → Detect within 60 seconds
3. **Slow network test:** Add latency with `tc` → Should timeout gracefully
4. **Concurrent clients test:** Multiple clients → All remain responsive

---

## Related Documents

- `FREEZE_ANALYSIS.md` - Original freeze issues and partial fixes
- `EVENT_DRIVEN_ARCHITECTURE.md` - Play mode state machine architecture
- `ARCHITECTURE.md` - Overall system architecture

---

**Document Version:** 1.0  
**Date:** 2025-01-12  
**Status:** Implemented and Tested  
**Coverage:** Complete network error handling overhaul

