// kernel/objects/loader.c
// Userland loading desactivado temporalmente.
// El compositor corre en ring-0 directamente desde kernel_main.

#include <stdint.h>
#include <stddef.h>

void exec_init(void *init_image, uint64_t size)
{
    (void)init_image;
    (void)size;
    // No-op: el compositor se lanza desde kernel_main en modo kernel.
    // Cuando el context switch + iretq estén completamente probados,
    // esta función lanzará el proceso de usuario real.
}
