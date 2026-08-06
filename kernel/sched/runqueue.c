// kernel/sched/runqueue.c
#include <stdint.h>
#include <stddef.h>
#include "scheduler.h"

#define MAX_TASKS 1024

static runqueue_t cpu_runqueues[256];

runqueue_t *runqueue_for_cpu(uint32_t cpu)
{
    if (cpu >= 256) cpu = 0;  // Bounds check: fallback to CPU 0
    return &cpu_runqueues[cpu];
}

void runqueue_add(runqueue_t *rq, task_t *task)
{
    if (!rq || !task) return;

    task->state = TASK_STATE_READY;

    if (!rq->head) {
        rq->head = task;
        task->next = task->prev = task;
    } else {
        task_t *h = rq->head;
        task_t *t = h->prev;

        t->next = task;
        task->prev = t;
        task->next = h;
        h->prev = task;

        if (task->vruntime < h->vruntime)
            rq->head = task;
    }
    rq->task_count++;
}

void runqueue_remove(runqueue_t *rq, task_t *task)
{
    if (!rq || !task) return;

    if (task->next == task) {
        rq->head = NULL;
    } else {
        task->prev->next = task->next;
        task->next->prev = task->prev;
        if (rq->head == task)
            rq->head = task->next;
    }
    rq->task_count--;
}

task_t *runqueue_pick_next(runqueue_t *rq)
{
    if (!rq || !rq->head)
        return NULL;

    // CFS: elegir el task con menor vruntime
    task_t *candidate = rq->head;
    task_t *current = rq->head->next;
    while (current != rq->head) {
        if (current->vruntime < candidate->vruntime)
            candidate = current;
        current = current->next;
    }

    // Remover del runqueue
    runqueue_remove(rq, candidate);
    return candidate;
}
