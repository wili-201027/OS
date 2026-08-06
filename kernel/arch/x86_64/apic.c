// kernel/arch/x86_64/apic.c
//
// Driver del Local APIC.
//
// NOTA: apic_init() accede a MMIO en 0xFEE00000.
// Esa dirección debe estar mapeada en las tablas de paginación ANTES de
// llamar a esta función.  boot.S mapea el rango 3GB-4GB (0xC0000000-0xFFFFFFFF)
// como identity-map con huge pages de 2MB, por lo que 0xFEE00000 está
// accesible nada más entrar en long mode.
//
// El puntero 'lapic' se inicializa en apic_init(), no en tiempo de carga,
// para evitar que el compilador genere un acceso en la sección .data antes
// de que el mapa esté listo.

#include <stdint.h>
#include <stddef.h>

#define IA32_APIC_BASE_MSR  0x1B
#define APIC_DEFAULT_BASE   0xFEE00000UL

#define APIC_ID             0x020
#define APIC_EOI            0x0B0
#define APIC_SVR            0x0F0
#define APIC_LVT_TIMER      0x320
#define APIC_ICR_LOW        0x300
#define APIC_ICR_HIGH       0x310

/* Puntero al LAPIC — se fija en apic_init(), nunca en .data estático */
static volatile uint32_t *lapic = (volatile uint32_t*)0;

static inline uint32_t apic_read(uint32_t reg_offset)
{
    return lapic[reg_offset >> 2];
}

static inline void apic_write(uint32_t reg_offset, uint32_t val)
{
    lapic[reg_offset >> 2] = val;
    (void)lapic[APIC_ID >> 2];  /* posting read para flush */
}

void apic_init(void)
{
    /* 1. Leer la dirección base del LAPIC desde el MSR IA32_APIC_BASE */
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(IA32_APIC_BASE_MSR));

    /* Bits [31:12] de lo son los bits [31:12] de la dirección base.
     * En QEMU suele ser siempre 0xFEE00000. */
    uint64_t base = ((uint64_t)(hi & 0xF) << 32) | (lo & 0xFFFFF000UL);
    if (base == 0) base = APIC_DEFAULT_BASE;

    lapic = (volatile uint32_t*)base;

    /* 2. Activar el LAPIC: SVR bit 8 = software enable, vector 0xFF = spurious */
    uint32_t svr = apic_read(APIC_SVR);
    svr |= (1u << 8) | 0xFFu;
    apic_write(APIC_SVR, svr);
}

void apic_eoi(void)
{
    if (lapic) apic_write(APIC_EOI, 0);
}

void apic_send_init_sipi(uint8_t apic_id, uint32_t sipi_vector)
{
    if (!lapic) return;
    apic_write(APIC_ICR_HIGH, (uint32_t)apic_id << 24);
    apic_write(APIC_ICR_LOW, 0x00004500u);  /* INIT assert */
    for (volatile int i = 0; i < 100000; ++i) asm volatile("nop");
    apic_write(APIC_ICR_LOW, 0x00004000u);  /* INIT deassert */
    apic_write(APIC_ICR_HIGH, (uint32_t)apic_id << 24);
    apic_write(APIC_ICR_LOW, 0x00000600u | (sipi_vector & 0xFFu));
}
