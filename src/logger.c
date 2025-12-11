#define _POSIX_C_SOURCE 200809L
#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <stdlib.h>

// Ponteiro global para o ficheiro de log
static FILE *g_log_file = NULL;

int logger_init(const char *log_file) {
    if (!log_file) return -1;
    
    // Abre o ficheiro em modo "append" ('a'). Cria se não existir.
    g_log_file = fopen(log_file, "a");
    if (!g_log_file) {
        perror("fopen (log file)");
        return -1;
    }
    
    return 0;
}

void logger_cleanup(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

void log_request(ipc_handles_t *ipc, const char *client_ip, 
                 const char *request_path, const char *method,
                 int status_code, size_t bytes_sent) {
    if (!ipc || !g_log_file) return;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%d/%b/%Y:%H:%M:%S %z", tm_info);
    
    /* Bloqueia o semáforo para garantir que apenas uma thread escreve no ficheiro.
     * Isto é a secção crítica que protege o g_log_file.
     */
    sem_wait(ipc->sem_log);
    
    /* Escreve a entrada de log (formato combinado) */
    fprintf(g_log_file, "%s - - [%s] \"%s %s HTTP/1.1\" %d %zu\n",
            client_ip ? client_ip : "127.0.0.1",
            time_buf,
            method ? method : "GET",
            request_path ? request_path : "/",
            status_code,
            bytes_sent);
    
    /* fflush é essencial para forçar a escrita imediata para o disco, 
     * em vez de esperar que o buffer interno do stdio encha.
     */
    fflush(g_log_file);
    
    /* Liberta o semáforo */
    sem_post(ipc->sem_log);
    
    /* Nota: A lógica de rotação de logs (se houver) deve ser feita fora 
     * deste semáforo (ou periodicamente) para não degradar a performance.
     */
}