#define _POSIX_C_SOURCE 200809L
#include "stats.h"
#include <stdio.h>

void stats_update(shared_data_t *shm, semaphores_t *sems, int status_code, uint64_t bytes) {
    if (!shm || !sems) return;

    sem_wait(sems->stats_mutex);
    
    shm->stats.total_requests++;
    shm->stats.bytes_transferred += bytes;

    switch (status_code) {
        case 200: shm->stats.status_200++; break;
        case 403: shm->stats.status_403++; break;
        case 404: shm->stats.status_404++; break;
        case 500: shm->stats.status_500++; break;
        case 503: shm->stats.status_503++; break;
    }
    
    sem_post(sems->stats_mutex);
}

void stats_display(shared_data_t *shm, semaphores_t *sems) {
    if (!shm || !sems) return;

    sem_wait(sems->stats_mutex);
    
    server_stats_t *s = &shm->stats;
    time_t now = time(NULL);
    
    printf("\n=== SERVER STATISTICS (Uptime: %ld s) ===\n", now - s->start_time);
    printf("Active Connections: %d\n", s->active_connections);
    printf("Total Requests:     %lu\n", s->total_requests);
    printf("Bytes Transferred:  %lu\n", s->bytes_transferred);
    printf("200 OK: %u | 404 NF: %u | 500 ERR: %u | 503 BUSY: %u\n", 
           s->status_200, s->status_404, s->status_500, s->status_503);
    printf("==========================================\n");

    sem_post(sems->stats_mutex);
}