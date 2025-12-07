#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include "shared_mem.h"
#include "semaphores.h"

typedef struct {
    char method[16];
    char path[512];
    char version[16];
} http_request_t;

void http_handle_client(int client_fd, const char* doc_root, shared_data_t* shm, semaphores_t* sems);

#endif