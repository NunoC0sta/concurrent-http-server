#include "master.h"
#include "config.h"
#include <stdio.h>

int main() {
    server_config_t config;
    if (load_config("server.conf", &config) != 0) {
        // Config default se falhar
        config.port = 8080;
        config.num_workers = 4;
        config.threads_per_worker = 10;
        config.max_queue_size = 100;
    }
    
    if (master_init(&config) != 0) return 1;
    master_accept_loop();
    master_cleanup();
    return 0;
}