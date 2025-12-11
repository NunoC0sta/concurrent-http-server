#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Função que lê o ficheiro de configuração e preenche a struct
int load_config(const char* filename, server_config_t* config) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return -1; // Se falhar ao abrir o ficheiro, devolve erro

    char line[512], key[128], value[256];

    while (fgets(line, sizeof(line), fp)) {
        // Ignora linhas de comentários (#) ou linhas vazias
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            // Compara as chaves para saber onde guardar os valores na struct
            if (strcmp(key, "PORT") == 0)
                config->port = atoi(value); // Converte string para int
            else if (strcmp(key, "NUM_WORKERS") == 0)
                config->num_workers = atoi(value);
            else if (strcmp(key, "THREADS_PER_WORKER") == 0)
                config->threads_per_worker = atoi(value);
            else if (strcmp(key, "DOCUMENT_ROOT") == 0)
                // Usar strncpy para evitar buffer overflows se o caminho for muito longo
                strncpy(config->document_root, value, sizeof(config->document_root));
            else if (strcmp(key, "MAX_QUEUE_SIZE") == 0)
                config->max_queue_size = atoi(value);
            else if (strcmp(key, "LOG_FILE") == 0)
                strncpy(config->log_file, value, sizeof(config->log_file));
            else if (strcmp(key, "CACHE_SIZE_MB") == 0)
                config->cache_size_mb = atoi(value);
            else if (strcmp(key, "TIMEOUT_SECONDS") == 0)
                config->timeout_seconds = atoi(value);
        }
    }
    
    fclose(fp);
    return 0;
}