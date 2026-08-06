#include <stdint.h>
#include <stddef.h>

extern void *wm_create_window(int x, int y, int w, int h, const char *title);
extern void wm_clear_window(void *win, uint32_t color);
extern void wm_write(void *win, int x, int y, const char *text, uint32_t color);
extern void wm_fill_rect(void *win, int x, int y, int w, int h, uint32_t color);
extern int fast_ipc_open_port(const char *name);
extern int fast_ipc_send(int port, const void *data, uint32_t size);
extern int fast_ipc_recv(int port, void *data, uint32_t size);

int shell_main(void)
{
    void *window = wm_create_window(70, 70, 520, 320, "Shell");
    if (!window) return -1;
    wm_clear_window(window, 0xFF08121C);
    wm_fill_rect(window, 0, 0, 520, 24, 0xFF2E6BFF);
    wm_write(window, 14, 8, "GPT-OS Shell", 0xFFFFFFFF);
    wm_write(window, 14, 44, "/bin /dev /etc /lib /usr", 0xFF8DC6FF);
    wm_write(window, 14, 66, "IPC ready", 0xFF5CFFB2);
    int port = fast_ipc_open_port("shell");
    if (port >= 0) {
        char msg[] = "ping";
        fast_ipc_send(port, msg, 4);
        char out[16] = {0};
        if (fast_ipc_recv(port, out, sizeof(out)) < 0) {
            wm_write(window, 14, 92, "IPC: no peer yet", 0xFFFFD76A);
        } else {
            wm_write(window, 14, 92, out, 0xFF70C6FF);
        }
    }
    return 0;
}
