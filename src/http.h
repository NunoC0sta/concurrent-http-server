#ifndef HTTP_H
#define HTTP_H
#include "master.h"
void http_handle_request(int client_fd, const char *document_root, ipc_handles_t *ipc);
#endif