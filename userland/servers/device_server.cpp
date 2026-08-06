// userland/servers/device_server.cpp
// Device server - manages input, PS/2, NIC, framebuffer hardware

#include <stdint.h>
#include <stddef.h>

extern "C" {
    void sys_yield(void);
}

extern "C"
void device_server_start(void)
{
    // Device server stub
    // In a full implementation, this would manage PS/2 input devices,
    // hotplug events, and expose device information via IPC.
    while (1) {
        sys_yield();
    }
}