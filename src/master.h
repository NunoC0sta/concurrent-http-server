#ifndef MASTER_H
#define MASTER_H

#include "config.h"
#include "stats.h"
#include <stdint.h>
#include <semaphore.h>
#include <time.h>

/* Maximum number of connections in the shared queue */
#define MAX_QUEUE_SIZE 100

/* Unix domain socket path for passing file descriptors */
#define UNIX_SOCKET_PATH "/tmp/concurrent_http_sock"

/* Shared memory and semaphore names */
#define SHM_NAME "/concurrent_http_shm"
#define SEM_MUTEX_NAME "/concurrent_http_mutex"
#define SEM_EMPTY_NAME "/concurrent_http_empty"
#define SEM_FULL_NAME "/concurrent_http_full"
#define SEM_STATS_NAME "/concurrent_http_stats"
#define SEM_LOG_NAME "/concurrent_http_log"

/* Shared connection queue (producer-consumer pattern) */
typedef struct {
    int connections[MAX_QUEUE_SIZE];  /* Array of client file descriptors */
    int front;                         /* Index to dequeue from */
    int rear;                          /* Index to enqueue to */
    int count;                         /* Current number of items in queue */
    int max_size;                      /* Maximum queue capacity */
} connection_queue_t;

/* Shared statistics structure */
typedef struct {
    connection_queue_t queue;          /* Connection queue */
    server_stats_t stats;              /* Server statistics */
} shared_data_t;

/* IPC handles - manages all shared resources */
typedef struct {
    /* Shared memory */
    shared_data_t *shared_data;        /* Pointer to shared memory region */
    int shm_fd;                        /* Shared memory file descriptor */

    /* Named POSIX semaphores for inter-process synchronization */
    sem_t *sem_mutex;                  /* Mutual exclusion for queue access */
    sem_t *sem_empty;                  /* Counts empty slots in queue */
    sem_t *sem_full;                   /* Counts filled slots in queue */
    sem_t *sem_stats;                  /* Protects statistics updates */
    sem_t *sem_log;                    /* Protects log file writes */
} ipc_handles_t;

/* IPC functions */
int ipc_init(ipc_handles_t *handles, int max_queue_size);
int ipc_attach(ipc_handles_t *handles);
void ipc_cleanup(ipc_handles_t *handles);
void ipc_detach(ipc_handles_t *handles);

/* Queue operations */
int queue_enqueue(ipc_handles_t *handles, int client_fd);
int queue_dequeue(ipc_handles_t *handles);

/* Statistics operations (implemented in stats.c) */
void stats_update(ipc_handles_t *handles, int status_code, uint64_t bytes);
void stats_inc_active(ipc_handles_t *handles);
void stats_dec_active(ipc_handles_t *handles);
void stats_display(ipc_handles_t *handles);

/* Master control functions */
int master_init(server_config_t *config);
void master_accept_loop(void);
void master_cleanup(void);

/* Get IPC handles (for workers to access) */
ipc_handles_t* get_ipc_handles(void);

#endif // MASTER_H