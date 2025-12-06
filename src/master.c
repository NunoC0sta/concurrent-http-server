#define _POSIX_C_SOURCE 200809L
#include "master.h"
#include "worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

/* Global state */
static int g_listen_fd = -1;
static pid_t *g_worker_pids = NULL;
static int g_num_workers = 0;
static ipc_handles_t g_ipc_handles;
static volatile sig_atomic_t g_running = 1;

/* Forward declaration */
static void cleanup_and_exit(void);

/* Signal handler for graceful shutdown */
static void signal_handler(int signum) {
    (void)signum;
    printf("\n[MASTER] Shutdown signal received (Ctrl+C)\n");
    g_running = 0;

    /* Close listening socket to unblock accept() */
    if (g_listen_fd >= 0) {
        shutdown(g_listen_fd, SHUT_RDWR);
        close(g_listen_fd);
        g_listen_fd = -1;
    }
}

/* ========================================================================
 * IPC IMPLEMENTATION
 * ======================================================================== */

/**
 * Initialize all IPC resources (master process only)
 */
int ipc_init(ipc_handles_t *handles, int max_queue_size) {
    if (!handles) return -1;

    memset(handles, 0, sizeof(ipc_handles_t));

    printf("[IPC] Initializing shared memory and semaphores...\n");

    /* 1. Create shared memory */
    handles->shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (handles->shm_fd < 0) {
        perror("[IPC] shm_open failed");
        return -1;
    }

    /* Set size of shared memory */
    if (ftruncate(handles->shm_fd, sizeof(shared_data_t)) < 0) {
        perror("[IPC] ftruncate failed");
        close(handles->shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    /* Map shared memory */
    handles->shared_data = mmap(NULL, sizeof(shared_data_t),
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED, handles->shm_fd, 0);
    if (handles->shared_data == MAP_FAILED) {
        perror("[IPC] mmap failed");
        close(handles->shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    /* Initialize shared data structures */
    memset(handles->shared_data, 0, sizeof(shared_data_t));
    handles->shared_data->queue.max_size = max_queue_size;
    handles->shared_data->queue.front = 0;
    handles->shared_data->queue.rear = 0;
    handles->shared_data->queue.count = 0;
    handles->shared_data->stats.start_time = time(NULL);

    printf("[IPC] Shared memory created and initialized\n");

    /* 2. Create semaphores */
    sem_unlink(SEM_MUTEX_NAME);
    handles->sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (handles->sem_mutex == SEM_FAILED) {
        perror("[IPC] sem_open (mutex) failed");
        goto cleanup_shm;
    }

    sem_unlink(SEM_EMPTY_NAME);
    handles->sem_empty = sem_open(SEM_EMPTY_NAME, O_CREAT | O_EXCL, 0666, max_queue_size);
    if (handles->sem_empty == SEM_FAILED) {
        perror("[IPC] sem_open (empty) failed");
        goto cleanup_mutex;
    }

    sem_unlink(SEM_FULL_NAME);
    handles->sem_full = sem_open(SEM_FULL_NAME, O_CREAT | O_EXCL, 0666, 0);
    if (handles->sem_full == SEM_FAILED) {
        perror("[IPC] sem_open (full) failed");
        goto cleanup_empty;
    }

    sem_unlink(SEM_STATS_NAME);
    handles->sem_stats = sem_open(SEM_STATS_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (handles->sem_stats == SEM_FAILED) {
        perror("[IPC] sem_open (stats) failed");
        goto cleanup_full;
    }

    sem_unlink(SEM_LOG_NAME);
    handles->sem_log = sem_open(SEM_LOG_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (handles->sem_log == SEM_FAILED) {
        perror("[IPC] sem_open (log) failed");
        goto cleanup_stats;
    }

    printf("[IPC] All semaphores created successfully\n");
    return 0;

cleanup_stats:
    sem_close(handles->sem_stats);
    sem_unlink(SEM_STATS_NAME);
cleanup_full:
    sem_close(handles->sem_full);
    sem_unlink(SEM_FULL_NAME);
cleanup_empty:
    sem_close(handles->sem_empty);
    sem_unlink(SEM_EMPTY_NAME);
cleanup_mutex:
    sem_close(handles->sem_mutex);
    sem_unlink(SEM_MUTEX_NAME);
cleanup_shm:
    munmap(handles->shared_data, sizeof(shared_data_t));
    close(handles->shm_fd);
    shm_unlink(SHM_NAME);
    return -1;
}

/**
 * Attach to existing IPC resources (worker processes)
 */
int ipc_attach(ipc_handles_t *handles) {
    if (!handles) return -1;

    memset(handles, 0, sizeof(ipc_handles_t));

    /* Open existing shared memory */
    handles->shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (handles->shm_fd < 0) {
        perror("[IPC] shm_open (attach) failed");
        return -1;
    }

    /* Map shared memory */
    handles->shared_data = mmap(NULL, sizeof(shared_data_t),
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED, handles->shm_fd, 0);
    if (handles->shared_data == MAP_FAILED) {
        perror("[IPC] mmap (attach) failed");
        close(handles->shm_fd);
        return -1;
    }

    /* Open existing semaphores */
    handles->sem_mutex = sem_open(SEM_MUTEX_NAME, 0);
    if (handles->sem_mutex == SEM_FAILED) {
        perror("[IPC] sem_open (mutex attach) failed");
        goto cleanup_shm;
    }

    handles->sem_empty = sem_open(SEM_EMPTY_NAME, 0);
    if (handles->sem_empty == SEM_FAILED) {
        perror("[IPC] sem_open (empty attach) failed");
        goto cleanup_mutex;
    }

    handles->sem_full = sem_open(SEM_FULL_NAME, 0);
    if (handles->sem_full == SEM_FAILED) {
        perror("[IPC] sem_open (full attach) failed");
        goto cleanup_empty;
    }

    handles->sem_stats = sem_open(SEM_STATS_NAME, 0);
    if (handles->sem_stats == SEM_FAILED) {
        perror("[IPC] sem_open (stats attach) failed");
        goto cleanup_full;
    }

    handles->sem_log = sem_open(SEM_LOG_NAME, 0);
    if (handles->sem_log == SEM_FAILED) {
        perror("[IPC] sem_open (log attach) failed");
        goto cleanup_stats;
    }

    return 0;

cleanup_stats:
    sem_close(handles->sem_stats);
cleanup_full:
    sem_close(handles->sem_full);
cleanup_empty:
    sem_close(handles->sem_empty);
cleanup_mutex:
    sem_close(handles->sem_mutex);
cleanup_shm:
    munmap(handles->shared_data, sizeof(shared_data_t));
    close(handles->shm_fd);
    return -1;
}

/**
 * Cleanup IPC resources (master process)
 */
void ipc_cleanup(ipc_handles_t *handles) {
    if (!handles) return;

    printf("[IPC] Cleaning up IPC resources...\n");

    if (handles->sem_log != NULL && handles->sem_log != SEM_FAILED) {
        sem_close(handles->sem_log);
        sem_unlink(SEM_LOG_NAME);
    }

    if (handles->sem_stats != NULL && handles->sem_stats != SEM_FAILED) {
        sem_close(handles->sem_stats);
        sem_unlink(SEM_STATS_NAME);
    }

    if (handles->sem_full != NULL && handles->sem_full != SEM_FAILED) {
        sem_close(handles->sem_full);
        sem_unlink(SEM_FULL_NAME);
    }

    if (handles->sem_empty != NULL && handles->sem_empty != SEM_FAILED) {
        sem_close(handles->sem_empty);
        sem_unlink(SEM_EMPTY_NAME);
    }

    if (handles->sem_mutex != NULL && handles->sem_mutex != SEM_FAILED) {
        sem_close(handles->sem_mutex);
        sem_unlink(SEM_MUTEX_NAME);
    }

    if (handles->shared_data != NULL && handles->shared_data != MAP_FAILED) {
        munmap(handles->shared_data, sizeof(shared_data_t));
    }

    if (handles->shm_fd >= 0) {
        close(handles->shm_fd);
    }

    shm_unlink(SHM_NAME);

    printf("[IPC] Cleanup complete\n");
}

/**
 * Detach from IPC resources (worker processes)
 */
void ipc_detach(ipc_handles_t *handles) {
    if (!handles) return;

    if (handles->sem_log != NULL && handles->sem_log != SEM_FAILED)
        sem_close(handles->sem_log);

    if (handles->sem_stats != NULL && handles->sem_stats != SEM_FAILED)
        sem_close(handles->sem_stats);

    if (handles->sem_full != NULL && handles->sem_full != SEM_FAILED)
        sem_close(handles->sem_full);

    if (handles->sem_empty != NULL && handles->sem_empty != SEM_FAILED)
        sem_close(handles->sem_empty);

    if (handles->sem_mutex != NULL && handles->sem_mutex != SEM_FAILED)
        sem_close(handles->sem_mutex);

    if (handles->shared_data != NULL && handles->shared_data != MAP_FAILED)
        munmap(handles->shared_data, sizeof(shared_data_t));

    if (handles->shm_fd >= 0)
        close(handles->shm_fd);
}

/**
 * Enqueue a connection (producer - master process)
 */
int queue_enqueue(ipc_handles_t *handles, int client_fd) {
    if (!handles || !handles->shared_data) return -1;

    /* Wait for empty slot */
    if (sem_wait(handles->sem_empty) < 0) {
        if (errno == EINTR) return -1;
        perror("[QUEUE] sem_wait (empty) failed");
        return -1;
    }

    /* Acquire mutex */
    if (sem_wait(handles->sem_mutex) < 0) {
        sem_post(handles->sem_empty);
        perror("[QUEUE] sem_wait (mutex) failed");
        return -1;
    }

    /* Critical section: add to queue */
    connection_queue_t *q = &handles->shared_data->queue;
    q->connections[q->rear] = client_fd;
    q->rear = (q->rear + 1) % q->max_size;
    q->count++;

    /* Release mutex */
    sem_post(handles->sem_mutex);

    /* Signal that slot is filled */
    sem_post(handles->sem_full);

    return 0;
}

/**
 * Dequeue a connection (consumer - worker threads)
 */
int queue_dequeue(ipc_handles_t *handles) {
    if (!handles || !handles->shared_data) return -1;

    /* Wait for filled slot */
    if (sem_wait(handles->sem_full) < 0) {
        if (errno == EINTR) return -1;
        perror("[QUEUE] sem_wait (full) failed");
        return -1;
    }

    /* Acquire mutex */
    if (sem_wait(handles->sem_mutex) < 0) {
        sem_post(handles->sem_full);
        perror("[QUEUE] sem_wait (mutex) failed");
        return -1;
    }

    /* Critical section: remove from queue */
    connection_queue_t *q = &handles->shared_data->queue;
    int client_fd = q->connections[q->front];
    q->front = (q->front + 1) % q->max_size;
    q->count--;

    /* Release mutex */
    sem_post(handles->sem_mutex);

    /* Signal that slot is empty */
    sem_post(handles->sem_empty);

    return client_fd;
}

/* Accessor for workers */
ipc_handles_t* get_ipc_handles(void) {
    return &g_ipc_handles;
}

/* ========================================================================
 * MASTER PROCESS IMPLEMENTATION
 * ======================================================================== */

/**
 * Initialize master: create listening socket, IPC, and fork workers
 */
int master_init(server_config_t *config) {
    if (!config) return -1;
    g_num_workers = (config->num_workers > 0) ? config->num_workers : 1;

    /* Setup signal handlers - MUST be done before anything else */
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Ignore SIGPIPE (can occur when writing to closed sockets) */
    signal(SIGPIPE, SIG_IGN);

    /* 1) Initialize IPC (shared memory + semaphores) */
    if (ipc_init(&g_ipc_handles, config->max_queue_size) != 0) {
        fprintf(stderr, "[MASTER] Failed to initialize IPC\n");
        return -1;
    }

    /* 2) Create listening socket */
    struct sockaddr_in addr;
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        perror("socket");
        ipc_cleanup(&g_ipc_handles);
        return -1;
    }

    int opt = 1;
    if (setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(g_listen_fd);
        ipc_cleanup(&g_ipc_handles);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config->port);

    if (bind(g_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_listen_fd);
        ipc_cleanup(&g_ipc_handles);
        return -1;
    }

    if (listen(g_listen_fd, 128) < 0) {
        perror("listen");
        close(g_listen_fd);
        ipc_cleanup(&g_ipc_handles);
        return -1;
    }

    printf("[MASTER] Listening on port %d\n", config->port);

    /* 3) Fork workers */
    g_worker_pids = calloc(g_num_workers, sizeof(pid_t));
    if (!g_worker_pids) {
        perror("calloc");
        close(g_listen_fd);
        ipc_cleanup(&g_ipc_handles);
        return -1;
    }

    for (int i = 0; i < g_num_workers; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            g_worker_pids[i] = -1;
            continue;
        } else if (pid == 0) {
            /* Child: run worker - workers inherit the listening socket */
            worker_main(i, config, g_listen_fd);
            _exit(0);
        } else {
            /* Parent: save pid */
            g_worker_pids[i] = pid;
            printf("[MASTER] Forked worker %d (pid=%d)\n", i, pid);
        }
    }

    printf("[MASTER] Init complete. %d workers forked\n", g_num_workers);
    printf("[MASTER] Press Ctrl+C to shutdown gracefully\n");
    return 0;
}

/**
 * Master accept loop: just wait and display statistics
 * Workers handle all connections by accepting from the shared socket
 */
void master_accept_loop(void) {
    printf("[MASTER] Workers are handling all connections\n");
    printf("[MASTER] Master will display statistics every 30 seconds\n");

    time_t last_stats = time(NULL);

    while (g_running) {
        /* Sleep for a bit */
        sleep(1);

        /* Display stats every 30 seconds */
        time_t now = time(NULL);
        if (now - last_stats >= 30) {
            stats_display(&g_ipc_handles);
            last_stats = now;
        }
    }

    printf("[MASTER] Shutting down\n");

    /* Cleanup on exit */
    cleanup_and_exit();
}

/**
 * Cleanup resources and terminate workers
 */
void master_cleanup(void) {
    printf("[MASTER] Cleanup requested\n");

    if (g_listen_fd >= 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
    }

    if (g_worker_pids) {
        printf("[MASTER] Sending SIGTERM to all workers...\n");
        for (int i = 0; i < g_num_workers; ++i) {
            pid_t pid = g_worker_pids[i];
            if (pid > 0) {
                printf("[MASTER] Terminating worker %d (pid=%d)\n", i, pid);
                kill(pid, SIGTERM);
            }
        }

        printf("[MASTER] Waiting for workers to exit...\n");
        for (int i = 0; i < g_num_workers; ++i) {
            pid_t pid = g_worker_pids[i];
            if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                printf("[MASTER] Worker %d (pid=%d) exited\n", i, pid);
            }
        }

        free(g_worker_pids);
        g_worker_pids = NULL;
    }

    /* Display final statistics */
    printf("\n[MASTER] Final Statistics:\n");
    stats_display(&g_ipc_handles);

    /* Cleanup IPC */
    ipc_cleanup(&g_ipc_handles);

    printf("[MASTER] Cleanup complete\n");
}

/**
 * Cleanup and exit (called from signal handler context)
 */
static void cleanup_and_exit(void) {
    master_cleanup();
    exit(0);
}