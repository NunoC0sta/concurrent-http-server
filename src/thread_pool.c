#include "thread_pool.h"
#include <stdlib.h>
#include <pthread.h>

thread_pool_t* create_thread_pool(int num_threads) {
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->num_threads = num_threads;
    pool->shutdown = 0;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);
    
    return pool;
}

void destroy_thread_pool(thread_pool_t* pool) {
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond);
    for(int i=0; i<pool->num_threads; i++) {
    }
    free(pool->threads);
    free(pool);
}