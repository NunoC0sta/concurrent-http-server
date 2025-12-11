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
#include <ctype.h>

const char* get_mime_type(const char* path) {
    char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    if (strcmp(dot, ".mp4") == 0) return "video/mp4";
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

// --- NOVO: Função auxiliar para ler cabeçalhos (Host, Range) ---
char* get_header(const char* buffer, const char* header_name) {
    static char value[1024];
    char* line = strstr(buffer, header_name);
    if (!line) return NULL;
    
    line += strlen(header_name);
    while (*line == ':' || *line == ' ') line++;
    
    int i = 0;
    while (*line != '\r' && *line != '\n' && i < 1023) {
        value[i++] = *line++;
    }
    value[i] = '\0';
    return value;
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

void serve_custom_error(int client_fd, int code, const char* doc_root, ipc_handles_t* ipc) {
    char error_path[1024];
    snprintf(error_path, sizeof(error_path), "%s/errors/%d.html", doc_root, code);
    FILE* f = fopen(error_path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
        char header[512];
        int hlen = snprintf(header, sizeof(header), "HTTP/1.1 %d Error\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", code, size);
        send_all(client_fd, header, hlen);
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) send_all(client_fd, buf, n);
        fclose(f);
    } else {
        char msg[128]; int len = snprintf(msg, sizeof(msg), "HTTP/1.1 %d Error\r\nContent-Length: 0\r\n\r\n", code);
        send_all(client_fd, msg, len);
    }
    sem_wait(ipc->sem_stats);
    ipc->shared_data->stats.total_requests++;
    if(code==404) ipc->shared_data->stats.status_404++;
    else if(code==500) ipc->shared_data->stats.status_500++;
    else if(code==403) ipc->shared_data->stats.status_403++;
    else if(code==503) ipc->shared_data->stats.status_503++;
    sem_post(ipc->sem_stats);
}

// --- O TEU DASHBOARD (INTOCÁVEL) ---
void serve_dashboard(int client_fd, ipc_handles_t* ipc) {
    sem_wait(ipc->sem_stats);
    server_stats_t s = ipc->shared_data->stats;
    time_t now = time(NULL);
    time_t uptime = now - s.start_time;
    sem_post(ipc->sem_stats);

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
        ".err{color:#d93025;} .ok{color:#188038;} .warn{color:#f57f17;}"
        "</style></head>"
        "<body>"
        "<h1>Dashboard do Servidor</h1>"
        "<div class='grid'>"
        
        "<div class='card'><h2>Performance</h2>"
        "<div class='row'><span>Uptime:</span> <span class='val'>%ld s</span></div>"
        "<div class='row'><span>Conexões Ativas:</span> <span class='val'>%d</span></div>"
        "<div class='row'><span>Tempo Médio:</span> <span class='val'>%.2f ms</span></div>"
        "</div>"
        
        "<div class='card'><h2>Tráfego</h2>"
        "<div class='row'><span>Total Pedidos:</span> <span class='val'>%lu</span></div>"
        "<div class='row'><span>Dados Enviados:</span> <span class='val'>%.2f MB</span></div>"
        "</div>"
        
        "<div class='card'><h2>Códigos de Resposta</h2>"
        "<div class='row'><span>200 OK:</span> <span class='val ok'>%u</span></div>"
        "<div class='row'><span>403 Forbidden:</span> <span class='val warn'>%u</span></div>"
        "<div class='row'><span>404 Not Found:</span> <span class='val warn'>%u</span></div>"
        "<div class='row'><span>500 Error:</span> <span class='val err'>%u</span></div>"
        "<div class='row'><span>503 Busy:</span> <span class='val err'>%u</span></div>"
        "</div>"
        
        "</div></body></html>",
        uptime, s.active_connections, avg_time,
        s.total_requests, (double)s.bytes_transferred / (1024*1024),
        s.status_200, s.status_403, s.status_404, s.status_500, s.status_503);

    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %d\r\n\r\n", body_len);
    
    send_all(client_fd, header, hlen);
    send_all(client_fd, body, body_len);
}

// --- FUNÇÃO ATUALIZADA: Suporta Range Requests (+3 Pontos) ---
void serve_file(int client_fd, const char* path, const char* doc_root, ipc_handles_t* ipc, const char* range_header) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    FILE* f = fopen(path, "rb");
    if (!f) {
        serve_custom_error(client_fd, 404, doc_root, ipc);
        return;
    }

    fseek(f, 0, SEEK_END); long filesize = ftell(f); fseek(f, 0, SEEK_SET);
    
    // Lógica para Range Request (Resumo de download)
    long start_byte = 0;
    long end_byte = filesize - 1;
    int is_partial = 0;

    if (range_header) {
        // Tenta ler "bytes=0-100"
        if (sscanf(range_header, "bytes=%ld-%ld", &start_byte, &end_byte) >= 1) {
            if (end_byte >= filesize || end_byte == 0) end_byte = filesize - 1;
            is_partial = 1;
            fseek(f, start_byte, SEEK_SET);
        }
    }

    long content_len = end_byte - start_byte + 1;
    char header[1024];
    int hlen;

    if (is_partial) {
        // Resposta 206 Partial Content
        hlen = snprintf(header, sizeof(header), 
            "HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nContent-Range: bytes %ld-%ld/%ld\r\nContent-Length: %ld\r\nConnection: keep-alive\r\n\r\n", 
            get_mime_type(path), start_byte, end_byte, filesize, content_len);
    } else {
        // Resposta 200 OK (Normal)
        hlen = snprintf(header, sizeof(header), 
            "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: keep-alive\r\n\r\n", 
            get_mime_type(path), filesize);
    }

    send_all(client_fd, header, hlen);

    char buf[8192];
    long sent = 0;
    while (sent < content_len) {
        // Lê apenas o necessário
        long to_read = (content_len - sent) > (long)sizeof(buf) ? (long)sizeof(buf) : (content_len - sent);
        size_t n = fread(buf, 1, to_read, f);
        if (n <= 0) break;
        send_all(client_fd, buf, n);
        sent += n;
    }
    fclose(f);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    long ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
    
    sem_wait(ipc->sem_stats);
    ipc->shared_data->stats.total_requests++;
    ipc->shared_data->stats.bytes_transferred += sent;
    ipc->shared_data->stats.status_200++;
    ipc->shared_data->stats.total_response_time_ms += ms;
    sem_post(ipc->sem_stats);
}

void http_handle_request(int client_fd, const char *default_root, ipc_handles_t *ipc) {
    struct timeval tv = {5, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    char buffer[4096];

    while (1) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';

        char *method = NULL, *path = NULL;
        if (parse_http_request(buffer, &method, &path) != 0) break;

        // --- BÓNUS: VIRTUAL HOSTS (+4 Pontos) ---
        char current_root[1024];
        char* host = get_header(buffer, "Host");
        char* range = get_header(buffer, "Range"); // Ler o Range header também

        if (host && strstr(host, "site1")) {
            snprintf(current_root, sizeof(current_root), "./www/site1");
        } else if (host && strstr(host, "site2")) {
            snprintf(current_root, sizeof(current_root), "./www/site2");
        } else {
            // Se não for site1 nem site2, usa a raiz normal (./www)
            strcpy(current_root, default_root);
        }

        if (strcmp(path, "/stats") == 0) {
            serve_dashboard(client_fd, ipc);
            log_request(ipc, "127.0.0.1", "/stats", method, 200, 0);
        } else {
            char full[2048]; // Buffer maior para o path
            if (strcmp(path, "/") == 0) snprintf(full, sizeof(full), "%s/index.html", current_root);
            else snprintf(full, sizeof(full), "%s%s", current_root, path);
            
            // Passamos agora o 'range' para a função serve_file
            serve_file(client_fd, full, current_root, ipc, range);
            
            log_request(ipc, "127.0.0.1", path, method, 200, 0);
        }
        free(method); free(path);
    }
    close(client_fd);
}