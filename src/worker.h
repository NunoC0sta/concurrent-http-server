#ifndef WORKER_H
#define WORKER_H

#include "config.h"

/* Worker main function - called by each worker process
 * worker_id: worker number (0 to num_workers-1)
 * config: server configuration
 * listen_fd: shared listening socket (workers accept directly)
 */
void worker_main(int worker_id, server_config_t *config, int listen_fd);

#endif // WORKER_H