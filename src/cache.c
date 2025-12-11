#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Limite fixo de entradas
#define MAX_CACHE_ENTRIES 100

typedef struct cache_entry {
    char *key;
    void *data;
    size_t size;
    time_t accessed; // Timestamp para gerir o LRU
} cache_entry_t;

struct cache {
    cache_entry_t *entries;
    int num_entries;
    int max_entries;
    size_t max_size;
    size_t current_size;
    pthread_rwlock_t rwlock; // Lock de leitura/escrita para garantir thread-safety
};

// Inicializa a cache e define o tamanho máximo em MB
cache_t* cache_init(size_t max_size_mb) {
    cache_t *cache = malloc(sizeof(cache_t));
    if (!cache) return NULL; // Se o malloc falhar retorna nulll
    
    cache->max_size = max_size_mb * 1024 * 1024;
    cache->current_size = 0;
    cache->max_entries = MAX_CACHE_ENTRIES;
    cache->num_entries = 0;
    cache->entries = malloc(sizeof(cache_entry_t) * MAX_CACHE_ENTRIES);
    
    if (!cache->entries) {
        free(cache);
        return NULL;
    }
    
// Inicializa o lock e verifica se correu bem.
    if (pthread_rwlock_init(&cache->rwlock, NULL) != 0) {
        free(cache->entries);
        free(cache);
        return NULL;
    }
    
    return cache;
}

cache_entry_t* cache_get(cache_t *cache, const char *key) {
    if (!cache || !key) return NULL;
    
    // Bloqueia para leitura. Várias threads podem ler ao mesmo tempo
    pthread_rwlock_rdlock(&cache->rwlock);
    
    for (int i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].key && strcmp(cache->entries[i].key, key) == 0) {
            //  Liberta o lock de leitura para adquirir o de escrita e atualizar o timestamp.
            pthread_rwlock_unlock(&cache->rwlock);
            
            pthread_rwlock_wrlock(&cache->rwlock);
            cache->entries[i].accessed = time(NULL); // Atualiza acesso (para o LRU)
            pthread_rwlock_unlock(&cache->rwlock);
            
            return &cache->entries[i];
        }
    }
    pthread_rwlock_unlock(&cache->rwlock);
    return NULL;
}

// Encontra o índice da entrada menos usada recentemente
static int find_lru_entry(cache_t *cache) {
    int lru_idx = 0;
    time_t oldest = cache->entries[0].accessed;
    
    // Tenta linear pelo timestamp mais antigo
    for (int i = 1; i < cache->num_entries; i++) {
        if (cache->entries[i].accessed < oldest) {
            oldest = cache->entries[i].accessed;
            lru_idx = i;
        }
    }
    return lru_idx;
}

// Adiciona ou atualiza a entrada. Precisa de lock de escrita exclusivo
void cache_put(cache_t *cache, const char *key, void *data, size_t size) {
    if (!cache || !key || !data) return;
    
    pthread_rwlock_wrlock(&cache->rwlock);
    
    // Verifica se a chave já existe
    for (int i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].key && strcmp(cache->entries[i].key, key) == 0) {
            // Atualiza a entrada existente 
            cache->current_size -= cache->entries[i].size;
            free(cache->entries[i].data); // Liberta os dados antigos
            
            cache->entries[i].data = malloc(size);
            if (!cache->entries[i].data) {
                pthread_rwlock_unlock(&cache->rwlock); // Evitar deadlock se falhar
                return;
            }
            
            memcpy(cache->entries[i].data, data, size);
            cache->entries[i].size = size;
            cache->entries[i].accessed = time(NULL);
            cache->current_size += size;
            pthread_rwlock_unlock(&cache->rwlock);
            return;
        }
    }
    
    // Se for nova entrada, verifica limites
    if (cache->num_entries < cache->max_entries) {
        // Se o tamanho exceder o máximo, remove o mais antigo (LRU)
        while (cache->current_size + size > cache->max_size && cache->num_entries > 0) {
            int lru_idx = find_lru_entry(cache);
            
            // Remove a entrada LRU
            cache->current_size -= cache->entries[lru_idx].size;
            free(cache->entries[lru_idx].key);
            free(cache->entries[lru_idx].data);
            cache->entries[lru_idx].key = NULL;
            
            // Move o último elemento para a posição vazia para manter o array contínuo
            if (lru_idx != cache->num_entries - 1) {
                cache->entries[lru_idx] = cache->entries[cache->num_entries - 1];
            }
            cache->num_entries--;
        }
        
        // Finalmente, adiciona a nova entrada se houver espaço 
        if (cache->current_size + size <= cache->max_size) {
            cache->entries[cache->num_entries].key = malloc(strlen(key) + 1);
            if (!cache->entries[cache->num_entries].key) {
                pthread_rwlock_unlock(&cache->rwlock);
                return;
            }
            
            strcpy(cache->entries[cache->num_entries].key, key);
            cache->entries[cache->num_entries].data = malloc(size);
            if (!cache->entries[cache->num_entries].data) {
                free(cache->entries[cache->num_entries].key); // Limpa a chave se falhar a alocação dos dados
                pthread_rwlock_unlock(&cache->rwlock);
                return;
            }
            
            memcpy(cache->entries[cache->num_entries].data, data, size);
            cache->entries[cache->num_entries].size = size;
            cache->entries[cache->num_entries].accessed = time(NULL);
            
            cache->current_size += size;
            cache->num_entries++;
        }
    }
    
    pthread_rwlock_unlock(&cache->rwlock);
}

/// Retorna o ponteiro para os dados 
void* cache_entry_get_data(cache_entry_t *entry) {
    if (!entry) return NULL;
    return entry->data;
}

// Retorna o tamanho da entrada 
size_t cache_entry_get_size(cache_entry_t *entry) {
    if (!entry) return 0;
    return entry->size;
}

// "Destrói" a cache e liberta todos os recursos
void cache_destroy(cache_t *cache) {
    if (!cache) return;
    
    // Garante que ninguém está a usar a cache antes de a "destruir"
    pthread_rwlock_wrlock(&cache->rwlock);
    
    for (int i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].key) {
            free(cache->entries[i].key);
        }
        if (cache->entries[i].data) {
            free(cache->entries[i].data);
        }
    }
    
    pthread_rwlock_unlock(&cache->rwlock);
    pthread_rwlock_destroy(&cache->rwlock);
    
    free(cache->entries);
    free(cache);
}