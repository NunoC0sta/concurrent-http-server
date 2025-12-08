#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include "logger.h"
#include "cache.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

/**
 * Validate that the resolved path stays within document root
 * Returns 1 if path is safe, 0 if it attempts directory traversal
 */
static int is_safe_path(const char *document_root, const char *requested_path) {
    if (!requested_path || !document_root) return 0;
    
    /* Simple check: reject any path containing .. */
    if (strstr(requested_path, "..") != NULL) {
        return 0;
    }
    
    /* Reject paths starting with / (absolute paths) */
    if (requested_path[0] == '/' && requested_path[1] == '/') {
        return 0;
    }
    
    return 1;  /* Safe path */
}

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
 * Extract Content-Length from HTTP headers
 * Returns the content length, or 0 if not found/invalid
 */
static size_t get_content_length(const char *headers) {
    if (!headers) return 0;
    
    const char *cl_start = strstr(headers, "Content-Length:");
    if (!cl_start) return 0;
    
    cl_start += 15; /* Skip "Content-Length:" */
    
    /* Skip whitespace */
    while (*cl_start == ' ' || *cl_start == '\t') cl_start++;
    
    size_t length = 0;
    if (sscanf(cl_start, "%zu", &length) != 1) {
        return 0;
    }
    
    return length;
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
    
    /* Get current time for Date header (RFC 1123 format) */
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    
    int len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Date: %s\r\n"
        "Server: ConcurrentHTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, content_length, date_buf);
    
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
 * Serve server statistics
 */
static void serve_stats(int client_fd, ipc_handles_t *ipc) {
    char body[2048];
    int len = snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>Server Statistics</title></head>\n"
        "<body style='font-family: monospace; margin: 20px;'>\n"
        "<h1>Server Statistics</h1>\n"
        "<p>For detailed stats, use the JSON endpoint or check logs.</p>\n"
        "<pre>\n"
        "Server is running and accepting requests.\n"
        "View access.log for request details.\n"
        "</pre>\n"
        "</body>\n"
        "</html>\n"
    );
    
    send_response_headers(client_fd, 200, "text/html", len);
    write(client_fd, body, len);
    
    log_request(ipc, "127.0.0.1", "/stats", "GET", 200, len);
    stats_update(ipc, 200, len);
}

/**
 * Main HTTP request handler
 */
void http_handle_request(int client_fd, const char *document_root, ipc_handles_t *ipc) {
    if (client_fd < 0) {
        stats_update(ipc, 500, 0);
        return;
    }

    /* Read request headers */
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
    
    /* Record start time for response time tracking */
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    /* Read request body if POST/PUT with Content-Length */
    char *body = NULL;
    size_t body_len = 0;
    if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
        size_t content_len = get_content_length(request_buf);
        if (content_len > 0 && content_len <= 65536) {  /* Limit to 64KB */
            body = malloc(content_len + 1);
            if (body) {
                /* Find end of headers (double CRLF) */
                char *body_start = strstr(request_buf, "\r\n\r\n");
                if (body_start) {
                    body_start += 4;
                    size_t headers_body_len = strlen(body_start);
                    
                    /* Copy any body data already in initial read */
                    if (headers_body_len > 0) {
                        memcpy(body, body_start, headers_body_len);
                        body_len = headers_body_len;
                    }
                    
                    /* Read remaining body data if needed */
                    while (body_len < content_len) {
                        ssize_t n = read(client_fd, body + body_len, content_len - body_len);
                        if (n <= 0) break;
                        body_len += n;
                    }
                }
                body[body_len] = '\0';
                printf("[HTTP] Body: %zu bytes\n", body_len);
            }
        }
    }

    /* Handle special endpoints */
    if (strcmp(path, "/stats") == 0) {
        serve_stats(client_fd, ipc);
        free(method);
        free(path);
        free(body);
        close(client_fd);
        stats_record_response_time(ipc, &start_time);
        return;
    }

    /* Check for path traversal attacks */
    if (!is_safe_path(document_root, path)) {
        const char *resp_body = "<html><body><h1>403 Forbidden</h1></body></html>";
        size_t resp_len = strlen(resp_body);
        
        send_response_headers(client_fd, 403, "text/html", resp_len);
        write(client_fd, resp_body, resp_len);
        
        log_request(ipc, "127.0.0.1", path, method, 403, resp_len);
        stats_update(ipc, 403, resp_len);
        
        free(method);
        free(path);
        free(body);
        close(client_fd);
        stats_record_response_time(ipc, &start_time);
        return;
    }

    /* Handle HEAD method (same as GET but without body) */
    if (strcmp(method, "HEAD") == 0) {
        /* Build full file path */
        char file_path[512];
        if (strcmp(path, "/") == 0) {
            snprintf(file_path, sizeof(file_path), "%s/index.html", document_root);
        } else {
            snprintf(file_path, sizeof(file_path), "%s%s", document_root, path);
        }
        
        struct stat st;
        if (stat(file_path, &st) < 0) {
            send_response_headers(client_fd, 404, "text/html", 0);
            log_request(ipc, "127.0.0.1", path, "HEAD", 404, 0);
            stats_update(ipc, 404, 0);
        } else if (S_ISDIR(st.st_mode)) {
            char index_path[1024];
            snprintf(index_path, sizeof(index_path), "%s/index.html", file_path);
            if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
                const char *mime_type = get_mime_type(index_path);
                send_response_headers(client_fd, 200, mime_type, st.st_size);
                log_request(ipc, "127.0.0.1", path, "HEAD", 200, 0);
                stats_update(ipc, 200, 0);
            } else {
                send_response_headers(client_fd, 403, "text/html", 0);
                log_request(ipc, "127.0.0.1", path, "HEAD", 403, 0);
                stats_update(ipc, 403, 0);
            }
        } else {
            const char *mime_type = get_mime_type(file_path);
            send_response_headers(client_fd, 200, mime_type, st.st_size);
            log_request(ipc, "127.0.0.1", path, "HEAD", 200, 0);
            stats_update(ipc, 200, 0);
        }
        
        free(method);
        free(path);
        free(body);
        close(client_fd);
        stats_record_response_time(ipc, &start_time);
        return;
    }

    /* Handle POST/PUT by accepting and echoing body */
    if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
        char resp_body[1024];
        int resp_len = snprintf(resp_body, sizeof(resp_body),
            "<html><body><h1>%s Request Received</h1>"
            "<p>Path: %s</p>"
            "<p>Body Length: %zu bytes</p></body></html>",
            method, path, body_len);
        
        send_response_headers(client_fd, 201, "text/html", resp_len);
        write(client_fd, resp_body, resp_len);
        
        log_request(ipc, "127.0.0.1", path, method, 201, resp_len);
        stats_update(ipc, 201, resp_len);
        
        free(method);
        free(path);
        free(body);
        close(client_fd);
        stats_record_response_time(ipc, &start_time);
        return;
    }

    /* Build full file path for GET requests */
    char file_path[512];
    
    /* Handle root path */
    if (strcmp(path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s/index.html", document_root);
    } else {
        snprintf(file_path, sizeof(file_path), "%s%s", document_root, path);
    }

    /* Serve the file */
    serve_file(client_fd, file_path, path, ipc);

    /* Record response time */
    stats_record_response_time(ipc, &start_time);
    
    /* Cleanup */
    free(method);
    free(path);
    free(body);
    close(client_fd);
}