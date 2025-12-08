#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>
#include <pthread.h>

/* Opaque cache types */
typedef struct cache cache_t;
typedef struct cache_entry cache_entry_t;

/* Initialize cache with maximum size in MB */
cache_t* cache_init(size_t max_size_mb);

/* Get entry from cache (returns NULL if not found) */
cache_entry_t* cache_get(cache_t *cache, const char *key);

/* Add or update entry in cache */
void cache_put(cache_t *cache, const char *key, void *data, size_t size);

/* Get data pointer from cache entry */
void* cache_entry_get_data(cache_entry_t *entry);

/* Get size from cache entry */
size_t cache_entry_get_size(cache_entry_t *entry);

/* Destroy cache and free all resources */
void cache_destroy(cache_t *cache);

#endif // CACHE_H