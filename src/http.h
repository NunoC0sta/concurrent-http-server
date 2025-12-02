#ifndef HTTP_H
#define HTTP_H

/* Minimal HTTP handler API used by worker threads.
 * client_fd: connected socket descriptor
 * document_root: path to document root from config
 *
 * The implementation is responsible for writing the response and closing client_fd.
 */
void http_handle_request(int client_fd, const char *document_root);

#endif // HTTP_H
