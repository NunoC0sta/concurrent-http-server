#ifndef LOGGER_H
#define LOGGER_H

#include "master.h"

void log_request(ipc_handles_t *ipc, const char *client_ip, 
                 const char *request_path, const char *method,
                 int status_code, size_t bytes_sent);

// Inicializa o sistema de logging (abre o ficheiro)
int logger_init(const char *log_file);

// Fecha o sistema de logging no shutdown
void logger_cleanup(void);

#endif