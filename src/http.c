#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include "logger.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>

/**
 * Get MIME type based on file extension
 */
static const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".pdf") == 0)
        return "application/pdf";
    if (strcmp(ext, ".txt") == 0)
        return "text/plain";
    
    return "application/octet-stream";
}

/**
 * Parse HTTP request line
 * Returns allocated strings for method and path (caller must free)
 */
static int parse_request(const char *request, char **method, char **path) {
    char *req_copy = strdup(request);
    if (!req_copy) return -1;
    
    /* Find first line */
    char *newline = strstr(req_copy, "\r\n");
    if (newline) *newline = '\0';
    
    /* Parse: METHOD PATH HTTP/version */
    char *token = strtok(req_copy, " ");
    if (!token) {
        free(req_copy);
        return -1;
    }
    *method = strdup(token);
    
    token = strtok(NULL, " ");
    if (!token) {
        free(*method);
        free(req_copy);
        return -1;
    }
    *path = strdup(token);
    
    free(req_copy);
    return 0;
}

/**
 * Send HTTP response headers
 */
static void send_response_headers(int client_fd, int status_code, 
                                   const char *content_type,
                                   size_t content_length) {
    char header[1024];
    const char *status_text = "OK";
    
    switch (status_code) {
        case 200: status_text = "OK"; break;
        case 403: status_text = "Forbidden"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        case 503: status_text = "Service Unavailable"; break;
    }
    
    int len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Server: ConcurrentHTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, content_length);
    
    write(client_fd, header, len);
}

/**
 * Serve a file from disk
 */
static void serve_file(int client_fd, const char *file_path, 
                       const char *request_path, ipc_handles_t *ipc) {
    struct stat st;
    if (stat(file_path, &st) < 0) {
        /* File not found */
        const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
        size_t body_len = strlen(body);
        
        send_response_headers(client_fd, 404, "text/html", body_len);
        write(client_fd, body, body_len);
        
        log_request(ipc, "127.0.0.1", request_path, "GET", 404, body_len);
        stats_update(ipc, 404, body_len);
        return;
    }
    
    /* Check if it's a directory */
    if (S_ISDIR(st.st_mode)) {
        /* Try to serve index.html */
        char index_path[512];
        snprintf(index_path, sizeof(index_path), "%s/index.html", file_path);
        
        if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
            serve_file(client_fd, index_path, request_path, ipc);
            return;
        } else {
            /* No index.html, return 403 Forbidden */
            const char *body = "<html><body><h1>403 Forbidden</h1></body></html>";
            size_t body_len = strlen(body);
            
            send_response_headers(client_fd, 403, "text/html", body_len);
            write(client_fd, body, body_len);
            
            log_request(ipc, "127.0.0.1", request_path, "GET", 403, body_len);
            stats_update(ipc, 403, body_len);
            return;
        }
    }
    
    /* Open and serve the file */
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        const char *body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
        size_t body_len = strlen(body);
        
        send_response_headers(client_fd, 500, "text/html", body_len);
        write(client_fd, body, body_len);
        
        log_request(ipc, "127.0.0.1", request_path, "GET", 500, body_len);
        stats_update(ipc, 500, body_len);
        return;
    }
    
    /* Send headers */
    const char *mime_type = get_mime_type(file_path);
    send_response_headers(client_fd, 200, mime_type, st.st_size);
    
    /* Send file content */
    char buf[4096];
    ssize_t bytes_read, total_sent = 0;
    
    while ((bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t bytes_written = write(client_fd, buf, bytes_read);
        if (bytes_written > 0) {
            total_sent += bytes_written;
        }
    }
    
    close(fd);
    
    log_request(ipc, "127.0.0.1", request_path, "GET", 200, total_sent);
    stats_update(ipc, 200, total_sent);
}

/**
 * Main HTTP request handler
 */
void http_handle_request(int client_fd, const char *document_root, ipc_handles_t *ipc) {
    if (client_fd < 0) {
        stats_update(ipc, 500, 0);
        return;
    }

    /* Read request */
    char request_buf[4096];
    ssize_t bytes_read = read(client_fd, request_buf, sizeof(request_buf) - 1);

    if (bytes_read <= 0) {
        if (bytes_read < 0 && errno != EINTR && errno != ECONNRESET) {
            perror("[HTTP] read failed");
        }
        close(client_fd);
        if (bytes_read < 0) stats_update(ipc, 500, 0);
        return;
    }

    request_buf[bytes_read] = '\0';

    /* Parse request */
    char *method = NULL, *path = NULL;
    if (parse_request(request_buf, &method, &path) < 0) {
        close(client_fd);
        stats_update(ipc, 500, 0);
        return;
    }

    printf("[HTTP] %s %s\n", method, path);

    /* Build full file path */
    char file_path[512];
    
    /* Handle root path */
    if (strcmp(path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s/index.html", document_root);
    } else {
        snprintf(file_path, sizeof(file_path), "%s%s", document_root, path);
    }

    /* Serve the file */
    serve_file(client_fd, file_path, path, ipc);

    /* Cleanup */
    free(method);
    free(path);
    close(client_fd);
}