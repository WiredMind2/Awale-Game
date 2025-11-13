/* Client Logging - Non-interactive logging functions
 * Handles client-side logging output separate from interactive UI
 */

#include "../../include/client/client_logging.h"
#include <stdarg.h>
#include <stdio.h>

void client_log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void client_log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void client_log_warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}