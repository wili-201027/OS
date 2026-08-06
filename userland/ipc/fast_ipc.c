#include "fast_ipc.h"
#include <string.h>

static fast_ipc_port_t s_ports[IPC_MAX_PORTS];
static int s_initialized = 0;

void fast_ipc_init(void)
{
    if (s_initialized) return;
    memset(s_ports, 0, sizeof(s_ports));
    s_initialized = 1;
}

int fast_ipc_open_port(const char *name)
{
    (void)name;
    fast_ipc_init();
    for (int i = 0; i < IPC_MAX_PORTS; ++i) {
        if (s_ports[i].count == 0 && s_ports[i].head == 0 && s_ports[i].tail == 0) {
            return i + 1;
        }
    }
    return -1;
}

int fast_ipc_send(int port, const void *data, uint32_t size)
{
    if (!data || size == 0 || port <= 0 || port > IPC_MAX_PORTS) return -1;
    fast_ipc_port_t *p = &s_ports[port - 1];
    if (size > IPC_MAX_MSG) size = IPC_MAX_MSG;
    if (p->count >= IPC_MAX_PORTS) return -1;
    fast_ipc_message_t *msg = &p->queue[p->tail];
    msg->msg_id = 0x49504300u | (p->count + 1);
    msg->size = size;
    memcpy(msg->payload, data, size);
    p->tail = (p->tail + 1) % IPC_MAX_PORTS;
    p->count++;
    return 0;
}

int fast_ipc_recv(int port, void *data, uint32_t size)
{
    if (!data || size == 0 || port <= 0 || port > IPC_MAX_PORTS) return -1;
    fast_ipc_port_t *p = &s_ports[port - 1];
    if (p->count == 0) return -1;
    fast_ipc_message_t *msg = &p->queue[p->head];
    uint32_t copy = size < msg->size ? size : msg->size;
    memcpy(data, msg->payload, copy);
    p->head = (p->head + 1) % IPC_MAX_PORTS;
    p->count--;
    return (int)copy;
}

int fast_ipc_close(int port)
{
    if (port <= 0 || port > IPC_MAX_PORTS) return -1;
    memset(&s_ports[port - 1], 0, sizeof(s_ports[port - 1]));
    return 0;
}
