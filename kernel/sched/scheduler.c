// kernel/sched/scheduler.c
// Añadimos contexto CPU para arrancar el task 'init' y una forma simple
// de arrancar el proceso de usuario. Este scheduler es mínimo: sólo
// inicia la tarea inicial; planificación completa vendrá después.

#include <stdint.h>
#include <stddef.h>
#include "scheduler.h"

extern void   *slab_alloc(uint32_t size);

extern cpu_local_t  *cpu_local(void);
extern runqueue_t   *runqueue_for_cpu(uint32_t cpu);
extern void switch_to(void *old_ctx, void *new_ctx);
extern void vmm_switch(void *);

// --- Serial helpers for diagnostics ---
static inline void outb_dx(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb_dx(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static void serial_putc(char c) {
    while (!(inb_dx(0x3FD) & 0x20)) { /* busy-wait */ }
    outb_dx(0x3F8, (uint8_t)c);
}
static void serial_puts(const char *s) {
    while (s && *s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}
static void serial_hex64(uint64_t v) {
    static const char h[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 15; i >= 0; --i)
        serial_putc(h[(v >> (i * 4)) & 0xF]);
}

static uint64_t  g_total_ticks = 0;
static uint32_t  next_pid      = 1;

/* Kernel-side scratch context usado al llamar switch_to() desde kernel */
static cpu_context_t g_kernel_ctx;
static task_t *g_init_task = NULL;
static task_t *g_current_task = NULL;
static uint32_t g_ready_count = 0;

// ── task_create ───────────────────────────────────────────────────────────────
task_t *task_create(void *entry, void *stack_bottom, uint64_t stack_size, int priority)
{
    task_t *t = (task_t *)slab_alloc(sizeof(task_t));
    if (!t) return (task_t*)0;

    // Limpiar struct
    uint8_t *p = (uint8_t *)t;
    for (uint64_t i = 0; i < sizeof(task_t); i++) p[i] = 0;

    t->pid      = next_pid++;
    t->priority = priority;
    t->state    = TASK_STATE_READY;
    t->timeslice = 10;

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

task_t *scheduler_spawn(void *entry, void *stack_bottom, uint64_t stack_size, int priority)
{
    task_t *t = task_create(entry, stack_bottom, stack_size, priority);
    if (!t) return (task_t*)0;

    cpu_local_t *cpu = cpu_local();
    if (cpu) {
        runqueue_t *rq = runqueue_for_cpu(cpu->cpu_id);
        if (rq) {
            t->state = TASK_STATE_READY;
            rq->task_count++;
            if (!rq->head) {
                rq->head = t;
                t->next = t->prev = t;
            } else {
                task_t *head = rq->head;
                task_t *tail = head->prev;
                tail->next = t;
                t->prev = tail;
                t->next = head;
                head->prev = t;
            }
            ++g_ready_count;
        }
    }

    return t;
}

void scheduler_yield(void)
{
    if (g_current_task) {
        g_current_task->state = TASK_STATE_READY;
        ++g_ready_count;
    }
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
    if (!g_init_task) {
        serial_puts("[schedule] ERROR: g_init_task is NULL\n");
        return;
    }
    if (cpu->current_task != NULL) {
        serial_puts("[schedule] ERROR: current_task already set\n");
        return; // ya arrancada
    }

    serial_puts("[schedule] setting current_task\n");
    cpu->current_task = g_init_task;
    
    serial_puts("[schedule] task->cr3=");
    serial_hex64((uint64_t)g_init_task->cr3);
    serial_puts(" task->context=");
    serial_hex64((uint64_t)g_init_task->context);
    serial_puts("\n");
    
    // NOTE: CR3 switch happens inside switch_to before iretq, not here.
    // This avoids kernel code crash after CR3 reload.
    serial_puts("[schedule] calling switch_to\n");
    switch_to(&g_kernel_ctx, (void *)g_init_task->context);
    serial_puts("[schedule] switch_to returned\n");
}

void scheduler_add_initial_task(task_t *t)
{
    g_init_task = t;
}

// ── scheduler_init ────────────────────────────────────────────────────────────
void scheduler_init(void)
{
    cpu_local_t *cpu = cpu_local();
    cpu->ticks        = 0;
    cpu->current_task = (task_t*)0;
    cpu->idle_task    = (task_t*)0;
    g_current_task = (task_t*)0;
    g_ready_count = 0;

    runqueue_t *rq = runqueue_for_cpu(cpu->cpu_id);
    rq->head       = (task_t*)0;
    rq->task_count = 0;
}

uint64_t scheduler_get_ticks(void) { return g_total_ticks; }

uint32_t scheduler_ready_count(void) { return g_ready_count; }
