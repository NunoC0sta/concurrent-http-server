#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

/* Opaque type for thread pool */
typedef struct {
    int num_threads;
    pthread_t *threads;
    void *(*thread_fn)(void *);
    void *arg;
} thread_pool_t;

/*
 * Initialize pool; thread_fn will be started num_threads times with arg.
 * Returns 0 on success, -1 on failure.
 */
int thread_pool_init(thread_pool_t *pool, int num_threads, void *(*thread_fn)(void*), void *arg);

/* Shutdown pool and join threads. */
void thread_pool_shutdown(thread_pool_t *pool);

#endif // THREAD_POOL_H