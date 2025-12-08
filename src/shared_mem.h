#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stddef.h>

#define MAX_QUEUE_SIZE 100

typedef struct {
    long total_requests;
    long bytes_transferred;
    long status_200;
    long status_404;
    long status_500;
    int active_connections;
} server_stats_t;

typedef struct {
    int sockets[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    int peak_depth;           /* Peak queue depth reached */
    long total_enqueued;      /* Total connections enqueued */
    long total_dequeued;      /* Total connections dequeued */
    unsigned long total_wait_time;  /* Cumulative wait time in ms */
} connection_queue_t;

typedef struct {
    connection_queue_t queue;
    server_stats_t stats;
} shared_data_t;

shared_data_t* create_shared_memory(void);
void destroy_shared_memory(shared_data_t* data);

#endif