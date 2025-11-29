#include <stdio.h>
#include "master.h"

int main() {
    server_config_t config;
    config.port = 8080;  // hardcoded for now

    printf("Starting server...\n");
    master_start(&config);

    return 0;
}
