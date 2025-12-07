#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Necessário para o atoi() usado no template

// Implementação baseada no Template [cite: 1232-1256]
int load_config(const char* filename, server_config_t* config) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;

    char line[512], key[128], value[256];

    config->port = 8080;
    config->num_workers = 4;
    config->threads_per_worker = 10;
    config->max_queue_size = 100;
    config->cache_size_mb = 10;
    config->timeout_seconds = 30;
    strcpy(config->document_root, "/var/www/html");
    strcpy(config->log_file, "access.log");

    while (fgets(line, sizeof(line), fp)) {
        // Ignorar comentários e linhas vazias
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            if (strcmp(key, "PORT") == 0) 
                config->port = atoi(value);
            else if (strcmp(key, "NUM_WORKERS") == 0) 
                config->num_workers = atoi(value);
            else if (strcmp(key, "THREADS_PER_WORKER") == 0) 
                config->threads_per_worker = atoi(value);
            else if (strcmp(key, "DOCUMENT_ROOT") == 0) 
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