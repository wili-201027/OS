// kernel/drivers/ahci.c
#include <stdint.h>

typedef struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
} hba_mem_t;

static hba_mem_t *abar;

void ahci_init(uint64_t abar_phys)
{
    abar = (hba_mem_t*)abar_phys;
    abar->ghc |= (1<<31); /* AHCI enable */
}
