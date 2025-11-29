#ifndef MASTER_H
#define MASTER_H

#include <netinet/in.h>

// Struct for server configuration (later we will expand)
typedef struct {
    int port;
} server_config_t;

int master_start(server_config_t *config);

#endif
