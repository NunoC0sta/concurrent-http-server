#ifndef LOGGER_H
#define LOGGER_H

#include "master.h"

/**
 * Log an HTTP request to the access log file
 * Format: Apache Combined Log Format
 * Example: 127.0.0.1 - - [10/Nov/2025:13:55:36 -0800] "GET /index.html HTTP/1.1" 200 2048
 */
void log_request(ipc_handles_t *ipc, const char *client_ip, 
                 const char *request_path, const char *method,
                 int status_code, size_t bytes_sent);

/**
 * Initialize logging system (if needed)
 */
int logger_init(const char *log_file);

/**
 * Close logging system
 */
void logger_cleanup(void);

#endif // LOGGER_H