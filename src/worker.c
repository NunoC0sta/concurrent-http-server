#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "worker.h"
#include "thread_pool.h"
#include "http.h"
#include "master.h"  /* For IPC access and stats functions */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

static volatile sig_atomic_t g_stop = 0;

static void term_handler(int sig) {
    (void)sig;
    g_stop = 1;
}

/* Worker state shared with threads */
typedef struct {
    int worker_id;
    server_config_t *config;
    ipc_handles_t *ipc;           /* IPC handles for stats */
    int listen_fd;                 /* Shared listening socket */
} worker_state_t;

/**
 * Worker thread function
 * Each thread accepts connections directly from the shared listening socket
 */
static void *worker_thread_fn(void *arg) {
    worker_state_t *st = (worker_state_t*)arg;

    printf("[WORKER %d THREAD %lu] Started\n",
           st->worker_id, (unsigned long)pthread_self());

    while (!g_stop) {
        /* Accept connection directly from shared listening socket
         * The kernel handles synchronization - only one thread gets each connection */
        int client_fd = accept(st->listen_fd, NULL, NULL);

        if (client_fd < 0) {
            if (errno == EINTR || g_stop) {
                /* Interrupted by signal or shutdown requested */
                printf("[WORKER %d THREAD %lu] Shutdown signal received\n",
                       st->worker_id, (unsigned long)pthread_self());
                break;
            }
            perror("[WORKER] accept failed");
            continue;
        }

        /* Check if we should stop */
        if (g_stop) {
            close(client_fd);
            break;
        }

        printf("[WORKER %d THREAD %lu] Accepted connection fd=%d\n",
               st->worker_id, (unsigned long)pthread_self(), client_fd);

        /* Increment active connections counter */
        stats_inc_active(st->ipc);

        /* Handle the HTTP request */
        http_handle_request(client_fd, st->config->document_root, st->ipc);

        /* Decrement active connections counter */
        stats_dec_active(st->ipc);

        printf("[WORKER %d THREAD %lu] Finished handling fd=%d\n",
               st->worker_id, (unsigned long)pthread_self(), client_fd);
    }

    printf("[WORKER %d THREAD %lu] Exiting\n",
           st->worker_id, (unsigned long)pthread_self());
    return NULL;
}

/**
 * Worker process main function
 * - Attaches to IPC resources
 * - Creates thread pool
 * - Threads accept connections directly from shared listening socket
 * - Waits for termination signal
 */
void worker_main(int worker_id, server_config_t *config, int listen_fd) {
    printf("[WORKER %d] Started (pid=%d)\n", worker_id, getpid());

    /* Setup signal handlers */
    struct sigaction sa;
    sa.sa_handler = term_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    /* Attach to IPC resources (shared memory + semaphores) */
    ipc_handles_t ipc;
    if (ipc_attach(&ipc) != 0) {
        fprintf(stderr, "[WORKER %d] Failed to attach to IPC\n", worker_id);
        _exit(1);
    }
    printf("[WORKER %d] Attached to IPC successfully\n", worker_id);

    /* Prepare worker state for threads */
    worker_state_t st;
    st.worker_id = worker_id;
    st.config = config;
    st.ipc = &ipc;
    st.listen_fd = listen_fd;

    int nthreads = (config->threads_per_worker > 0) ? config->threads_per_worker : 4;

    /* Create thread pool */
    thread_pool_t pool;
    if (thread_pool_init(&pool, nthreads, worker_thread_fn, &st) != 0) {
        fprintf(stderr, "[WORKER %d] Failed to create thread pool\n", worker_id);
        ipc_detach(&ipc);
        _exit(1);
    }

    printf("[WORKER %d] Thread pool created with %d threads\n", worker_id, nthreads);
    printf("[WORKER %d] Threads will accept connections from shared socket\n", worker_id);

    /* Wait for termination signal */
    while (!g_stop) {
        pause();
    }

    printf("[WORKER %d] Shutdown signal received, cleaning up...\n", worker_id);

    /* Close listening socket to wake up threads blocked on accept() */
    shutdown(listen_fd, SHUT_RDWR);

    /* Give threads a moment to notice */
    struct timespec ts = {0, 100000000}; // 100ms
    nanosleep(&ts, NULL);

    /* Shutdown thread pool (join all threads) */
    printf("[WORKER %d] Waiting for threads to exit...\n", worker_id);
    thread_pool_shutdown(&pool);
    printf("[WORKER %d] All threads exited\n", worker_id);

    /* Detach from IPC */
    ipc_detach(&ipc);

    printf("[WORKER %d] Exiting cleanly\n", worker_id);
    _exit(0);
}