// kernel/drivers/net/nic.c
#include <stdint.h>

typedef struct {
    uint8_t mac[6];
    void (*send)(const void*,uint32_t);
} nic_t;

static nic_t active_nic;

void nic_register(nic_t *n)
{
    active_nic = *n;
}

void nic_send(const void *buf,uint32_t len)
{
    if (active_nic.send)
        active_nic.send(buf,len);
}
