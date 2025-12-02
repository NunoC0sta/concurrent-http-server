#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

void http_handle_request(int client_fd, const char *document_root) {
    (void)document_root;

    // Read request (must read before writing response)
    char request_buf[4096];
    ssize_t bytes_read = read(client_fd, request_buf, sizeof(request_buf) - 1);
    
    if (bytes_read < 0) {
        perror("[HTTP] read failed");
        close(client_fd);
        return;
    }
    
    if (bytes_read > 0) {
        request_buf[bytes_read] = '\0';
        // Just print first line
        char *newline = strchr(request_buf, '\r');
        if (newline) *newline = '\0';
        printf("[HTTP] Request: %s\n", request_buf);
    }

    // Prepare response
    const char *body = "Hello world!\n";
    char response[512];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        strlen(body), body);

    // Send response
    ssize_t total_sent = 0;
    while (total_sent < response_len) {
        ssize_t sent = write(client_fd, response + total_sent, response_len - total_sent);
        if (sent < 0) {
            if (errno == EINTR) continue;
            perror("[HTTP] write failed");
            break;
        }
        total_sent += sent;
    }
    
    printf("[HTTP] Sent %zd bytes total\n", total_sent);
    
    // Close connection
    close(client_fd);
}