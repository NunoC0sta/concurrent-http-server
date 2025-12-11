#define _POSIX_C_SOURCE 200809L
#include "master.h"
#include "worker.h"
#include "logger.h"
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
#include <time.h>
#include <errno.h>

static int g_listen_fd = -1;
static pid_t *g_worker_pids = NULL;
static int g_num_workers = 0;
static ipc_handles_t g_ipc_handles;
static volatile sig_atomic_t g_running = 1;

static void cleanup_and_exit(void);

static void signal_handler(int signum) {
    (void)signum;
    printf("\n[MASTER] Shutdown signal received\n");
    g_running = 0;
    // Fechar o socket de escuta no handler força o "accept" a sair
    if (g_listen_fd >= 0) {
        shutdown(g_listen_fd, SHUT_RDWR);
        close(g_listen_fd);
        g_listen_fd = -1;
    }
}


int ipc_init(ipc_handles_t *handles, int max_queue_size) {
    if (!handles) return -1;
    memset(handles, 0, sizeof(ipc_handles_t));

    // Configurar Memória Partilhada
    handles->shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (handles->shm_fd < 0) return -1;
    if (ftruncate(handles->shm_fd, sizeof(shared_data_t)) < 0) return -1;
    // Mapear SHM para o espaço de endereços do processo
    handles->shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, handles->shm_fd, 0);
    if (handles->shared_data == MAP_FAILED) return -1;

    // Inicializar a SHM com zeros e guardar o tamanho da fila
    memset(handles->shared_data, 0, sizeof(shared_data_t));
    handles->shared_data->queue.max_size = max_queue_size; 
    handles->shared_data->stats.start_time = time(NULL); // Importante para o Dashboard

    // Configurar Semáforos
    sem_unlink(SEM_STATS_NAME);
    sem_unlink(SEM_LOG_NAME);
    
    // Abre/Cria os semáforos. Inicializaos a1
    handles->sem_stats = sem_open(SEM_STATS_NAME, O_CREAT, 0666, 1);
    handles->sem_log = sem_open(SEM_LOG_NAME, O_CREAT, 0666, 1);

    if (handles->sem_stats == SEM_FAILED || handles->sem_log == SEM_FAILED) return -1;
    return 0;
}

void ipc_cleanup(ipc_handles_t *handles) {
    if (!handles) return;
    // Fechar descritores e remove do sistema
    if (handles->sem_log) { sem_close(handles->sem_log); sem_unlink(SEM_LOG_NAME); }
    if (handles->sem_stats) { sem_close(handles->sem_stats); sem_unlink(SEM_STATS_NAME); }
    // Desfazer mapeamento da memória
    if (handles->shared_data) munmap(handles->shared_data, sizeof(shared_data_t));
    // Fechar SHM
    if (handles->shm_fd >= 0) { close(handles->shm_fd); shm_unlink(SHM_NAME); }
}

int master_init(server_config_t *config) {
    if (!config) return -1;
    g_num_workers = config->num_workers;

    // Configurar o tratamento de sinais SIGINT e SIGTERM
    struct sigaction sa;
    sa.sa_handler = signal_handler; // Função que muda a flag g_running
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Inicialização dos subsistemas
    logger_init(config->log_file);
    if (ipc_init(&g_ipc_handles, config->max_queue_size) != 0) return -1;

    // Configurar Socket de Escuta
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // Aceita conexões em qualquer interface
    addr.sin_port = htons(config->port);

    if (bind(g_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    if (listen(g_listen_fd, 128) < 0) return -1; // Backlog de 128 conexões

    printf("[MASTER] Listening on port %d\n", config->port);

    // Lançar Workers (Multi-processo)
    g_worker_pids = calloc(g_num_workers, sizeof(pid_t));
    for (int i = 0; i < g_num_workers; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // Processo Filho- Não volta ao main, executa a função worker
            worker_main(i, config, g_listen_fd);
            exit(0);
        }
        g_worker_pids[i] = pid; // Processo Pai: Guarda o PID
    }
    return 0;
}

void master_accept_loop(void) {
    printf("[MASTER] Monitorando (Workers a aceitar conexoes)...\n");
    time_t last_stats = time(NULL);

    while (g_running) {
        sleep(1); // Evita busy-wait
        time_t now = time(NULL);
        // Atualiza estatísticas a cada 10 segundos
        if (now - last_stats >= 10) {
            stats_display(&g_ipc_handles);
            last_stats = now;
        }
    }
    cleanup_and_exit();
}

void master_cleanup(void) {
    // Limpa o socket
    if (g_listen_fd >= 0) close(g_listen_fd);
    // Termina workers (sinais)
    for (int i = 0; i < g_num_workers; ++i) if (g_worker_pids[i] > 0) kill(g_worker_pids[i], SIGTERM);
    // Espera por todos os workers
    while (wait(NULL) > 0);
    // Limpa IPC (shm, semáforos) e logs
    ipc_cleanup(&g_ipc_handles);
    logger_cleanup();
    free(g_worker_pids);
}

static void cleanup_and_exit(void) {
    master_cleanup();
    exit(0);
}