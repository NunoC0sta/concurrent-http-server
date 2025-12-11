#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include <time.h>

typedef struct {
    uint64_t total_requests;
    uint64_t bytes_transferred;
    uint32_t status_200;
    uint32_t status_404;
    uint32_t status_403;
    uint32_t status_500;
    uint32_t status_503;
    uint32_t active_connections;
    time_t start_time;
    uint64_t total_response_time_ms;
} server_stats_t;

#endif