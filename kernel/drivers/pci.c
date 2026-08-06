// kernel/drivers/pci.c
#include <stdint.h>

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static inline void outl(uint16_t p, uint32_t v){ asm volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline uint32_t inl(uint16_t p){ uint32_t r; asm volatile("inl %1,%0":"=a"(r):"Nd"(p)); return r; }

uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    uint32_t addr = (1<<31) | (bus<<16) | (dev<<11) | (fn<<8) | (off & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

void pci_scan(void)
{
    for (uint16_t b=0;b<256;b++)
        for (uint8_t d=0;d<32;d++) {
            uint32_t id = pci_read(b,d,0,0);
            if ((id & 0xFFFF) != 0xFFFF) {
                /* device discovered */
            }
        }
}
