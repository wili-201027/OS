// PHASE 1.1: DIRECTORY CACHE - Implementación
// Cache de directorios con LRU eviction y TTL

#include "directory_cache.h"
#include "../libc/stdlib.h"
#include "../libc/string.h"

static int dcache_find_index(const DirectoryFileSystem *dfs, const char *path) {
    if (!dfs || !path) return -1;

    for (int i = 0; i < dfs->active_count; i++) {
        if (dfs->caches[i].is_valid && strcmp(dfs->caches[i].path, path) == 0) {
            return i;
        }
    }

    return -1;
}

static void dcache_reset_slot(DirectoryCache *cache) {
    if (!cache) return;

    cache->path[0] = '\0';
    cache->entries = NULL;
    cache->count = 0;
    cache->created_at_ms = 0;
    cache->last_access_ms = 0;
    cache->ttl_ms = 0;
    cache->is_valid = 0;
}

static void dcache_clear_entry(DirectoryCache *cache) {
    if (!cache) return;

    if (cache->entries) {
        free(cache->entries);
        cache->entries = NULL;
    }

    dcache_reset_slot(cache);
}

static void dcache_evict_least_recent(DirectoryFileSystem *dfs) {
    if (!dfs || dfs->active_count == 0) return;

    int oldest_index = -1;
    uint64_t oldest_time = UINT64_MAX;

    for (int i = 0; i < dfs->active_count; i++) {
        if (dfs->caches[i].is_valid && dfs->caches[i].last_access_ms < oldest_time) {
            oldest_index = i;
            oldest_time = dfs->caches[i].last_access_ms;
        }
    }

    if (oldest_index < 0) return;

    dcache_clear_entry(&dfs->caches[oldest_index]);

    if (oldest_index != dfs->active_count - 1) {
        dfs->caches[oldest_index] = dfs->caches[dfs->active_count - 1];
        dcache_reset_slot(&dfs->caches[dfs->active_count - 1]);
    }

    dfs->active_count--;
}

DirectoryFileSystem* dcache_create(void) {
    DirectoryFileSystem *dfs = malloc(sizeof(DirectoryFileSystem));
    if (!dfs) return NULL;

    dfs->active_count = 0;
    dfs->default_ttl_ms = 5000;

    for (int i = 0; i < DIRECTORY_CACHE_SIZE; i++) {
        dcache_reset_slot(&dfs->caches[i]);
        dfs->caches[i].ttl_ms = dfs->default_ttl_ms;
    }

    return dfs;
}

void* dcache_get(DirectoryFileSystem *dfs, const char *path, uint32_t *count) {
    if (!dfs || !path || !count) return NULL;

    int index = dcache_find_index(dfs, path);
    if (index < 0) return NULL;

    DirectoryCache *cache = &dfs->caches[index];
    uint64_t now = get_time_ms();
    uint64_t age = now - cache->created_at_ms;

    if (age >= cache->ttl_ms) {
        dcache_invalidate(dfs, path);
        return NULL;
    }

    // Actualizar último acceso para LRU.
    cache->last_access_ms = now;
    *count = cache->count;
    return cache->entries;
}

void dcache_add(DirectoryFileSystem *dfs, const char *path, void *entries, uint32_t count) {
    if (!dfs || !path || !entries || count == 0) return;

    int existing = dcache_find_index(dfs, path);
    if (existing >= 0) {
        DirectoryCache *cache = &dfs->caches[existing];
        if (cache->entries != entries) {
            free(cache->entries);
        }

        strncpy(cache->path, path, DIRECTORY_CACHE_PATH_MAX - 1);
        cache->path[DIRECTORY_CACHE_PATH_MAX - 1] = '\0';
        cache->entries = entries;
        cache->count = count;
        uint64_t now = get_time_ms();
        cache->created_at_ms = now;
        cache->last_access_ms = now;
        cache->ttl_ms = dfs->default_ttl_ms;
        cache->is_valid = 1;
        return;
    }

    if (dfs->active_count >= DIRECTORY_CACHE_SIZE) {
        dcache_evict_least_recent(dfs);
    }

    DirectoryCache *cache = &dfs->caches[dfs->active_count];
    strncpy(cache->path, path, DIRECTORY_CACHE_PATH_MAX - 1);
    cache->path[DIRECTORY_CACHE_PATH_MAX - 1] = '\0';
    cache->entries = entries;
    cache->count = count;
    uint64_t now = get_time_ms();
    cache->created_at_ms = now;
    cache->last_access_ms = now;
    cache->ttl_ms = dfs->default_ttl_ms;
    cache->is_valid = 1;

    dfs->active_count++;
}

void dcache_invalidate(DirectoryFileSystem *dfs, const char *path) {
    if (!dfs || !path) return;

    int index = dcache_find_index(dfs, path);
    if (index < 0) return;

    int last_index = dfs->active_count - 1;
    if (index != last_index) {
        dcache_clear_entry(&dfs->caches[index]);
        dfs->caches[index] = dfs->caches[last_index];
        dcache_reset_slot(&dfs->caches[last_index]);
    } else {
        dcache_clear_entry(&dfs->caches[index]);
    }

    dfs->active_count--;
}

void dcache_invalidate_all(DirectoryFileSystem *dfs) {
    if (!dfs) return;

    for (int i = 0; i < dfs->active_count; i++) {
        dcache_clear_entry(&dfs->caches[i]);
    }

    dfs->active_count = 0;
}

int dcache_is_cached(DirectoryFileSystem *dfs, const char *path) {
    if (!dfs || !path) return 0;

    int index = dcache_find_index(dfs, path);
    if (index < 0) return 0;

    DirectoryCache *cache = &dfs->caches[index];
    uint64_t now = get_time_ms();
    uint64_t age = now - cache->created_at_ms;

    if (age >= cache->ttl_ms) {
        dcache_invalidate(dfs, path);
        return 0;
    }

    return 1;
}

void dcache_cleanup(DirectoryFileSystem *dfs) {
    if (!dfs) return;

    dcache_invalidate_all(dfs);
    free(dfs);
}

void dcache_set_ttl(DirectoryFileSystem *dfs, uint32_t ttl_ms) {
    if (!dfs) return;
    dfs->default_ttl_ms = ttl_ms;
}
