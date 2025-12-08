#define _POSIX_C_SOURCE 200809L
#include "stats.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>

void stats_update(shared_data_t *shm, semaphores_t *sems, int status_code, uint64_t bytes) {
    if (!shm || !sems) return;

    sem_wait(sems->stats_mutex);
    
    shm->stats.total_requests++;
    shm->stats.bytes_transferred += bytes;

    switch (status_code) {
        case 200: shm->stats.status_200++; break;
        case 404: shm->stats.status_404++; break;
        case 500: shm->stats.status_500++; break;
    }
    
    sem_post(sems->stats_mutex);
}

void stats_display(shared_data_t *shm, semaphores_t *sems) {
    if (!shm || !sems) return;

    sem_wait(sems->stats_mutex);
    
    server_stats_t *s = &shm->stats;
    
    printf("\n=== SERVER STATISTICS ===\n");
    printf("Active Connections: %d\n", s->active_connections);
    printf("Total Requests:     %lu\n", s->total_requests);
    printf("Bytes Transferred:  %lu\n", s->bytes_transferred);
    printf("200 OK: %ld | 404 NF: %ld | 500 ERR: %ld\n", 
           s->status_200, s->status_404, s->status_500);
    printf("==========================================\n");

    sem_post(sems->stats_mutex);
}