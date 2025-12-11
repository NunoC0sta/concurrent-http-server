#include <stdio.h>
#include <stdlib.h>
#include "master.h"
#include "config.h"

int main() {
    server_config_t config;
    // Tenta carregar os parâmetros do servidor a partir do ficheiro
    if (load_config("server.conf", &config) != 0) {
        fprintf(stderr, "ERROR: Could not read 'server.conf'\n");
        fprintf(stderr, "Check that the file exists and is readable.\n");
        return 1;
    }

    // Feedback básico na consola para confirmar que o ficheiro foi lido
    printf("--- Configuration Loaded ---\n");
    printf("Port: %d\n", config.port);
    printf("Workers: %d\n", config.num_workers);
    printf("Threads per Worker: %d\n", config.threads_per_worker);
    printf("----------------------------\n");

    if (master_init(&config) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize master\n");
        return 1;
    }

    master_accept_loop();

    // Limpeza
    master_cleanup();

    return 0;
}