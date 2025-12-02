#include <stdio.h>
#include <stdlib.h>
#include "master.h"
#include "config.h"

int main() {
    server_config_t config;

    // Load configuration from "server.conf"
    if (load_config("server.conf", &config) != 0) {
        fprintf(stderr, "ERROR: Could not read 'server.conf'\n");
        fprintf(stderr, "Check that the file exists and is readable.\n");
        return 1;
    }

    printf("--- Configuration Loaded ---\n");
    printf("Port: %d\n", config.port);
    printf("Workers: %d\n", config.num_workers);
    printf("Threads per Worker: %d\n", config.threads_per_worker);
    printf("----------------------------\n");

    // Initialize master (socket, shared memory, semaphores, fork workers)
    if (master_init(&config) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize master\n");
        return 1;
    }

    // Enter accept loop (blocks handling incoming connections)
    master_accept_loop();

    // Cleanup resources (this may never be reached in normal operation)
    master_cleanup();

    return 0;
}
