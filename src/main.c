#include <stdio.h>
#include <stdlib.h>
#include "master.h"
#include "config.h"

int main() {
    server_config_t config;

    // Carregar configurações do ficheiro "server.conf"
    // A função load_config está definida em src/config.c
    if (load_config("server.conf", &config) != 0) {
        fprintf(stderr, "ERRO: Não foi possível ler o ficheiro 'server.conf'.\n");
        fprintf(stderr, "Verifica se o ficheiro está na pasta correta.\n");
        return 1;
    }

    printf("--- Configuração Carregada ---\n");
    printf("Porta: %d\n", config.port);
    printf("Workers: %d\n", config.num_workers);
    printf("Threads por Worker: %d\n", config.threads_per_worker);
    printf("------------------------------\n");

    printf("A iniciar servidor...\n");

    master_start(&config);

    return 0;
}