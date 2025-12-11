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
#include <errno.h>
#include <time.h>

const char* get_mime_type(const char* path) {
    char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    return "text/plain";
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

// Helper para atualizar estatísticas com tempo
void update_stats_full(ipc_handles_t* ipc, int code, size_t bytes, long ms_taken) {
    sem_wait(ipc->sem_stats);
    server_stats_t *s = &ipc->shared_data->stats;
    s->total_requests++;
    s->bytes_transferred += bytes;
    s->total_response_time_ms += ms_taken;
    
    switch(code) {
        case 200: s->status_200++; break;
        case 403: s->status_403++; break;
        case 404: s->status_404++; break;
        case 500: s->status_500++; break;
        case 503: s->status_503++; break;
    }
    sem_post(ipc->sem_stats);
}

// --- DASHBOARD COMPLETO ---
void serve_dashboard(int client_fd, ipc_handles_t* ipc) {
    sem_wait(ipc->sem_stats);
    server_stats_t s = ipc->shared_data->stats;
    time_t now = time(NULL);
    time_t uptime = now - s.start_time;
    sem_post(ipc->sem_stats);

    // Calcular média de tempo de resposta
    double avg_time = 0.0;
    if (s.total_requests > 0) {
        avg_time = (double)s.total_response_time_ms / s.total_requests;
    }

    char body[8192];
    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head><title>Monitor Completo</title>"
        "<meta charset='UTF-8'>"
        "<meta http-equiv='refresh' content='2'>"
        "<style>"
        "body{font-family:'Segoe UI',sans-serif;padding:20px;background:#f0f2f5;color:#333;}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;}"
        ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 8px rgba(0,0,0,0.1);}"
        "h1{text-align:center;color:#1a73e8;margin-bottom:30px;}"
        "h2{font-size:1.1em;color:#5f6368;border-bottom:1px solid #eee;padding-bottom:10px;margin-top:0;}"
        ".row{display:flex;justify-content:space-between;margin:8px 0;font-size:1.05em;}"
        ".val{font-weight:bold;color:#1a73e8;}"
        ".err{color:#d93025;} .ok{color:#188038;}"
        "</style></head>"
        "<body>"
        "<h1>📊 Dashboard do Servidor</h1>"
        "<div class='grid'>"
        
        "<div class='card'><h2>🚀 Performance</h2>"
        "<div class='row'><span>Uptime:</span> <span class='val'>%ld s</span></div>"
        "<div class='row'><span>Conexões Ativas:</span> <span class='val'>%d</span></div>"
        "<div class='row'><span>Tempo Médio:</span> <span class='val'>%.2f ms</span></div>"
        "</div>"
        
        "<div class='card'><h2>📡 Tráfego</h2>"
        "<div class='row'><span>Total Pedidos:</span> <span class='val'>%lu</span></div>"
        "<div class='row'><span>Dados Enviados:</span> <span class='val'>%.2f MB</span></div>"
        "</div>"
        
        "<div class='card'><h2>📝 Códigos de Resposta</h2>"
        "<div class='row'><span>200 OK:</span> <span class='val ok'>%u</span></div>"
        "<div class='row'><span>403 Forbidden:</span> <span class='val err'>%u</span></div>"
        "<div class='row'><span>404 Not Found:</span> <span class='val err'>%u</span></div>"
        "<div class='row'><span>500 Error:</span> <span class='val err'>%u</span></div>"
        "<div class='row'><span>503 Busy:</span> <span class='val err'>%u</span></div>"
        "</div>"
        
        "</div></body></html>",
        uptime, s.active_connections, avg_time,
        s.total_requests, (double)s.bytes_transferred / (1024*1024),
        s.status_200, s.status_403, s.status_404, s.status_500, s.status_503);

    char header[512];
    int hlen = snprintf(header, sizeof(header),
        // CORREÇÃO 2: Adicionado charset=utf-8 no cabeçalho HTTP
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %d\r\n\r\n", body_len);
    
    send_all(client_fd, header, hlen);
    send_all(client_fd, body, body_len);
}

void serve_file(int client_fd, const char* path, ipc_handles_t* ipc, long* ms_taken) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    FILE* f = fopen(path, "rb");
    if (!f) {
        char* msg = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send_all(client_fd, msg, strlen(msg));
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        *ms_taken = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
        
        update_stats_full(ipc, 404, 0, *ms_taken);
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
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    *ms_taken = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;

    update_stats_full(ipc, 200, size, *ms_taken);
}

void http_handle_request(int client_fd, const char *document_root, ipc_handles_t *ipc) {
    struct timeval tv = {5, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    char buffer[4096];

    while (1) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';

        char *method = NULL, *path = NULL;
        if (parse_http_request(buffer, &method, &path) != 0) break;

        long ms_taken = 0;

        if (strcmp(path, "/stats") == 0) {
            serve_dashboard(client_fd, ipc);
            log_request(ipc, "127.0.0.1", "/stats", method, 200, 0);
        } else {
            char full_path[1024];
            if (strcmp(path, "/") == 0) snprintf(full_path, sizeof(full_path), "%s/index.html", document_root);
            else snprintf(full_path, sizeof(full_path), "%s%s", document_root, path);
            
            serve_file(client_fd, full_path, ipc, &ms_taken);
            log_request(ipc, "127.0.0.1", path, method, 200, 0);
        }
        
        free(method); free(path);
    }
    close(client_fd);
}