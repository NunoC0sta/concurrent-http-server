#ifndef MASTER_H
#define MASTER_H

#include "config.h"

/* Master control functions */
int master_init(server_config_t *config);
void master_accept_loop(void);
void master_cleanup(void);

#endif // MASTER_H