#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

void http_handle_request(int client_fd, const char *document_root, ipc_handles_t *ipc) {
    (void)document_root;  /* Will be used later for file serving */

    /* Verify file descriptor is valid */
    if (client_fd < 0) {
        stats_update(ipc, 500, 0);
        return;
    }

    /* Read request (must read before writing response) */
    char request_buf[4096];
    ssize_t bytes_read = read(client_fd, request_buf, sizeof(request_buf) - 1);

    if (bytes_read < 0) {
        if (errno != EINTR && errno != ECONNRESET) {
            perror("[HTTP] read failed");
        }
        close(client_fd);
        stats_update(ipc, 500, 0);  /* Internal server error */
        return;
    }

    if (bytes_read == 0) {
        /* Client closed connection before sending data */
        close(client_fd);
        return;
    }

    request_buf[bytes_read] = '\0';

    /* Just print first line */
    char *newline = strchr(request_buf, '\r');
    if (newline) *newline = '\0';
    printf("[HTTP] Request: %s\n", request_buf);

    /* Prepare response */
    const char *body = "<!DOCTYPE html>\n"
                       "<html>\n"
                       "<head><title>ConcurrentHTTP Server</title></head>\n"
                       "<body>\n"
                       "<h1>Welcome to ConcurrentHTTP Server!</h1>\n"
                       "<p>Your multi-threaded web server is working!</p>\n"
                       "<p>Worker process ID: %d</p>\n"
                       "</body>\n"
                       "</html>\n";

    char full_body[1024];
    snprintf(full_body, sizeof(full_body), body, getpid());

    char response[2048];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Server: ConcurrentHTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        strlen(full_body), full_body);

    /* Send response */
    ssize_t total_sent = 0;
    while (total_sent < response_len) {
        ssize_t sent = write(client_fd, response + total_sent, response_len - total_sent);
        if (sent < 0) {
            if (errno == EINTR) continue;
            if (errno != EPIPE && errno != ECONNRESET) {
                perror("[HTTP] write failed");
            }
            stats_update(ipc, 500, total_sent);
            close(client_fd);
            return;
        }
        total_sent += sent;
    }

    printf("[HTTP] Sent %zd bytes total\n", total_sent);

    /* Update statistics: successful request (200 OK) */
    stats_update(ipc, 200, total_sent);

    /* Close connection */
    close(client_fd);
}