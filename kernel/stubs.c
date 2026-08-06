// kernel/stubs.c
// Stubs per a subsistemes no implementats.
// NO redefinir funcions que ja existeixen en fitxers reals.

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Memòria ───────────────────────────────────────────────────────────────────
void paging_enable(void) {}
void heap_init(void)     {}

// ── Xarxa ─────────────────────────────────────────────────────────────────────
void net_init(void) {}

// ── SMP ───────────────────────────────────────────────────────────────────────
uint8_t  ap_trampoline      = 0;
uint64_t ap_trampoline_size = 0;
void     secondary_main(void) {}
uint64_t secondary_stack_top = 0;

// ── Scheduler helpers ─────────────────────────────────────────────────────────
#include "sched/scheduler.h"

typedef struct {
    uint32_t cpu_id; uint64_t ticks; task_t *current_task; task_t *idle_task;
} _cpu_local_t;
static _cpu_local_t _cpu0 __attribute__((unused));

// ── malloc / free ─────────────────────────────────────────────────────────────
// NOTE: compositor needs a large ring-0 heap for back buffers and blur scratch memory.
// The original 2 MB heap was too small for a 1024x768 back buffer + blur buffers.
//
// FIXED: the previous allocator was a bump allocator whose free() was a total
// no-op ("leak allocator, suficient per ara"). Every single malloc() in the
// whole system -- every window, every DOM element, every per-frame blur
// scratch buffer -- permanently consumed heap space and NEVER came back. On a
// 16 MB heap that fills up within seconds of normal desktop use (opening/
// closing windows, resizing, animating). Once it fills, malloc() starts
// silently returning NULL everywhere, which is a major real cause of windows
// failing to render/"breaking" and of the system slowing down the longer it
// runs. This replaces it with a small but real free-list allocator: block
// headers, first-fit search, and coalescing of adjacent free blocks on free().
#define HEAP_SIZE (16u * 1024u * 1024u)
static uint8_t s_heap[HEAP_SIZE] __attribute__((aligned(16)));

typedef struct block_header {
    uint32_t size;              // usable payload size of this block, in bytes
    uint32_t free;               // 1 = free, 0 = in use
    struct block_header *next;   // next block in address order (or NULL)
    struct block_header *prev;   // previous block in address order (or NULL)
} block_header_t;

static block_header_t *s_free_list = (block_header_t*)0;
static int s_heap_ready = 0;

static void heap_lazy_init(void) {
    if (s_heap_ready) return;
    s_heap_ready = 1;
    block_header_t *first = (block_header_t*)s_heap;
    first->size = HEAP_SIZE - (uint32_t)sizeof(block_header_t);
    first->free = 1;
    first->next = (block_header_t*)0;
    first->prev = (block_header_t*)0;
    s_free_list = first;
}

void *malloc(uint32_t size) {
    heap_lazy_init();
    if (size == 0) return (void*)0;
    size = (size + 15u) & ~15u; // 16-byte-align the payload

    block_header_t *b = s_free_list;
    while (b) {
        if (b->free && b->size >= size) {
            // Split the block if there's enough room left over to form
            // another usable block, so we don't waste large chunks on small
            // requests.
            if (b->size >= size + (uint32_t)sizeof(block_header_t) + 16u) {
                uint8_t *split_addr = (uint8_t*)b + sizeof(block_header_t) + size;
                block_header_t *rem = (block_header_t*)split_addr;
                rem->size = b->size - size - (uint32_t)sizeof(block_header_t);
                rem->free = 1;
                rem->next = b->next;
                rem->prev = b;
                if (b->next) b->next->prev = rem;
                b->next = rem;
                b->size = size;
            }
            b->free = 0;
            return (void*)((uint8_t*)b + sizeof(block_header_t));
        }
        b = b->next;
    }
    return (void*)0; // genuinely out of memory
}

static void coalesce_with_neighbours(block_header_t *b) {
    if (b->next && b->next->free) {
        b->size += (uint32_t)sizeof(block_header_t) + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free) {
        b->prev->size += (uint32_t)sizeof(block_header_t) + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void free(void *ptr) {
    if (!ptr) return;
    block_header_t *b = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    // Sanity check: the block header must fall inside our static heap.
    if ((uint8_t*)b < s_heap || (uint8_t*)b >= s_heap + HEAP_SIZE) return;
    if (b->free) return; // double-free guard
    b->free = 1;
    coalesce_with_neighbours(b);
}

// ── Syscalls (stubs per al compositor en ring-0) ──────────────────────────────
int  sys_read_keyboard(uint8_t *b, uint32_t n) { (void)b; (void)n; return 0; }
int  sys_read_mouse(uint8_t *b, uint32_t n)    { (void)b; (void)n; return 0; }

static int s_ipc_next_handle = 1;
int sys_ipc_open(const char *s, const char *r){ 
    (void)s; (void)r;
    if (s_ipc_next_handle < 0x7fffffff) return s_ipc_next_handle++;
    return 1;
}

int fast_ipc_init(void) { return 0; }
int fast_ipc_open_port(const char *name) { return sys_ipc_open(name, ""); }
int fast_ipc_send(int port, const void *data, uint32_t size) { (void)port; (void)data; (void)size; return 0; }
int fast_ipc_recv(int port, void *data, uint32_t size) { (void)port; (void)data; (void)size; return -1; }
int fast_ipc_close(int port) { (void)port; return 0; }

void sys_yield(void) { asm volatile("hlt"); }

// sleep_ms: busy wait amb HLT (en ring-0 funciona, no és precís però suficient)
void sys_sleep_ms(uint32_t ms) {
    // Aproximació: ~100k cicles per ms a 100MHz virtual de QEMU
    for(uint32_t i=0; i<ms*1000; ++i) asm volatile("pause");
}

#ifdef __cplusplus
}
#endif
