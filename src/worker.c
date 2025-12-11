#define _POSIX_C_SOURCE 200809L
#include "worker.h"
#include "thread_pool.h"
#include "http.h"
#include "master.h"
#include "logger.h"
#include "cache.h"
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
    cache_t *cache;
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
        int client_fd = accept(st->listen_fd, NULL, NULL);
        if (client_fd < 0) continue;

        stats_inc_active(st->ipc);

        // Processar o request com cache
        http_handle_request(client_fd, st->config->document_root, st->ipc, st->cache);

        stats_dec_active(st->ipc);
    }
    return NULL;
}

void worker_main(int worker_id, server_config_t *config, int listen_fd) {
    struct sigaction sa = { .sa_handler = term_handler };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Anexar IPC do worker
    ipc_handles_t ipc;
    if (ipc_attach_worker(&ipc) != 0) {
        fprintf(stderr, "[WORKER %d] Failed to attach IPC\n", worker_id);
        exit(1);
    }

    printf("[WORKER %d] Initializing logger: %s\n", worker_id, config->log_file);
    if (logger_init(config->log_file) != 0) {
        fprintf(stderr, "[WORKER %d] Failed to initialize logger\n", worker_id);
        exit(1);
    }

    // Inicializar a cache
    printf("[WORKER %d] Initializing cache with %d MB...\n", worker_id, config->cache_size_mb);
    cache_t *local_cache = cache_init(config->cache_size_mb);
    if (!local_cache) {
        fprintf(stderr, "[WORKER %d] Failed to initialize cache\n", worker_id);
        logger_cleanup();
        exit(1);
    }

    // Estado do worker
    worker_state_t st = {
        .worker_id = worker_id,
        .config = config,
        .ipc = &ipc,
        .listen_fd = listen_fd,
        .cache = local_cache
    };

    // Criar thread pool
    thread_pool_t pool;
    if (thread_pool_init(&pool, config->threads_per_worker, worker_thread_fn, &st) != 0) {
        fprintf(stderr, "[WORKER %d] Failed to initialize thread pool\n", worker_id);
        cache_destroy(local_cache);
        logger_cleanup();
        exit(1);
    }

    printf("[WORKER %d] Ready with %d threads\n", worker_id, config->threads_per_worker);

    while (!g_stop) sleep(1);

    printf("[WORKER %d] Shutting down...\n", worker_id);

    // Cleanup
    thread_pool_shutdown(&pool);

    if (local_cache) {
        cache_destroy(local_cache);
    }

    logger_cleanup();

    printf("[WORKER %d] Exiting\n", worker_id);
    exit(0);
}