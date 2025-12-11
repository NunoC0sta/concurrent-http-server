#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>
#include <pthread.h>

// Tipos opacos.
typedef struct cache cache_t;
typedef struct cache_entry cache_entry_t;

// Inicializa a estrutura da cache
cache_t* cache_init(size_t max_size_mb);

// Tenta encontrar uma entrada na cache através da chave
cache_entry_t* cache_get(cache_t *cache, const char *key);

// Adiciona um novo item à cache. Se a chave já existir, atualiza os dados e o tamanho
void cache_put(cache_t *cache, const char *key, void *data, size_t size);

// Função auxiliar para aceder aos dados da entrada
void* cache_entry_get_data(cache_entry_t *entry);

// Helper para obter o tamanho dos dados guardados numa entrada
size_t cache_entry_get_size(cache_entry_t *entry);

// Limpa toda a memória alocada e destrói os locks/recursos associados
void cache_destroy(cache_t *cache);

#endif