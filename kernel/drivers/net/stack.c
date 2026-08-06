// kernel/drivers/net/stack.c
#include <stdint.h>

typedef struct {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed)) eth_hdr_t;

void net_rx(void *frame)
{
    eth_hdr_t *eth = (eth_hdr_t*)frame;
    if (eth->type == 0x0800) {
        /* IPv4 */
    }
}
