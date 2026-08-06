// userland/file_manager/file_manager_integration.h
// Header de integración para incluir en compositor

#ifndef FILE_MANAGER_INTEGRATION_H
#define FILE_MANAGER_INTEGRATION_H

#include <stdint.h>

// ─── Forward declarations ─────────────────────────────────────────────────
extern void file_manager_init(void);
extern void file_manager_shutdown(void);
extern void file_manager_process_input(void);
extern void file_manager_draw(uint32_t *framebuffer, uint32_t fb_width, uint32_t fb_height);
extern void file_manager_update(uint32_t *framebuffer, uint32_t fb_width, uint32_t fb_height);

// ─── Macros de integración ────────────────────────────────────────────────

// Llamar en el compositor main loop para actualizar el gestor
#define UPDATE_FILE_MANAGER(fb, w, h) \
    do { \
        file_manager_update(fb, w, h); \
    } while(0)

// Inicializar el sistema de archivos al arrancar
#define INIT_FILE_MANAGER() file_manager_init()

// Limpiar al apagar
#define SHUTDOWN_FILE_MANAGER() file_manager_shutdown()

#endif // FILE_MANAGER_INTEGRATION_H
