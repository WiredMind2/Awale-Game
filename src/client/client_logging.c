/* Client Logging - Non-interactive logging functions
 * Handles client-side logging output separate from interactive UI
 */

#include "../../include/client/client_logging.h"
#include "../../include/client/ansi_colors.h"
#include <stdarg.h>
#include <stdio.h>

void client_log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    /* Info messages in green */
    printf(ANSI_COLOR_BRIGHT_GREEN);
    vprintf(fmt, args);
    printf(ANSI_COLOR_RESET);
    va_end(args);
}

void client_log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    /* Errors to stderr in bright red */
    fprintf(stderr, ANSI_COLOR_BRIGHT_RED);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, ANSI_COLOR_RESET);
    va_end(args);
}

void client_log_warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    /* Warnings in yellow */
    printf(ANSI_COLOR_BRIGHT_YELLOW);
    vprintf(fmt, args);
    printf(ANSI_COLOR_RESET);
    va_end(args);
}