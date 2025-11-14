/* Client Logging - Non-interactive logging functions
 * Handles client-side logging output separate from interactive UI
 */

#ifndef CLIENT_LOGGING_H
#define CLIENT_LOGGING_H

#define LANGUAGE_EN 0
#define LANGUAGE_FR 1
#define CURRENT_LANGUAGE LANGUAGE_FR

#include <stdarg.h>
#include <stdio.h>

#if CURRENT_LANGUAGE == LANGUAGE_EN
#include "client_logging_strings_en.h"
#elif CURRENT_LANGUAGE == LANGUAGE_FR
#include "client_logging_strings_fr.h"
#endif

/* Logging functions */
void client_log_info(const char* fmt, ...);
void client_log_error(const char* fmt, ...);
void client_log_warning(const char* fmt, ...);

#endif /* CLIENT_LOGGING_H */