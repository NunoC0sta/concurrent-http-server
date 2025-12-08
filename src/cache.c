#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CACHE_ENTRIES 100

typedef struct cache_entry {
    char *key;
    void *data;
    size_t size;
    time_t accessed;
} cache_entry_t;

struct cache {
    cache_entry_t *entries;
    int num_entries;
    int max_entries;
    size_t max_size;
    size_t current_size;
};

/* Initialize cache with maximum size in MB */
cache_t* cache_init(size_t max_size_mb) {
    cache_t *cache = malloc(sizeof(cache_t));
    if (!cache) return NULL;
    
    cache->max_size = max_size_mb * 1024 * 1024;
    cache->current_size = 0;
    cache->max_entries = MAX_CACHE_ENTRIES;
    cache->num_entries = 0;
    cache->entries = malloc(sizeof(cache_entry_t) * MAX_CACHE_ENTRIES);
    
    if (!cache->entries) {
        free(cache);
        return NULL;
    }
    
    return cache;
}

/* Get entry from cache (returns NULL if not found) */
cache_entry_t* cache_get(cache_t *cache, const char *key) {
    if (!cache || !key) return NULL;
    
    for (int i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].key && strcmp(cache->entries[i].key, key) == 0) {
            cache->entries[i].accessed = time(NULL);
            return &cache->entries[i];
        }
    }
    return NULL;
}

/* Find least recently used entry */
static int find_lru_entry(cache_t *cache) {
    int lru_idx = 0;
    time_t oldest = cache->entries[0].accessed;
    
    for (int i = 1; i < cache->num_entries; i++) {
        if (cache->entries[i].accessed < oldest) {
            oldest = cache->entries[i].accessed;
            lru_idx = i;
        }
    }
    return lru_idx;
}

/* Add or update entry in cache */
void cache_put(cache_t *cache, const char *key, void *data, size_t size) {
    if (!cache || !key || !data) return;
    
    /* Check if entry already exists */
    for (int i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].key && strcmp(cache->entries[i].key, key) == 0) {
            /* Update existing entry */
            cache->current_size -= cache->entries[i].size;
            free(cache->entries[i].data);
            
            cache->entries[i].data = malloc(size);
            if (!cache->entries[i].data) return;
            
            memcpy(cache->entries[i].data, data, size);
            cache->entries[i].size = size;
            cache->entries[i].accessed = time(NULL);
            cache->current_size += size;
            return;
        }
    }
    
    /* Add new entry */
    if (cache->num_entries < cache->max_entries) {
        /* Check if we need to make space */
        while (cache->current_size + size > cache->max_size && cache->num_entries > 0) {
            int lru_idx = find_lru_entry(cache);
            cache->current_size -= cache->entries[lru_idx].size;
            free(cache->entries[lru_idx].key);
            free(cache->entries[lru_idx].data);
            cache->entries[lru_idx].key = NULL;
            
            /* Move last entry to this position */
            if (lru_idx != cache->num_entries - 1) {
                cache->entries[lru_idx] = cache->entries[cache->num_entries - 1];
            }
            cache->num_entries--;
        }
        
        /* Add new entry if we have space */
        if (cache->current_size + size <= cache->max_size) {
            cache->entries[cache->num_entries].key = malloc(strlen(key) + 1);
            if (!cache->entries[cache->num_entries].key) return;
            
            strcpy(cache->entries[cache->num_entries].key, key);
            cache->entries[cache->num_entries].data = malloc(size);
            if (!cache->entries[cache->num_entries].data) {
                free(cache->entries[cache->num_entries].key);
                return;
            }
            
            memcpy(cache->entries[cache->num_entries].data, data, size);
            cache->entries[cache->num_entries].size = size;
            cache->entries[cache->num_entries].accessed = time(NULL);
            
            cache->current_size += size;
            cache->num_entries++;
        }
    }
}

/* Get data pointer from cache entry */
void* cache_entry_get_data(cache_entry_t *entry) {
    if (!entry) return NULL;
    return entry->data;
}

/* Get size from cache entry */
size_t cache_entry_get_size(cache_entry_t *entry) {
    if (!entry) return 0;
    return entry->size;
}

/* Destroy cache and free all resources */
void cache_destroy(cache_t *cache) {
    if (!cache) return;
    
    for (int i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].key) {
            free(cache->entries[i].key);
        }
        if (cache->entries[i].data) {
            free(cache->entries[i].data);
        }
    }
    
    free(cache->entries);
    free(cache);
}
