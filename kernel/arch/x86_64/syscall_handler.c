// kernel/arch/x86_64/syscall_handler.c
//
// Despachador de llamadas al sistema.
// Recibe el bloque de registros guardado por syscall.S:
//   regs[0]=r9  [1]=r8  [2]=r10  [3]=rdx
//   regs[4]=rsi [5]=rdi [6]=rax  [7]=r11  [8]=rcx (rip usuario)

#include <stdint.h>
#include <stddef.h>

#include "../../syscalls/fs_syscalls.h"

/* Syscall numbers (mantener sincronizado con userland/libc/syscall.h) */
#define SYS_YIELD         1
#define SYS_EXIT          2
#define SYS_WRITE         3
#define SYS_READ          4
#define SYS_FB_WIDTH      5
#define SYS_FB_HEIGHT     6
#define SYS_FB_ADDR       7
#define SYS_SLEEP         10
#define SYS_THREAD_CREATE 11
#define SYS_THREAD_EXIT   12
#define SYS_WIN_CLEAR     19
#define SYS_WIN_WRITE     20
#define SYS_WIN_FILL      21
#define SYS_GET_TICKS     30

extern uint32_t  fb_get_width(void);
extern uint32_t  fb_get_height(void);
extern uint32_t *fb_get_addr(void);
extern void      schedule(void);
extern void      scheduler_yield(void);
extern uint64_t  scheduler_get_ticks(void);
extern int       thread_create(void *entry, void *arg, void *stack_bottom, uint64_t stack_size);
extern void      task_exit(int exit_code);

uint64_t syscall_handler(uint64_t *regs)
{
    if (!regs) return (uint64_t)-1;

    uint64_t num  = regs[6];  /* RAX: syscall number */
    uint64_t arg1 = regs[5];  /* RDI */
    uint64_t arg2 = regs[4];  /* RSI */
    uint64_t arg3 = regs[3];  /* RDX */
    uint64_t arg4 = regs[2];  /* R10 */
    uint64_t arg5 = regs[1];  /* R8  */
    uint64_t arg6 = regs[0];  /* R9  */

    /* Route filesystem syscalls to the FS dispatcher */
    if (num >= SYSCALL_FS_OPEN && num <= SYSCALL_FS_CHDIR) {
        extern long dispatch_fs_syscall(uint32_t syscall_num, uint64_t arg1, uint64_t arg2,
                                         uint64_t arg3, uint64_t arg4);
        return (uint64_t)dispatch_fs_syscall((uint32_t)num, arg1, arg2, arg3, arg4);
    }

    /* Basic IPC hooks: sys_ipc_open is available as a stub; others return ENOSYS */
    if (num == 200) { /* SYS_IPC_OPEN */
        extern int sys_ipc_open(const char *s, const char *r);
        return (uint64_t)sys_ipc_open((const char *)arg1, (const char *)arg2);
    }
    if (num == 201) {
        extern int fast_ipc_send(int port, const void *data, uint32_t size);
        return (uint64_t)fast_ipc_send((int)arg1, (const void *)arg2, (uint32_t)arg3);
    }
    if (num == 202) {
        extern int fast_ipc_recv(int port, void *data, uint32_t size);
        return (uint64_t)fast_ipc_recv((int)arg1, (void *)arg2, (uint32_t)arg3);
    }
    if (num == 203) {
        extern int fast_ipc_close(int port);
        return (uint64_t)fast_ipc_close((int)arg1);
    }

    switch (num) {

    case SYS_YIELD:
        scheduler_yield();
        return 0;

    case SYS_EXIT:
        task_exit((int)arg1);
        return 0;

    case SYS_THREAD_CREATE:
        return (uint64_t)thread_create((void *)arg1, (void *)arg2, (void *)arg3, arg4);

    case SYS_THREAD_EXIT:
        task_exit((int)arg1);
        return 0;

    case SYS_FB_WIDTH:  return (uint64_t)fb_get_width();
    case SYS_FB_HEIGHT: return (uint64_t)fb_get_height();
    case SYS_FB_ADDR:   return (uint64_t)(uintptr_t)fb_get_addr();

    case SYS_GET_TICKS: return scheduler_get_ticks();

    case SYS_SLEEP:
        /* Implementación trivial: ceder la CPU N veces */
        for (uint64_t i = 0; i < arg1; ++i) scheduler_yield();
        return 0;

    /* Window Manager API */
    case SYS_WIN_CLEAR:
    case SYS_WIN_WRITE:
    case SYS_WIN_FILL:
        /* Pendiente de implementar el WM */
        (void)arg1; (void)arg2; (void)arg3;
        (void)arg4; (void)arg5; (void)arg6;
        return (uint64_t)-38;   /* -ENOSYS */

    default:
        return (uint64_t)-38;   /* -ENOSYS */
    }
}
