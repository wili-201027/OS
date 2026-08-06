// kernel/sched/scheduler.c
// Añadimos contexto CPU para arrancar el task 'init' y una forma simple
// de arrancar el proceso de usuario. Este scheduler es mínimo: sólo
// inicia la tarea inicial; planificación completa vendrá después.

#include <stdint.h>
#include <stddef.h>
#include "scheduler.h"

#define PTE_P  (1ULL << 0)
#define PTE_RW (1ULL << 1)
#define PTE_US (1ULL << 2)

extern void   *slab_alloc(uint32_t size);
extern void    slab_free(void *ptr);

extern cpu_local_t  *cpu_local(void);
extern runqueue_t   *runqueue_for_cpu(uint32_t cpu);
extern void runqueue_add(runqueue_t *rq, task_t *task);
extern task_t *runqueue_pick_next(runqueue_t *rq);
extern void switch_to(void *old_ctx, void *new_ctx);
extern void vmm_switch(void *);
extern void vmm_map(void *address_space, uint64_t vaddr, uint64_t paddr, uint64_t flags);
extern uint64_t pmm_alloc_pages(int order);
extern void pmm_free_pages(uint64_t paddr, int order);
extern void thread_start(void (*entry)(void*), void *arg);

static uint64_t g_next_user_stack = 0x00007FFF00000000ULL;

// --- Minimal stubs for scheduler diagnostics ---
static void serial_putc(char c) { (void)c; }
static void serial_puts(const char *s) { (void)s; }
static void serial_hex64(uint64_t v) { (void)v; }

static uint64_t  g_total_ticks = 0;
static uint32_t  next_pid      = 1;

/* Kernel-side scratch context usado al llamar switch_to() desde kernel */
static cpu_context_t g_kernel_ctx;
static task_t *g_init_task = NULL;
static uint32_t g_ready_count = 0;

// ── task_create ───────────────────────────────────────────────────────────────
task_t *task_create(void *entry, void *stack_bottom, uint64_t stack_size, int priority)
{
    task_t *t = (task_t *)slab_alloc(sizeof(task_t));
    if (!t) return (task_t*)0;

    // Limpiar struct
    uint8_t *p = (uint8_t *)t;
    for (uint64_t i = 0; i < sizeof(task_t); i++) p[i] = 0;

    t->pid         = next_pid++;
    t->priority    = priority;
    t->state       = TASK_STATE_READY;
    t->timeslice   = 10;
    t->stack_bottom = stack_bottom;
    t->stack_size  = stack_size;
    t->stack_order = UINT32_MAX;

    // Crear un cpu_context en el heap del kernel y rellenarlo.
    cpu_context_t *ctx = (cpu_context_t *)slab_alloc(sizeof(cpu_context_t));
    if (!ctx) return (task_t*)0;
    for (uint64_t i = 0; i < sizeof(cpu_context_t); ++i)
        ((uint8_t*)ctx)[i] = 0;

    // RSP del usuario = stack_bottom + stack_size (top)
    ctx->rsp = (uint64_t)stack_bottom + stack_size;
    ctx->rip = (uint64_t)entry;
    // Segment selectors para usuario (flat user code/data)
    ctx->cs = 0x1B;   // user code segment
    ctx->ss = 0x23;   // user stack segment
    ctx->rflags = 0x202; // IF = 1
    ctx->cr3 = 0;     // Will be set by loader after create

    t->rsp = (void *)ctx->rsp;
    t->context = (void *)ctx;

    return t;
}

static void scheduler_enqueue(task_t *task)
{
    if (!task) return;

    cpu_local_t *cpu = cpu_local();
    if (!cpu) return;

    runqueue_t *rq = runqueue_for_cpu(cpu->cpu_id);
    if (!rq) return;

    task->state = TASK_STATE_READY;
    runqueue_add(rq, task);
    ++g_ready_count;
}

static task_t *scheduler_pick_next(void)
{
    cpu_local_t *cpu = cpu_local();
    if (!cpu) return (task_t*)0;

    runqueue_t *rq = runqueue_for_cpu(cpu->cpu_id);
    if (!rq) return (task_t*)0;

    task_t *next = runqueue_pick_next(rq);
    if (next) --g_ready_count;
    return next;
}

task_t *scheduler_spawn(void *entry, void *stack_bottom, uint64_t stack_size, int priority)
{
    task_t *t = task_create(entry, stack_bottom, stack_size, priority);
    if (!t) return (task_t*)0;

    scheduler_enqueue(t);
    return t;
}

void scheduler_yield(void)
{
    cpu_local_t *cpu = cpu_local();
    if (!cpu || !cpu->current_task) return;

    task_t *current = (task_t*)cpu->current_task;
    current->state = TASK_STATE_READY;
    current->vruntime += current->timeslice;
    scheduler_enqueue(current);
    schedule();
}

// ── scheduler_tick (llamado desde el timer handler) ───────────────────────────
void scheduler_tick(void)
{
    cpu_local_t *cpu = cpu_local();
    cpu->ticks = ++g_total_ticks;
    // Preempción desactivada en esta fase.
}

// ── schedule: arrancar la tarea inicial si existe ─────────────────────────────
void schedule(void)
{
    cpu_local_t *cpu = cpu_local();
    if (!cpu) return;

    if (cpu->current_task == NULL) {
        if (!g_init_task) {
            serial_puts("[schedule] ERROR: g_init_task is NULL\n");
            return;
        }

        serial_puts("[schedule] setting current_task\n");
        cpu->current_task = g_init_task;
        g_init_task->state = TASK_STATE_RUNNING;

        serial_puts("[schedule] task->cr3=");
        serial_hex64((uint64_t)g_init_task->cr3);
        serial_puts(" task->context=");
        serial_hex64((uint64_t)g_init_task->context);
        serial_puts("\n");

        serial_puts("[schedule] calling switch_to\n");
        switch_to(&g_kernel_ctx, (void *)g_init_task->context);
        serial_puts("[schedule] switch_to returned\n");
        return;
    }

    task_t *next = scheduler_pick_next();
    if (!next) {
        return; // no other task ready
    }

    task_t *current = (task_t*)cpu->current_task;
    if (current == next) {
        next->state = TASK_STATE_RUNNING;
        cpu->current_task = next;
        return;
    }

    next->state = TASK_STATE_RUNNING;
    cpu->current_task = next;
    switch_to(current->context, next->context);
}

void scheduler_add_initial_task(task_t *t)
{
    g_init_task = t;
}

static void *allocate_user_stack(uint64_t requested_size,
                                  uint32_t *out_order,
                                  uint64_t *out_alloc_size,
                                  uint64_t *out_phys_base,
                                  uint64_t address_space)
{
    if (!requested_size) requested_size = 4096;
    if (requested_size < 4096) requested_size = 4096;

    uint32_t order = 0;
    while (((uint64_t)1 << order) * 4096 < requested_size) order++;
    uint64_t pages = (uint64_t)1 << order;
    uint64_t alloc_size = pages * 4096;

    uint64_t phys = pmm_alloc_pages(order);
    if (!phys) return (void *)0;

    g_next_user_stack &= ~(uint64_t)0xFFFULL;
    uint64_t stack_top = g_next_user_stack;
    uint64_t stack_bottom_alloc = stack_top - alloc_size;
    g_next_user_stack = stack_bottom_alloc;

    for (uint64_t i = 0; i < pages; ++i) {
        vmm_map((void *)address_space,
                stack_bottom_alloc + i * 4096,
                phys + i * 4096,
                PTE_P | PTE_RW | PTE_US);
    }

    *out_order = order;
    *out_alloc_size = alloc_size;
    *out_phys_base = phys;
    return (void *)stack_bottom_alloc;
}

int thread_create(void *entry, void *arg, void *stack_bottom, uint64_t stack_size)
{
    if (!entry || stack_size < 1024) return -1;

    cpu_local_t *cpu = cpu_local();
    if (!cpu || !cpu->current_task) return -1;

    task_t *current = (task_t *)cpu->current_task;
    if (!current || !current->context) return -1;

    cpu_context_t *current_ctx = (cpu_context_t *)current->context;
    uint64_t user_cr3 = current_ctx->cr3;

    uint64_t effective_size = stack_size;
    uint32_t stack_order = UINT32_MAX;
    uint64_t alloc_size = 0;
    uint64_t phys_base = 0;

    if (!stack_bottom) {
        stack_bottom = allocate_user_stack(effective_size, &stack_order, &alloc_size,
                                           &phys_base, user_cr3);
        if (!stack_bottom) return -1;
        effective_size = alloc_size;
    }

    uint64_t aligned_top = ((uint64_t)stack_bottom + effective_size) & ~0xFULL;
    uint64_t actual_size = aligned_top - (uint64_t)stack_bottom;
    task_t *t = task_create((void *)entry, stack_bottom, actual_size, 0);
    if (!t) {
        if (stack_order != UINT32_MAX) {
            pmm_free_pages(phys_base, stack_order);
        }
        return -1;
    }

    if (stack_order != UINT32_MAX) {
        t->stack_order = stack_order;
        t->stack_size = alloc_size;
        t->stack_phys = (void *)(uintptr_t)phys_base;
    }

    cpu_context_t *ctx = (cpu_context_t *)t->context;
    ctx->rdi = (uint64_t)arg;
    ctx->rsi = 0;
    ctx->rsp = aligned_top;
    ctx->cr3 = user_cr3;
    t->cr3 = (void *)(uintptr_t)ctx->cr3;
    t->vruntime = current->vruntime;

    scheduler_enqueue(t);
    return (int)t->pid;
}

void task_exit(int exit_code)
{
    (void)exit_code;
    cpu_local_t *cpu = cpu_local();
    if (!cpu || !cpu->current_task) {
        for (;;) { }
    }

    task_t *current = (task_t*)cpu->current_task;
    current->state = TASK_STATE_BLOCKED;

    task_t *next = scheduler_pick_next();
    if (!next) {
        for (;;) { }
    }

    cpu->current_task = next;
    next->state = TASK_STATE_RUNNING;

    cpu_context_t *old_ctx = (cpu_context_t *)current->context;
    if (current->stack_order != UINT32_MAX && current->stack_phys) {
        pmm_free_pages((uint64_t)current->stack_phys, current->stack_order);
        current->stack_phys = (void*)0;
    }
    current->context = (void*)0;
    current->rsp = (void*)0;
    current->cr3 = (void*)0;

    switch_to(old_ctx, next->context);
    __builtin_unreachable();
}

// ── scheduler_init ────────────────────────────────────────────────────────────
void scheduler_init(void)
{
    cpu_local_t *cpu = cpu_local();
    cpu->ticks        = 0;
    cpu->current_task = (task_t*)0;
    cpu->idle_task    = (task_t*)0;
    g_ready_count = 0;

    runqueue_t *rq = runqueue_for_cpu(cpu->cpu_id);
    rq->head       = (task_t*)0;
    rq->task_count = 0;
}

uint64_t scheduler_get_ticks(void) { return g_total_ticks; }

uint32_t scheduler_ready_count(void) { return g_ready_count; }
