#define _POSIX_C_SOURCE 200809L
#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void cache_init(file_cache_t* cache, size_t max_size_mb) {
    cache->head = NULL;
    cache->current_size = 0;
    // Converter MB para Bytes (10MB = 10 * 1024 * 1024)
    cache->max_size = max_size_mb * 1024 * 1024;
    pthread_rwlock_init(&cache->lock, NULL);
}

void cache_destroy(file_cache_t* cache) {
    pthread_rwlock_wrlock(&cache->lock);
    cache_entry_t* current = cache->head;
    while (current) {
        cache_entry_t* next = current->next;
        free(current->path);
        free(current->data);
        free(current);
        current = next;
    }
    pthread_rwlock_unlock(&cache->lock);
    pthread_rwlock_destroy(&cache->lock);
}

// Retorna 0 se encontrou, -1 se não encontrou
int cache_get(file_cache_t* cache, const char* path, void** out_data, size_t* out_size) {
    // Lock de Leitura (múltiplas threads podem ler ao mesmo tempo)
    pthread_rwlock_rdlock(&cache->lock);
    
    cache_entry_t* current = cache->head;
    while (current) {
        if (strcmp(current->path, path) == 0) {

            *out_data = current->data; // Retorna ponteiro direto
            *out_size = current->size;
            
            current->last_access = time(NULL);
            
            pthread_rwlock_unlock(&cache->lock);
            return 0;
        }
        current = current->next;
    }
    
    pthread_rwlock_unlock(&cache->lock);
    return -1; 
}

// Remove o item mais antigo se a cache estiver cheia
void evict_lru(file_cache_t* cache) {
    if (!cache->head) return;

    cache_entry_t *prev = NULL, *current = cache->head;
    cache_entry_t *lru_prev = NULL, *lru_node = cache->head;
    
    // Encontrar o nó com 'last_access' mais antigo
    while (current) {
        if (current->last_access < lru_node->last_access) {
            lru_node = current;
            lru_prev = prev;
        }
        prev = current;
        current = current->next;
    }

    // Remover o nó LRU
    if (lru_prev) {
        lru_prev->next = lru_node->next;
    } else {
        cache->head = lru_node->next;
    }

    cache->current_size -= lru_node->size;
    free(lru_node->path);
    free(lru_node->data);
    free(lru_node);
}

void cache_put(file_cache_t* cache, const char* path, const void* data, size_t size) {
    if (size > 1024 * 1024) return;

    // Lock de Escrita
    pthread_rwlock_wrlock(&cache->lock);

    // Verificar se já temos espaço
    while (cache->current_size + size > cache->max_size) {
        evict_lru(cache);
    }

    // Criar nova entrada
    cache_entry_t* new_entry = malloc(sizeof(cache_entry_t));
    new_entry->path = strdup(path);
    new_entry->data = malloc(size);
    memcpy(new_entry->data, data, size);
    new_entry->size = size;
    new_entry->last_access = time(NULL);
    
    // Inserir no início da lista
    new_entry->next = cache->head;
    cache->head = new_entry;
    cache->current_size += size;

    pthread_rwlock_unlock(&cache->lock);
}