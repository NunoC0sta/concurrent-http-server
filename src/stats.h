#ifndef STATS_H
#define STATS_H

#include "shared_mem.h"
#include "semaphores.h"

void stats_update(shared_data_t *shm, semaphores_t *sems, int status_code, uint64_t bytes);
void stats_display(shared_data_t *shm, semaphores_t *sems);

#endif