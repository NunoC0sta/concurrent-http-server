#define _POSIX_C_SOURCE 200809L
#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

int thread_pool_init(thread_pool_t *pool, int num_threads, void *(*thread_fn)(void*), void *arg) {
    if (!pool || num_threads <= 0 || !thread_fn) return -1;

    printf("[THREAD_POOL] Initializing with %d threads\n", num_threads);
    fflush(stdout);

    pool->num_threads = num_threads;
    pool->thread_fn = thread_fn;
    pool->arg = arg;

    // Alocar array para guardar os IDs das threads
    pool->threads = calloc(num_threads, sizeof(pthread_t));
    if (!pool->threads) return -1;

    for (int i = 0; i < num_threads; ++i) {
        printf("[THREAD_POOL] Creating thread %d...\n", i);
        fflush(stdout);
        
        // Lançar a thread
        int rc = pthread_create(&pool->threads[i], NULL, thread_fn, arg);
        if (rc != 0) {
            fprintf(stderr, "[THREAD_POOL] pthread_create failed with error %d\n", rc);
            perror("pthread_create");
            
            // Se falhar a criação de uma thread, tenta limpar as que já foram criadas
            for (int j = 0; j < i; ++j) {
                pthread_kill(pool->threads[j], SIGTERM); // Tenta enviar sinal para interromper
            }
            for (int j = 0; j < i; ++j) {
                pthread_join(pool->threads[j], NULL); // Espera que as interrompidas terminem
            }
            free(pool->threads);
            pool->threads = NULL;
            return -1;
        }
        printf("[THREAD_POOL] Thread %d created successfully (tid=%lu)\n", 
               i, (unsigned long)pool->threads[i]);
        fflush(stdout);
    }
    
    printf("[THREAD_POOL] All %d threads created\n", num_threads);
    fflush(stdout);
    
    return 0;
}

void thread_pool_shutdown(thread_pool_t *pool) {
    if (!pool || !pool->threads) return;

    for (int i = 0; i < pool->num_threads; ++i) {
        pthread_kill(pool->threads[i], SIGTERM);
    }

    // Espera que todas as threads terminem
    for (int i = 0; i < pool->num_threads; ++i) {
        pthread_join(pool->threads[i], NULL);
    }

    // Liberta a memória do array de IDs
    free(pool->threads);    
    pool->threads = NULL;
}