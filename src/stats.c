#define _POSIX_C_SOURCE 200809L
#include "master.h"
#include <stdio.h>
#include <semaphore.h>
#include <time.h>

void stats_update(ipc_handles_t *handles, int status_code, uint64_t bytes) {
    if (!handles || !handles->shared_data) return;
    // Entra na secção critica
    sem_wait(handles->sem_stats);

    server_stats_t *stats = &handles->shared_data->stats;
    stats->total_requests++;
    stats->bytes_transferred += bytes;

    switch (status_code) {
        case 200: stats->status_200++; break;
        case 403: stats->status_403++; break;
        case 404: stats->status_404++; break;
        case 500: stats->status_500++; break;
        case 503: stats->status_503++; break;
    }

    // Sai da secção crítica
    sem_post(handles->sem_stats);
}
void stats_record_response_time(ipc_handles_t *handles, struct timespec *start_time) {
    if (!handles || !handles->shared_data || !start_time) return;

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    uint64_t elapsed_ms = (end_time.tv_sec - start_time->tv_sec) * 1000 +(end_time.tv_nsec - start_time->tv_nsec) / 1000000;
    
    // Protege a escrita do tempo total
    sem_wait(handles->sem_stats);
    handles->shared_data->stats.total_response_time_ms += elapsed_ms;
    sem_post(handles->sem_stats);
}

void stats_inc_active(ipc_handles_t *handles) {
    if (!handles || !handles->shared_data) return;

    // Protege o contador de conexões ativas
    sem_wait(handles->sem_stats);
    handles->shared_data->stats.active_connections++;
    sem_post(handles->sem_stats);
}

void stats_dec_active(ipc_handles_t *handles) {
    if (!handles || !handles->shared_data) return;

    // Protege o contador de conexões ativas
    sem_wait(handles->sem_stats);
    if (handles->shared_data->stats.active_connections > 0)
        handles->shared_data->stats.active_connections--;
    sem_post(handles->sem_stats);
}


void stats_display(ipc_handles_t *handles) {
    if (!handles || !handles->shared_data) return;

    // Bloqueia para garantir que a leitura de todas as stats é consistente
    sem_wait(handles->sem_stats);

    server_stats_t *stats = &handles->shared_data->stats;
    time_t uptime = time(NULL) - stats->start_time;

    uint64_t avg_response_time_ms = 0;
    if (stats->total_requests > 0) {
        // Cálculo da média
        avg_response_time_ms = stats->total_response_time_ms / stats->total_requests;
    }

    // Formata e imprime o relatório (simplesmente para consola do Master)
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
    printf("Average Response Time: %lu ms\n", avg_response_time_ms);
    printf("========================================\n\n");

    sem_post(handles->sem_stats);
}