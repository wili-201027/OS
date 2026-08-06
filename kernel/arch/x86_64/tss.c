// kernel/arch/x86_64/tss.c

#include <stdint.h>
#include <stddef.h>

struct __attribute__((packed)) tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;   /* IST 1 → #DF */
    uint64_t ist2;   /* IST 2 → NMI */
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};

static struct tss64 kernel_tss __attribute__((aligned(16)));

#define IST_STACK_SIZE 4096
static uint8_t ist1_stack[IST_STACK_SIZE] __attribute__((aligned(16)));  /* #DF  */
static uint8_t ist2_stack[IST_STACK_SIZE] __attribute__((aligned(16)));  /* NMI  */

extern void gdt_set_tss(void *addr, uint32_t size);
extern void load_tss(uint16_t sel);
extern uint64_t kernel_stack_top;

void tss_init(void)
{
    kernel_tss.rsp0      = kernel_stack_top;
    kernel_tss.ist1      = (uint64_t)(ist1_stack + IST_STACK_SIZE);
    kernel_tss.ist2      = (uint64_t)(ist2_stack + IST_STACK_SIZE);
    kernel_tss.iomap_base = (uint16_t)sizeof(struct tss64);

    gdt_set_tss(&kernel_tss, (uint32_t)sizeof(struct tss64));
    load_tss(0x28);
}
