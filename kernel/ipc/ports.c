// kernel/ipc/ports.c
#include <stdint.h>
#include <stddef.h>

#define PORT_QUEUE_MAX 64

typedef struct message {
    uint64_t src_pid;
    uint64_t value;
} message_t;

typedef struct port {
    message_t queue[PORT_QUEUE_MAX];
    uint32_t head, tail, count;
} port_t;

extern void *slab_alloc(uint32_t);
extern void slab_free(void *);
extern void schedule(void);

port_t *port_create(void)
{
    port_t *p = (port_t*)slab_alloc(sizeof(port_t));
    p->head = p->tail = p->count = 0;
    return p;
}

int port_send(port_t *p, message_t *msg)
{
    if (p->count >= PORT_QUEUE_MAX)
        return -1;

    p->queue[p->tail] = *msg;
    p->tail = (p->tail + 1) % PORT_QUEUE_MAX;
    p->count++;
    return 0;
}

int port_recv(port_t *p, message_t *out)
{
    while (p->count == 0) {
        schedule(); /* block/yield until message arrives */
    }

    *out = p->queue[p->head];
    p->head = (p->head + 1) % PORT_QUEUE_MAX;
    p->count--;
    return 0;
}
