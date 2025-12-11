#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

typedef struct {
    int num_threads;
    pthread_t *threads;
    void *(*thread_fn)(void *);
    void *arg;
} thread_pool_t;

int thread_pool_init(thread_pool_t *pool, int num_threads, void *(*thread_fn)(void*), void *arg);

void thread_pool_shutdown(thread_pool_t *pool);

#endif