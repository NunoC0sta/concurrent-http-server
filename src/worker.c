#define _POSIX_C_SOURCE 200809L
#include "worker.h"
#include "thread_pool.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>  // ADD THIS LINE

static volatile sig_atomic_t g_stop = 0;

static void term_handler(int sig) {
    (void)sig;
    g_stop = 1;
}

/* Worker state shared with threads */
typedef struct {
    int worker_id;
    int listen_fd;
    server_config_t *config;
} worker_state_t;

/* Each thread runs this: accept connections and process */
static void *worker_thread_fn(void *arg) {
    worker_state_t *st = (worker_state_t*)arg;
    
    printf("[WORKER %d THREAD %lu] Started\n", st->worker_id, (unsigned long)pthread_self());

    while (!g_stop) {
        /* Accept connection from shared listening socket */
        int client_fd = accept(st->listen_fd, NULL, NULL);
        
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("[WORKER] accept");
            continue;
        }

        printf("[WORKER %d THREAD %lu] Accepted connection fd=%d\n", 
               st->worker_id, (unsigned long)pthread_self(), client_fd);

        /* Handle the request */
        http_handle_request(client_fd, st->config->document_root);
        
        printf("[WORKER %d THREAD %lu] Finished handling fd=%d\n",
               st->worker_id, (unsigned long)pthread_self(), client_fd);
    }

    printf("[WORKER %d THREAD %lu] Exiting\n", st->worker_id, (unsigned long)pthread_self());
    return NULL;
}

void worker_main(int worker_id, server_config_t *config, int listen_fd) {
    printf("[WORKER %d] Started (pid=%d)\n", worker_id, getpid());

    /* Setup signal handlers */
    struct sigaction sa;
    sa.sa_handler = term_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Prepare worker state */
    worker_state_t st;
    st.worker_id = worker_id;
    st.listen_fd = listen_fd;
    st.config = config;

    int nthreads = (config->threads_per_worker > 0) ? config->threads_per_worker : 4;

    /* Create thread pool */
    thread_pool_t pool;
    if (thread_pool_init(&pool, nthreads, worker_thread_fn, &st) != 0) {
        fprintf(stderr, "[WORKER %d] Failed to create thread pool\n", worker_id);
        _exit(1);
    }

    printf("[WORKER %d] Thread pool created with %d threads\n", worker_id, nthreads);

    /* Wait for termination signal; threads handle all connections */
    while (!g_stop) {
        pause();
    }

    /* Shutdown */
    thread_pool_shutdown(&pool);
    
    printf("[WORKER %d] Exiting\n", worker_id);
    _exit(0);
}
