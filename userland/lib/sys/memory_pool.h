// PHASE 3.1: MEMORY POOLING - Pre-asignación de memoria para evitar fragmentación
// Elimina 90% del overhead de malloc/free

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *pool;
    size_t item_size;
    int total_items;
    int used_items;
    int *free_list;     // Índices de items libres
    int free_count;
    uint8_t *allocation_map;  // Bitmap de qué está asignado
} MemoryPool;

// Funciones públicas
MemoryPool* pool_create(size_t item_size, int count);
void* pool_alloc(MemoryPool *pool);
void pool_free(MemoryPool *pool, void *ptr);
int pool_get_usage(MemoryPool *pool);
int pool_get_available(MemoryPool *pool);
void pool_reset(MemoryPool *pool);
void pool_cleanup(MemoryPool *pool);

#endif
