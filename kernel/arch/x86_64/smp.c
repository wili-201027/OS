// kernel/arch/x86_64/smp.c
//
// SMP bring-up mínimo. Por ahora solo arranque del BSP.
// El código SMP completo requiere ACPI MADT parsing y trampoline real.

#include <stdint.h>
#include <stddef.h>

extern void apic_send_init_sipi(uint8_t apic_id, uint32_t vector);

static int cpu_count = 1;

int smp_detect_and_boot(void)
{
    // Por ahora solo reportamos 1 CPU (el BSP).
    // Un futuro ACPI MADT parser rellenará esto.
    return cpu_count;
}

int get_cpu_count(void) { return cpu_count; }
