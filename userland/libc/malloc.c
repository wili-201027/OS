// userland/libc/malloc.c
// Simple memory allocator for userland (freestanding environment)

#include <stdint.h>
#include <stddef.h>

#define HEAP_SIZE (1024 * 1024)  // 1 MB heap

static uint8_t heap[HEAP_SIZE];
static uint32_t top = 0;

void *malloc(size_t size)
{
    if (size == 0 || top + size > HEAP_SIZE) return NULL;
    void *ptr = &heap[top];
    top += size;
    return ptr;
}

void free(void *ptr) 
{ 
    // Simple allocator: no-op
    // In production, implement a proper free list or garbage collector
    (void)ptr;
}

void *realloc(void *ptr, size_t size)
{
    // Simplified: allocate new block and copy (no actual rearrangement)
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    void *new_ptr = malloc(size);
    if (new_ptr && ptr) {
        // Copy old data (assuming original block size unknown)
        // This is a limitation of simple allocators
        for (size_t i = 0; i < size; i++) {
            ((uint8_t *)new_ptr)[i] = ((uint8_t *)ptr)[i];
        }
    }
    return new_ptr;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    
    if (ptr) {
        // Zero-initialize
        for (size_t i = 0; i < total; i++) {
            ((uint8_t *)ptr)[i] = 0;
        }
    }
    
    return ptr;
}

void exit(int status)
{
    // Halt the system
    for(;;) asm volatile("hlt");
}

void abort(void)
{
    exit(1);
}
