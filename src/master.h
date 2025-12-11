#ifndef MASTER_H
#define MASTER_H

#include "config.h"
#include "stats.h"
#include <stdint.h>
#include <semaphore.h>
#include <time.h>
#define MAX_QUEUE_SIZE 100 
#define UNIX_SOCKET_PATH "/tmp/concurrent_http_sock"


#define SHM_NAME "/concurrent_http_shm"
#define SEM_MUTEX_NAME "/concurrent_http_mutex" // Mutex para exclusão mútua na fila
#define SEM_EMPTY_NAME "/concurrent_http_empty"
#define SEM_FULL_NAME "/concurrent_http_full"
#define SEM_STATS_NAME "/concurrent_http_stats"
#define SEM_LOG_NAME "/concurrent_http_log"

typedef struct {
    int connections[MAX_QUEUE_SIZE];  // Fila circular de sockets
    int front;
    int rear;
    int count;
    int max_size;
    int peak_depth;
    long total_enqueued;
    long total_dequeued;
} connection_queue_t;


typedef struct {
    connection_queue_t queue;
    server_stats_t stats;
} shared_data_t;


typedef struct {
    /* Shared memory */
    shared_data_t *shared_data;
    int shm_fd;                        
    sem_t *sem_mutex;
    sem_t *sem_empty;                  
    sem_t *sem_full;                   
    sem_t *sem_stats;
    sem_t *sem_log;
} ipc_handles_t;


int ipc_init(ipc_handles_t *handles, int max_queue_size); // Inicialização do Semáforos 
int ipc_attach(ipc_handles_t *handles);
void ipc_cleanup(ipc_handles_t *handles);
void ipc_detach(ipc_handles_t *handles);

int queue_enqueue(ipc_handles_t *handles, int client_fd);
int queue_dequeue(ipc_handles_t *handles);

void stats_update(ipc_handles_t *handles, int status_code, uint64_t bytes);
void stats_inc_active(ipc_handles_t *handles);
void stats_dec_active(ipc_handles_t *handles);
void stats_display(ipc_handles_t *handles);
void stats_record_response_time(ipc_handles_t *handles, struct timespec *start_time);

int master_init(server_config_t *config);
void master_accept_loop(void);
void master_cleanup(void);// Limpeza final

ipc_handles_t* get_ipc_handles(void);

#endif