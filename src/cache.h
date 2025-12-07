#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>
#include <time.h>
#include <pthread.h>

// Estrutura para cada ficheiro em cache
typedef struct cache_entry {
    char* path;
    void* data;
    size_t size;
    time_t last_access;
    struct cache_entry* next;
} cache_entry_t;

typedef struct {
    cache_entry_t* head;
    size_t current_size;
    size_t max_size;
    pthread_rwlock_t lock;
} file_cache_t;

// Funções
void cache_init(file_cache_t* cache, size_t max_size_mb);
void cache_destroy(file_cache_t* cache);
int cache_get(file_cache_t* cache, const char* path, void** out_data, size_t* out_size);
void cache_put(file_cache_t* cache, const char* path, const void* data, size_t size);

#endif