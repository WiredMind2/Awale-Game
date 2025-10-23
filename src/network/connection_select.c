/* Select-based Multiplexing
 * Provides select() based I/O multiplexing for non-blocking operations
 */

#include "../../include/network/connection.h"
#include <string.h>
#include <errno.h>
#include <sys/select.h>

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
