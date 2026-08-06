// kernel/drivers/acpi.c
#include <stdint.h>

typedef struct {
    char sig[8];
    uint8_t csum;
    char oem[6];
    uint8_t rev;
    uint32_t rsdt;
    uint64_t xsdt;
} __attribute__((packed)) rsdp_t;

static rsdp_t *rsdp;

void acpi_init(void *bios_area)
{
    for (uint8_t *p=(uint8_t*)bios_area; p<(uint8_t*)bios_area+0x20000; p+=16) {
        if (!__builtin_memcmp(p,"RSD PTR ",8)) {
            rsdp = (rsdp_t*)p;
            break;
        }
    }
}
