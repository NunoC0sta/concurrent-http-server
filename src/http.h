#ifndef HTTP_H
#define HTTP_H

#include "master.h"
#include "cache.h"

void http_handle_request(int client_fd, const char *document_root, ipc_handles_t *ipc, cache_t* cache);

#endif