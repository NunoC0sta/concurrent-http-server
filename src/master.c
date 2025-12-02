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

static int g_listen_fd = -1;
static pid_t *g_worker_pids = NULL;
static int g_num_workers = 0;

/* Initialize master: create listening socket and fork workers */
int master_init(server_config_t *config) {
    if (!config) return -1;
    g_num_workers = (config->num_workers > 0) ? config->num_workers : 1;

    /* 1) Create listening socket */
    struct sockaddr_in addr;
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(g_listen_fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config->port);

    if (bind(g_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_listen_fd);
        return -1;
    }

    if (listen(g_listen_fd, 128) < 0) {
        perror("listen");
        close(g_listen_fd);
        return -1;
    }

    printf("[MASTER] Listening on port %d\n", config->port);

    /* 2) Fork workers - they will inherit the listening socket */
    g_worker_pids = calloc(g_num_workers, sizeof(pid_t));
    if (!g_worker_pids) {
        perror("calloc");
        close(g_listen_fd);
        return -1;
    }

    for (int i = 0; i < g_num_workers; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            g_worker_pids[i] = -1;
            continue;
        } else if (pid == 0) {
            /* Child: run worker with shared listening socket */
            worker_main(i, config, g_listen_fd);
            _exit(0);
        } else {
            /* Parent: save pid */
            g_worker_pids[i] = pid;
            printf("[MASTER] Forked worker %d (pid=%d)\n", i, pid);
        }
    }

    printf("[MASTER] Init complete. %d workers forked\n", g_num_workers);
    return 0;
}

/* Master just waits for workers - they handle connections */
void master_accept_loop(void) {
    printf("[MASTER] Workers are handling connections. Press Ctrl+C to stop.\n");
    
    /* Master doesn't accept - workers do it directly */
    /* Just wait for signals */
    while (1) {
        pause();
    }
}

/* Cleanup resources and terminate workers */
void master_cleanup(void) {
    printf("[MASTER] Cleanup requested\n");

    if (g_listen_fd >= 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
    }

    if (g_worker_pids) {
        for (int i = 0; i < g_num_workers; ++i) {
            pid_t pid = g_worker_pids[i];
            if (pid > 0) {
                printf("[MASTER] Terminating worker %d (pid=%d)\n", i, pid);
                kill(pid, SIGTERM);
            }
        }
        for (int i = 0; i < g_num_workers; ++i) {
            pid_t pid = g_worker_pids[i];
            if (pid > 0) waitpid(pid, NULL, 0);
        }
        free(g_worker_pids);
        g_worker_pids = NULL;
    }

    printf("[MASTER] Cleanup complete\n");
}