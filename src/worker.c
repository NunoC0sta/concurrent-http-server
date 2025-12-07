#define _POSIX_C_SOURCE 200809L
#include "worker.h"
#include "thread_pool.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>

static volatile int running = 1;

// Estrutura para passar args para as threads
typedef struct {
    shared_data_t* shm;
    semaphores_t* sems;
    server_config_t* config;
    int worker_id;
} worker_args_t;

void* worker_thread_logic(void* arg) {
    worker_args_t* args = (worker_args_t*)arg;
    
    while(running) {
        //Espera haver algo na fila
        if (sem_wait(args->sems->filled_slots) != 0) break;

        //Protege acesso à fila
        if (sem_wait(args->sems->queue_mutex) != 0) break;

        //Retira socket
        connection_queue_t* q = &args->shm->queue;
        int client_fd = q->sockets[q->front];
        q->front = (q->front + 1) % MAX_QUEUE_SIZE;
        q->count--;

        //Liberta fila
        sem_post(args->sems->queue_mutex);
        sem_post(args->sems->empty_slots);

        sem_wait(args->sems->stats_mutex);
        args->shm->stats.active_connections++;
        sem_post(args->sems->stats_mutex);

        //Processa
        http_handle_client(client_fd, args->config->document_root, args->shm, args->sems);

        //Fim da conexão
        sem_wait(args->sems->stats_mutex);
        args->shm->stats.active_connections--;
        sem_post(args->sems->stats_mutex);
    }
    return NULL;
}

void worker_main(int worker_id, server_config_t *config) {
    int shm_fd = shm_open("/webserver_shm", O_RDWR, 0666);
    shared_data_t* shm = mmap(NULL, sizeof(shared_data_t), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    semaphores_t sems;
    sems.empty_slots = sem_open("/ws_empty", 0);
    sems.filled_slots = sem_open("/ws_filled", 0);
    sems.queue_mutex = sem_open("/ws_queue_mutex", 0);
    sems.stats_mutex = sem_open("/ws_stats_mutex", 0);
    sems.log_mutex = sem_open("/ws_log_mutex", 0);

    // Iniciar Thread Pool
    worker_args_t args = {shm, &sems, config, worker_id};
    thread_pool_t* pool = create_thread_pool(config->threads_per_worker);

    // Threads começam a trabalhar
    for(int i=0; i<config->threads_per_worker; i++) {
        // Usamos uma função wrapper para passar os args corretos
        pthread_create(&pool->threads[i], NULL, worker_thread_logic, &args);
    }

    // Processo worker fica vivo até receber sinal
    while(running) {
        sleep(1);
    }

    // Cleanup
    destroy_thread_pool(pool);
    munmap(shm, sizeof(shared_data_t));
}