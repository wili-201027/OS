// userland/init/init.cpp
#include <stdint.h>

extern "C" {
    void fs_server_start(void);
    void device_server_start(void);
    void gpu_server_start(void);
    void quantum_server_start(void);
    void graphics_server_start(void);
    void sys_yield(void);
    int userland_launch_app(const char *name, int argc, char **argv);
}

int main(void)
{
    // 1. Servers Essencials
    fs_server_start();
    device_server_start();
    gpu_server_start();
    quantum_server_start();
    graphics_server_start();

    // 2. Lanzar aplicaciones de userland desde la capa superior
    static char *argv[] = { (char *)"shell", (char *)"localhost" };
    userland_launch_app("shell", 2, argv);

    // Compositor runs in kernel (ring-0), started by kernel_main
    for(;;) {
        sys_yield();
    }
}
