// kernel/arch/x86_64/traps.c
//
// Handlers de excepciones e IRQs del hardware.
// Registra callbacks en la IDT mediante traps_init().
// Los mensajes de diagnóstico van por COM1 (serial) porque el framebuffer
// puede no estar disponible cuando ocurre una excepción.

#include <stdint.h>
#include <stddef.h>

/* ── Frame de interrupciones construido por isr_common_stub ─────────────── */
struct __attribute__((packed)) trap_frame {
    /* GPRs empujados por isr_common_stub (orden de pop inverso al push) */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    /* Empujados por el stub individual */
    uint64_t vector;
    uint64_t error_code;
    /* Empujados por la CPU al vectorizar */
    uint64_t rip, cs, rflags;
    /* Solo presentes si hubo cambio de privilegio (CPL cambia) */
    uint64_t rsp, ss;
};

/* ── Prototipos externos ────────────────────────────────────────────────── */
extern void idt_register_handler(int, void (*)(void *));
extern void pic_eoi(uint8_t vec);
extern void scheduler_tick(void);

/* ── Salida serie COM1 (diagnóstico pre-FB) ─────────────────────────────── */
static inline void s_outb(uint16_t p, uint8_t v) {
    asm volatile("outb %0,%1"::"a"(v),"Nd"(p));
}
static inline uint8_t s_inb(uint16_t p) {
    uint8_t v; asm volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}
static void serial_c(char c) {
    while (!(s_inb(0x3FD) & 0x20));
    s_outb(0x3F8, (uint8_t)c);
}
static void serial_s(const char *s) {
    for (; *s; ++s) { if (*s == '\n') serial_c('\r'); serial_c(*s); }
}
static void serial_x(uint64_t v) {
    static const char h[] = "0123456789ABCDEF";
    serial_s("0x");
    for (int i = 15; i >= 0; --i) serial_c(h[(v >> (i*4)) & 0xF]);
}
static void serial_u(uint64_t v) {
    char buf[21]; int n = 0;
    if (!v) { serial_c('0'); return; }
    while (v) { buf[n++] = '0' + (int)(v % 10); v /= 10; }
    for (int i = n-1; i >= 0; --i) serial_c(buf[i]);
}

/* ── Macro: dump de registro con nombre ─────────────────────────────────── */
#define DUMP_REG(name, val) do { serial_s("  " name "="); serial_x(val); serial_c('\n'); } while(0)

static void dump_frame(const char *exc, struct trap_frame *f)
{
    serial_s("\n\033[31m[KERNEL PANIC] ");
    serial_s(exc);
    serial_s("\033[0m\n");
    DUMP_REG("RIP    ", f->rip);
    DUMP_REG("RSP    ", f->rsp);
    DUMP_REG("RFLAGS ", f->rflags);
    DUMP_REG("CS     ", f->cs);
    DUMP_REG("ERR    ", f->error_code);
    DUMP_REG("VEC    ", f->vector);
    DUMP_REG("RAX    ", f->rax);
    DUMP_REG("RBX    ", f->rbx);
    DUMP_REG("RCX    ", f->rcx);
    DUMP_REG("RDX    ", f->rdx);
    DUMP_REG("RSI    ", f->rsi);
    DUMP_REG("RDI    ", f->rdi);
    DUMP_REG("RBP    ", f->rbp);
    DUMP_REG("R8     ", f->r8);
    DUMP_REG("R9     ", f->r9);
    DUMP_REG("R10    ", f->r10);
    DUMP_REG("R11    ", f->r11);
    DUMP_REG("R12    ", f->r12);
    DUMP_REG("R13    ", f->r13);
    DUMP_REG("R14    ", f->r14);
    DUMP_REG("R15    ", f->r15);
}

/* ── Handlers de excepciones ────────────────────────────────────────────── */

static void handler_de(void *fp) {
    struct trap_frame *f = fp;
    dump_frame("#DE Divide Error", f);
    for (;;) asm volatile("cli; hlt");
}
static void handler_ud(void *fp) {
    struct trap_frame *f = fp;
    dump_frame("#UD Invalid Opcode", f);
    for (;;) asm volatile("cli; hlt");
}
static void handler_nm(void *fp) {
    /* #NM: Device Not Available – FPU no inicializado.
     * En lugar de fallar, habilitar FPU/SSE en el task actual. */
    (void)fp;
    uint64_t cr0;
    asm volatile("mov %%cr0,%0":"=r"(cr0));
    cr0 &= ~(1ULL << 3);   /* limpiar TS (Task Switched) */
    asm volatile("mov %0,%%cr0"::"r"(cr0));
    /* El iretq en el stub reiniciará la instrucción FPU que causó la trampa */
}
static void handler_df(void *fp) {
    (void)fp;
    serial_s("\n[PANIC] #DF Double Fault! Sistema detenido.\n");
    for (;;) asm volatile("cli; hlt");
}
static void handler_gpf(void *fp) {
    struct trap_frame *f = fp;
    dump_frame("#GP General Protection Fault", f);
    /* Diagnostic: dump few bytes at RIP to help identify faulting instruction */
    serial_s("[GP] Dumping bytes at RIP:\n");
    uint8_t *pc = (uint8_t *)(uintptr_t)f->rip;
    for (int i = 0; i < 16; ++i) {
        uint8_t b = 0;
        /* try/catch not available; read directly (may crash if RIP invalid) */
        b = pc[i];
        serial_s(" ");
        serial_x((uint64_t)b);
    }
    serial_s("\n");
    /* Si el fallo fue en CPL=3, matar la tarea en vez de hundir el kernel */
    if ((f->cs & 3) == 3) {
        serial_s("  → Fallo en proceso de usuario; debería terminarse la tarea.\n");
        /* Implementación de do_exit(SIGSEGV) - terminar proceso usuario */
        /* En el futuro conectar con task_kill() para enviar SIGSEGV */
        /* Por ahora: solo loguear y detener */
    }
    for (;;) asm volatile("cli; hlt");
}
static void handler_pf(void *fp) {
    struct trap_frame *f = fp;
    uint64_t cr2;
    asm volatile("mov %%cr2,%0":"=r"(cr2));
    dump_frame("#PF Page Fault", f);
    DUMP_REG("CR2    ", cr2);
    serial_s("  Access: ");
    serial_s(f->error_code & 4 ? "user " : "kernel ");
    serial_s(f->error_code & 2 ? "write " : "read ");
    serial_s(f->error_code & 1 ? "(protection)\n" : "(not-present)\n");
    if (f->error_code & 16) serial_s("  (instruction fetch)\n");
    for (;;) asm volatile("cli; hlt");
}
static void handler_bp(void *fp) {
    struct trap_frame *f = fp;
    serial_s("[DBG] #BP Breakpoint hit at RIP=");
    serial_x(f->rip);
    serial_c('\n');
    /* No haltar — continuar después del int3 */
}
static void handler_of(void *fp) {
    struct trap_frame *f = fp;
    dump_frame("#OF Overflow", f);
    for (;;) asm volatile("cli; hlt");
}

/* ── IRQ handlers ────────────────────────────────────────────────────────── */

/* IRQ0 (vector 0x20): timer del PIC 8259 */
static void handler_timer(void *fp) {
    (void)fp;
    /* Contador de diagnóstico: imprimir cada 100 ticks para evitar spamming */
    static uint64_t __tick_cnt = 0;
    __tick_cnt++;
    scheduler_tick();      /* contabilizar tick; posible preempción */
    /* Avoid printing from the timer handler to prevent reentrant serial
     * usage and potential corruption; keep handler minimal. */
    pic_eoi(0x20);
}

/* IRQ1 (vector 0x21): teclado PS/2 */
static void handler_keyboard(void *fp) {
    (void)fp;
    /* El driver PS/2 lee el scancode en su propio poll;
     * aquí solo mandamos el EOI para que el PIC libere la línea. */
    pic_eoi(0x21);
}

/* IRQ genérica: solo EOI */
static void handler_irq_generic(void *fp) {
    struct trap_frame *f = (struct trap_frame *)fp;
    pic_eoi((uint8_t)f->vector);
}

/* ── traps_init ─────────────────────────────────────────────────────────── */
void traps_init(void)
{
    /* Excepciones CPU */
    idt_register_handler(0,  handler_de);
    idt_register_handler(3,  handler_bp);
    idt_register_handler(4,  handler_of);
    idt_register_handler(6,  handler_ud);
    idt_register_handler(7,  handler_nm);   /* FPU not available */
    idt_register_handler(8,  handler_df);
    idt_register_handler(13, handler_gpf);
    idt_register_handler(14, handler_pf);

    /* IRQs del PIC 8259 (remapeadas a 0x20-0x2F) */
    idt_register_handler(0x20, handler_timer);
    idt_register_handler(0x21, handler_keyboard);
    for (int i = 0x22; i <= 0x2F; ++i)
        idt_register_handler(i, handler_irq_generic);
}
