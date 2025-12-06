#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include <time.h>

/* Shared statistics structure */
typedef struct {
    uint64_t total_requests;           /* Total HTTP requests served */
    uint64_t bytes_transferred;        /* Total bytes sent to clients */
    uint32_t status_200;               /* Successful requests */
    uint32_t status_404;               /* Not found errors */
    uint32_t status_403;               /* Forbidden errors */
    uint32_t status_500;               /* Server errors */
    uint32_t status_503;               /* Service unavailable (queue full) */
    uint32_t active_connections;       /* Current active connections */
    time_t start_time;                 /* Server start timestamp */
} server_stats_t;

#endif // STATS_H