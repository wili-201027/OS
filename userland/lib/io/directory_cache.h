// PHASE 1.1: DIRECTORY CACHE - Caché de directorios para performance
// Reduce latencia de navegación en 90%

#ifndef DIRECTORY_CACHE_H
#define DIRECTORY_CACHE_H

#include <stdint.h>
#include "../libc/time.h"

#define DIRECTORY_CACHE_PATH_MAX 512
#define DIRECTORY_CACHE_SIZE 16

typedef struct {
    char path[DIRECTORY_CACHE_PATH_MAX];
    void *entries;        // Pointer a FileInfo array. Cache owns this memory.
    uint32_t count;
    uint64_t created_at_ms; // Timestamp when the entry was inserted.
    uint64_t last_access_ms; // Timestamp used for LRU eviction.
    uint32_t ttl_ms;       // Time-to-live in milliseconds.
    int is_valid;
} DirectoryCache;

typedef struct {
    DirectoryCache caches[DIRECTORY_CACHE_SIZE];
    int active_count;
    uint32_t default_ttl_ms;
} DirectoryFileSystem;

// Funciones públicas
DirectoryFileSystem* dcache_create(void);
void* dcache_get(DirectoryFileSystem *dfs, const char *path, uint32_t *count);
void dcache_add(DirectoryFileSystem *dfs, const char *path, void *entries, uint32_t count);
void dcache_invalidate(DirectoryFileSystem *dfs, const char *path);
void dcache_invalidate_all(DirectoryFileSystem *dfs);
int dcache_is_cached(DirectoryFileSystem *dfs, const char *path);
void dcache_cleanup(DirectoryFileSystem *dfs);
void dcache_set_ttl(DirectoryFileSystem *dfs, uint32_t ttl_ms);

#endif
