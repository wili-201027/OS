#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>

#define TASK_STATE_READY 0
#define TASK_STATE_RUNNING 1
#define TASK_STATE_BLOCKED 2

typedef struct task {
    uint64_t vruntime;
    uint64_t timeslice;
    uint32_t pid;
    int state;
    int priority;
    int nice;
    uint32_t weight;
    void *context;
    void *rsp;
    void *cr3;
    struct task *next;
    struct task *prev;
} task_t;

typedef struct runqueue {
    task_t *head;
    task_t *current;
    uint32_t task_count;
} runqueue_t;

typedef struct cpu_local {
    uint32_t cpu_id;
    uint64_t ticks;
    void *current_task;
    void *idle_task;
} cpu_local_t;

// CPU context layout (match kernel/arch/x86_64/context_switch.S)
typedef struct cpu_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rsp;    /* offset 0x78 */
    uint64_t rip;    /* offset 0x80 */
    uint64_t cs;     /* offset 0x88 */
    uint64_t rflags; /* offset 0x90 */
    uint64_t ss;     /* offset 0x98 */
    uint64_t cr3;    /* offset 0xA0 for user task page table */
} cpu_context_t;

/* Register initial user task with scheduler */
void scheduler_add_initial_task(task_t *t);

task_t *scheduler_spawn(void *entry, void *stack_bottom, uint64_t stack_size, int priority);
void scheduler_yield(void);
uint32_t scheduler_ready_count(void);

void scheduler_init(void);
void scheduler_tick(void);
void schedule(void);
uint64_t scheduler_get_ticks(void);

task_t *task_create(void *entry, void *stack_bottom, uint64_t stack_size, int priority);

#endif // SCHEDULER_H
