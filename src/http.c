#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>


const char* get_mime_type(const char* path) {
    char *dot = strrchr(path, '.');
    if (!dot) return "text/plain";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    return "application/octet-stream";
}

ssize_t send_all(int sock, const char* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sock, buf + total, len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

int parse_http_request(const char* buffer, http_request_t* req) {
    char *line_end = strstr(buffer, "\r\n");
    if (!line_end) return -1;
    
    char first_line[1024];
    size_t len = line_end - buffer;
    if (len >= sizeof(first_line)) len = sizeof(first_line) - 1;
    strncpy(first_line, buffer, len);
    first_line[len] = '\0';
    
    if (sscanf(first_line, "%s %s %s", req->method, req->path, req->version) != 3) {
        return -1;
    }
    return 0;
}

void serve_file(int client_fd, const char* full_path, shared_data_t* shm, semaphores_t* sems, http_request_t* req) {
    FILE* file = fopen(full_path, "rb");
    if (!file) {
        const char* msg = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, msg, strlen(msg), 0);
        
        sem_wait(sems->stats_mutex);
        shm->stats.status_404++;
        shm->stats.total_requests++;
        sem_post(sems->stats_mutex);
        
        log_request(sems->log_mutex, "127.0.0.1", req->method, req->path, 404, 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char header[1024];
    const char* mime = get_mime_type(full_path);
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Server: ConcurrentHTTP/1.0\r\n\r\n", mime, fsize);

    send_all(client_fd, header, hlen);

    char fbuf[4096];
    size_t bytes_read;
    long total_bytes = hlen;
    
    while ((bytes_read = fread(fbuf, 1, sizeof(fbuf), file)) > 0) {
        send_all(client_fd, fbuf, bytes_read);
        total_bytes += bytes_read;
    }
    fclose(file);

    sem_wait(sems->stats_mutex);
    shm->stats.status_200++;
    shm->stats.bytes_transferred += total_bytes;
    shm->stats.total_requests++;
    sem_post(sems->stats_mutex);

    log_request(sems->log_mutex, "127.0.0.1", req->method, req->path, 200, total_bytes);
}

void http_handle_client(int client_fd, const char* doc_root, shared_data_t* shm, semaphores_t* sems) {
    char buffer[4096];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buffer[n] = '\0';

    http_request_t req;
    if (parse_http_request(buffer, &req) != 0) {
        close(client_fd);
        return;
    }

    char full_path[1024];
    if (strcmp(req.path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/index.html", doc_root);
    } else {
        snprintf(full_path, sizeof(full_path), "%s%s", doc_root, req.path);
    }

    serve_file(client_fd, full_path, shm, sems, &req);
    close(client_fd);
}