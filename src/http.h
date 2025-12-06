
#ifndef HTTP_H
#define HTTP_H

#include "master.h"  /* For ipc_handles_t */

/* HTTP handler API used by worker threads.
 * client_fd: connected socket descriptor
 * document_root: path to document root from config
 * ipc: IPC handles for statistics updates
 *
 * The implementation is responsible for:
 * - Reading and parsing HTTP request
 * - Serving files or generating error responses
 * - Updating statistics (status codes, bytes transferred)
 * - Closing client_fd
 */
void http_handle_request(int client_fd, const char *document_root, ipc_handles_t *ipc);

#endif // HTTP_H

