#define _POSIX_C_SOURCE 200809L
#include "master.h"  /* For ipc_handles_t and function declarations */
#include <stdio.h>
#include <semaphore.h>

/**
 * Update statistics atomically
 */
void stats_update(ipc_handles_t *handles, int status_code, uint64_t bytes) {
    if (!handles || !handles->shared_data) return;

    sem_wait(handles->sem_stats);

    server_stats_t *stats = &handles->shared_data->stats;
    stats->total_requests++;
    stats->bytes_transferred += bytes;

    /* Update status code counters */
    switch (status_code) {
        case 200: stats->status_200++; break;
        case 403: stats->status_403++; break;
        case 404: stats->status_404++; break;
        case 500: stats->status_500++; break;
        case 503: stats->status_503++; break;
    }

    sem_post(handles->sem_stats);
}

/**
 * Increment active connections
 */
void stats_inc_active(ipc_handles_t *handles) {
    if (!handles || !handles->shared_data) return;

    sem_wait(handles->sem_stats);
    handles->shared_data->stats.active_connections++;
    sem_post(handles->sem_stats);
}

/**
 * Decrement active connections
 */
void stats_dec_active(ipc_handles_t *handles) {
    if (!handles || !handles->shared_data) return;

    sem_wait(handles->sem_stats);
    if (handles->shared_data->stats.active_connections > 0)
        handles->shared_data->stats.active_connections--;
    sem_post(handles->sem_stats);
}

/**
 * Display statistics (master process)
 */
void stats_display(ipc_handles_t *handles) {
    if (!handles || !handles->shared_data) return;

    sem_wait(handles->sem_stats);

    server_stats_t *stats = &handles->shared_data->stats;
    time_t uptime = time(NULL) - stats->start_time;

    printf("\n========================================\n");
    printf("SERVER STATISTICS\n");
    printf("========================================\n");
    printf("Uptime: %ld seconds\n", uptime);
    printf("Total Requests: %lu\n", stats->total_requests);
    printf("Successful (200): %u\n", stats->status_200);
    printf("Forbidden (403): %u\n", stats->status_403);
    printf("Not Found (404): %u\n", stats->status_404);
    printf("Server Error (500): %u\n", stats->status_500);
    printf("Service Unavailable (503): %u\n", stats->status_503);
    printf("Bytes Transferred: %lu\n", stats->bytes_transferred);
    printf("Active Connections: %u\n", stats->active_connections);
    printf("========================================\n\n");

    sem_post(handles->sem_stats);
}
