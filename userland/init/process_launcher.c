#include "process_launcher.h"
#include <stdint.h>

extern int shell_main(void);
extern int dns_resolver_main(const char *host);
extern void graphics_server_start(void);
extern void sys_yield(void);

static int run_shell(void)
{
    return shell_main();
}

static int run_dns(const char *host)
{
    return dns_resolver_main(host);
}

int userland_launch_app(const char *name, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!name) return -1;
    if (name[0] == '/' || name[0] == '.') {
        if (name[0] == '/' && name[1] == 'b' && name[2] == 'i' && name[3] == 'n') {
            return run_shell();
        }
        if (name[0] == '/' && name[1] == 'u' && name[2] == 's' && name[3] == 'r') {
            graphics_server_start();
            return 0;
        }
    }

    if (name[0] == 's' && name[1] == 'h' && name[2] == 'e' && name[3] == 'l' && name[4] == 'l') {
        return run_shell();
    }

    if (name[0] == 'd' && name[1] == 'n' && name[2] == 's') {
        return run_dns(argc > 1 ? argv[1] : "localhost");
    }

    return -1;
}
