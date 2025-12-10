#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include "logger.h"
#include "master.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

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

int parse_http_request(const char* buffer, char** method, char** path) {
    char* buf_copy = strdup(buffer);
    if (!buf_copy) return -1;
    char* line = strtok(buf_copy, "\r\n");
    if (!line) { free(buf_copy); return -1; }
    char* m = strtok(line, " ");
    char* p = strtok(NULL, " ");
    if (!m || !p) { free(buf_copy); return -1; }
    *method = strdup(m);
    *path = strdup(p);
    free(buf_copy);
    return 0;
}

void serve_dashboard(int client_fd, ipc_handles_t* ipc) {
    sem_wait(ipc->sem_stats);
    server_stats_t s = ipc->shared_data->stats;
    time_t uptime = time(NULL) - s.start_time;
    sem_post(ipc->sem_stats);

    char body[4096];
    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head><title>Monitor</title><meta http-equiv='refresh' content='3'>"
        "<style>body{font-family:sans-serif;padding:30px;background:#f4f4f4;}.card{background:white;padding:20px;margin:10px;border-radius:5px;box-shadow:0 2px 5px #ccc;}</style></head>"
        "<body><h1>Dashboard do Servidor</h1>"
        "<div class='card'><h3>Estado</h3>Uptime: %ld s | Ativos: %d</div>"
        "<div class='card'><h3>Trafego</h3>Total: %lu | Bytes: %lu</div>"
        "<div class='card'><h3>Erros</h3>404: %u | 500: %u</div></body></html>",
        uptime, s.active_connections, s.total_requests, s.bytes_transferred, s.status_404, s.status_500);

    char header[512];
    int hlen = snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", body_len);
    send_all(client_fd, header, hlen);
    send_all(client_fd, body, body_len);
}

void serve_file(int client_fd, const char* path, ipc_handles_t* ipc) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        char* msg = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send_all(client_fd, msg, strlen(msg));
        stats_update(ipc, 404, 0);
        return;
    }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char header[1024];
    int hlen = snprintf(header, sizeof(header), 
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: keep-alive\r\n\r\n", 
        get_mime_type(path), size);
    send_all(client_fd, header, hlen);
    char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) send_all(client_fd, buf, n);
    fclose(f);
    stats_update(ipc, 200, size);
}

void http_handle_request(int client_fd, const char *doc_root, ipc_handles_t *ipc) {
    struct timeval tv = {5, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    char buffer[4096];

    while (1) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';

        char *method = NULL, *path = NULL;
        if (parse_http_request(buffer, &method, &path) != 0) break;

        if (strcmp(path, "/stats") == 0) {
            serve_dashboard(client_fd, ipc);
            log_request(ipc, "127.0.0.1", "/stats", method, 200, 0);
        } else {
            char full[1024];
            if (strcmp(path, "/") == 0) snprintf(full, sizeof(full), "%s/index.html", doc_root);
            else snprintf(full, sizeof(full), "%s%s", doc_root, path);
            serve_file(client_fd, full, ipc);
            log_request(ipc, "127.0.0.1", path, method, 200, 0);
        }
        free(method); free(path);
    }
    close(client_fd);
}