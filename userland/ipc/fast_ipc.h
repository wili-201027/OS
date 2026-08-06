#ifndef GPTOS_FAST_IPC_H
#define GPTOS_FAST_IPC_H

#include <stdint.h>

#define IPC_MAX_MSG 128
#define IPC_MAX_PORTS 16

typedef struct {
    uint32_t msg_id;
    uint32_t size;
    uint8_t payload[IPC_MAX_MSG];
} fast_ipc_message_t;

typedef struct {
    fast_ipc_message_t queue[IPC_MAX_PORTS];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} fast_ipc_port_t;

void fast_ipc_init(void);
int fast_ipc_open_port(const char *name);
int fast_ipc_send(int port, const void *data, uint32_t size);
int fast_ipc_recv(int port, void *data, uint32_t size);
int fast_ipc_close(int port);

#endif
