#define _POSIX_C_SOURCE 200809L
#include "worker.h"
#include "thread_pool.h"
#include "http.h"
#include "master.h"
#include "cache.h" // <--- Importante: Incluir o header da cache
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>

static volatile int g_stop = 0;
void term_handler(int sig) { (void)sig; g_stop = 1; }

typedef struct {
    int worker_id;
    server_config_t *config;
    ipc_handles_t *ipc;
    int listen_fd;
    cache_t *cache; // <--- Novo campo para guardar o ponteiro da cache
} worker_state_t;

// Anexo IPC simplificado para workers
int ipc_attach_worker(ipc_handles_t *handles) {
    handles->shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    handles->shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ|PROT_WRITE, MAP_SHARED, handles->shm_fd, 0);
    handles->sem_stats = sem_open(SEM_STATS_NAME, 0);
    handles->sem_log = sem_open(SEM_LOG_NAME, 0);
    return (handles->shared_data == MAP_FAILED) ? -1 : 0;
}

static void *worker_thread_fn(void *arg) {
    worker_state_t *st = (worker_state_t*)arg;
    while (!g_stop) {
        // Shared Port Accept: O Kernel distribui a carga
        int client_fd = accept(st->listen_fd, NULL, NULL);
        if (client_fd < 0) continue;

        stats_inc_active(st->ipc);
        
        // CORREÇÃO AQUI: Passamos st->cache como 4º argumento
        http_handle_request(client_fd, st->config->document_root, st->ipc, st->cache);
        
        stats_dec_active(st->ipc);
    }
    return NULL;
}

void worker_main(int worker_id, server_config_t *config, int listen_fd) {
    struct sigaction sa = { .sa_handler = term_handler };
    sigaction(SIGINT, &sa, NULL); 
    sigaction(SIGTERM, &sa, NULL);

    ipc_handles_t ipc;
    ipc_attach_worker(&ipc);

    // 1. INICIALIZAR A CACHE (Cada worker tem a sua)
    printf("[WORKER %d] A iniciar cache com %d MB...\n", worker_id, config->cache_size_mb);
    cache_t *local_cache = cache_init(config->cache_size_mb);

    // 2. ADICIONAR AO ESTADO DO WORKER
    worker_state_t st = { 
        .worker_id = worker_id, 
        .config = config, 
        .ipc = &ipc, 
        .listen_fd = listen_fd,
        .cache = local_cache // <--- Guardar aqui
    };

    thread_pool_t pool;
    thread_pool_init(&pool, config->threads_per_worker, worker_thread_fn, &st);

    while (!g_stop) sleep(1);

    thread_pool_shutdown(&pool);

    // 3. LIMPAR A CACHE ANTES DE SAIR
    if (local_cache) {
        cache_destroy(local_cache);
    }

    exit(0);
}