// kernel/mm/numa.c
#include <stdint.h>
#include <stddef.h>

#define MAX_NUMA_NODES 8

typedef struct numa_node {
    uint32_t id;
    uint64_t mem_start;
    uint64_t mem_end;
    uint64_t free_pages;
} numa_node_t;

static numa_node_t nodes[MAX_NUMA_NODES];
static uint32_t node_count = 1;

void numa_init(void)
{
    /* Early system: single-node fallback.
       Real implementation would parse ACPI SRAT/SLIT. */
    nodes[0].id = 0;
    nodes[0].mem_start = 0;
    nodes[0].mem_end = (1ULL << 36);
    nodes[0].free_pages = (nodes[0].mem_end - nodes[0].mem_start) / 4096;
    node_count = 1;
}

numa_node_t *numa_current_node(void)
{
    /* SMP-aware version would read APIC ID and map to node */
    return &nodes[0];
}

uint64_t numa_alloc_local(int order)
{
    extern uint64_t pmm_alloc_pages(int);
    return pmm_alloc_pages(order);
}
