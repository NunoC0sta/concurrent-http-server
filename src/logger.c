#define _POSIX_C_SOURCE 200809L
#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>

static FILE *g_log_file = NULL;

/**
 * Initialize logging system
 */
int logger_init(const char *log_file) {
    if (!log_file) return -1;
    
    g_log_file = fopen(log_file, "a");
    if (!g_log_file) {
        perror("fopen (log file)");
        return -1;
    }
    
    return 0;
}

/**
 * Close logging system
 */
void logger_cleanup(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

/**
 * Log an HTTP request in Apache Combined Log Format
 * Format: IP - - [timestamp] "METHOD path HTTP/1.1" status bytes
 */
void log_request(ipc_handles_t *ipc, const char *client_ip, 
                 const char *request_path, const char *method,
                 int status_code, size_t bytes_sent) {
    if (!ipc || !g_log_file) return;
    
    /* Get current time */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%d/%b/%Y:%H:%M:%S %z", tm_info);
    
    /* Lock log semaphore for thread-safe writing */
    sem_wait(ipc->sem_log);
    
    /* Write log entry */
    fprintf(g_log_file, "%s - - [%s] \"%s %s HTTP/1.1\" %d %zu\n",
            client_ip ? client_ip : "127.0.0.1",
            time_buf,
            method ? method : "GET",
            request_path ? request_path : "/",
            status_code,
            bytes_sent);
    
    /* Flush to ensure it's written */
    fflush(g_log_file);
    
    /* Unlock */
    sem_post(ipc->sem_log);
}