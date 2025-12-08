#define _POSIX_C_SOURCE 200809L
#include "master.h"
#include "worker.h"
#include "semaphores.h"
#include "shared_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

static int g_sockfd = -1;
static volatile int keep_running = 1;
static shared_data_t* g_shm = NULL;
static semaphores_t g_sems;
static pid_t* g_workers = NULL;
static int g_num_workers = 0;

void signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
    if (g_sockfd != -1) close(g_sockfd);
}

int create_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    if (listen(sockfd, 128) < 0) return -1;
    return sockfd;
}

void enqueue_connection(shared_data_t* data, semaphores_t* sems, int client_fd) {
    sem_wait(sems->empty_slots);
    sem_wait(sems->queue_mutex);
    data->queue.sockets[data->queue.rear] = client_fd;
    data->queue.rear = (data->queue.rear + 1) % MAX_QUEUE_SIZE;
    data->queue.count++;
    sem_post(sems->queue_mutex);
    sem_post(sems->filled_slots);
}

void print_stats() {
    sem_wait(g_sems.stats_mutex);
    server_stats_t* s = &g_shm->stats;
    printf("\n=== STATS ===\nTotal Req: %ld | Bytes: %ld | Active: %d\n", 
           s->total_requests, s->bytes_transferred, s->active_connections);
    printf("200 OK: %ld | 404 NF: %ld | 500 Err: %ld\n", 
           s->status_200, s->status_404, s->status_500);
    sem_post(g_sems.stats_mutex);
}

int master_init(server_config_t *config) {
    signal(SIGINT, signal_handler);
    
    //Setup IPC
    g_shm = create_shared_memory();
    // Limpar stats iniciais
    memset(&g_shm->stats, 0, sizeof(server_stats_t));

    //Setup Semaphores
    init_semaphores(&g_sems, config->max_queue_size);

    //Socket
    g_sockfd = create_server_socket(config->port);
    if (g_sockfd < 0) return -1;
    printf("Server listening on %d\n", config->port);

    //Fork Workers
    g_num_workers = config->num_workers;
    g_workers = malloc(sizeof(pid_t) * g_num_workers);
    for(int i=0; i<g_num_workers; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            worker_main(i, config);
            exit(0);
        }
        g_workers[i] = pid;
    }
    return 0;
}

void master_accept_loop(void) {
    time_t last_print = time(NULL);
    
    while(keep_running) {
        // Print stats a cada 30s
        if (time(NULL) - last_print >= 30) {
            print_stats();
            last_print = time(NULL);
        }

        int client_fd = accept(g_sockfd, NULL, NULL);
        if (client_fd < 0) continue;
        
        enqueue_connection(g_shm, &g_sems, client_fd);
    }
}

void master_cleanup(void) {
    for(int i=0; i<g_num_workers; i++) kill(g_workers[i], SIGTERM);
    while(wait(NULL) > 0);
    destroy_semaphores(&g_sems);
    destroy_shared_memory(g_shm);
    free(g_workers);
}