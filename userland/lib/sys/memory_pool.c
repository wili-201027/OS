// PHASE 3.1: MEMORY POOLING - Implementación
// Pool de memoria con free list para O(1) asignación

#include "memory_pool.h"
#include "../../libc/stdlib.h"
#include "../../libc/string.h"

MemoryPool* pool_create(size_t item_size, int count) {
    if (item_size == 0 || count <= 0) return NULL;
    
    MemoryPool *pool = malloc(sizeof(MemoryPool));
    if (!pool) return NULL;
    
    // Asignar bloque de memoria continua
    pool->pool = malloc(item_size * count);
    if (!pool->pool) {
        free(pool);
        return NULL;
    }
    
    // Asignar bitmap de allocations (1 bit por item) y poner a 0 con calloc
    pool->allocation_map = calloc((count + 7) / 8, 1);
    if (!pool->allocation_map) {
        free(pool->pool);
        free(pool);
        return NULL;
    }
    
    // Asignar free list
    pool->free_list = malloc(sizeof(int) * count);
    if (!pool->free_list) {
        free(pool->allocation_map);
        free(pool->pool);
        free(pool);
        return NULL;
    }
    
    pool->item_size = item_size;
    pool->total_items = count;
    pool->used_items = 0;
    pool->free_count = count;
    
    // Inicializar free list: todos los items están disponibles
    for(int i = 0; i < count; i++) {
        pool->free_list[i] = i;
    }
    
    
    return pool;
}

void* pool_alloc(MemoryPool *pool) {
    if (!pool || pool->free_count <= 0) {
        return NULL;  // Sin memoria disponible
    }
    
    // Obtener próximo item libre de free list
    int item_idx = pool->free_list[pool->free_count - 1];
    pool->free_count--;
    pool->used_items++;
    
    // Marcar como asignado en bitmap
    int byte_idx = item_idx / 8;
    int bit_idx = item_idx % 8;
    pool->allocation_map[byte_idx] |= (1 << bit_idx);
    
    // Retornar pointer al item
    return (uint8_t*)pool->pool + (item_idx * pool->item_size);
}

void pool_free(MemoryPool *pool, void *ptr) {
    if (!pool || !ptr) return;
    
    // Calcular índice del item
    size_t offset = (uint8_t*)ptr - (uint8_t*)pool->pool;
    int item_idx = offset / pool->item_size;
    
    // Validar que está dentro del rango
    if (item_idx < 0 || item_idx >= pool->total_items) {
        return;
    }
    
    // Marcar como libre en bitmap
    int byte_idx = item_idx / 8;
    int bit_idx = item_idx % 8;
    pool->allocation_map[byte_idx] &= ~(1 << bit_idx);
    
    // Agregar a free list
    if (pool->free_count < pool->total_items) {
        pool->free_list[pool->free_count] = item_idx;
        pool->free_count++;
        if (pool->used_items > 0) pool->used_items--;
    }
}

int pool_get_usage(MemoryPool *pool) {
    if (!pool) return 0;
    return pool->used_items;
}

int pool_get_available(MemoryPool *pool) {
    if (!pool) return 0;
    return pool->free_count;
}

void pool_reset(MemoryPool *pool) {
    if (!pool) return;
    
    // Resetear bitmap
    memset(pool->allocation_map, 0, (pool->total_items + 7) / 8);
    
    // Resetear free list
    for(int i = 0; i < pool->total_items; i++) {
        pool->free_list[i] = i;
    }
    
    pool->free_count = pool->total_items;
    pool->used_items = 0;
}

void pool_cleanup(MemoryPool *pool) {
    if (!pool) return;
    
    if (pool->pool) free(pool->pool);
    if (pool->free_list) free(pool->free_list);
    if (pool->allocation_map) free(pool->allocation_map);
    free(pool);
}
