// kernel/sched/cpu_local.c
#include <stdint.h>
#include <stddef.h>
#include "scheduler.h"

#define MAX_CPUS 256

static cpu_local_t cpu_locals[MAX_CPUS];
static uint32_t g_current_cpu = 0;  // Cache the current CPU ID to avoid repeated APIC reads

static inline uint32_t read_apic_id(void)
{
    uint32_t id;
    asm volatile("movl $0xfee00020, %%eax; movl (%%rax), %%eax; shr $24, %%eax"
                 : "=a"(id) :: "memory");
    return id;
}

cpu_local_t *cpu_local(void)
{
    uint32_t id = g_current_cpu;  // Use cached CPU ID
    if (id >= MAX_CPUS) id = 0;
    return &cpu_locals[id];
}

void cpu_local_init(uint32_t cpu_id)
{
    // Cache the CPU ID at initialization time
    if (cpu_id < MAX_CPUS) {
        g_current_cpu = cpu_id;
    } else {
        g_current_cpu = 0;
    }
    
    cpu_locals[cpu_id].cpu_id = cpu_id;
    cpu_locals[cpu_id].ticks = 0;
    cpu_locals[cpu_id].current_task = NULL;
    cpu_locals[cpu_id].idle_task = NULL;
}
