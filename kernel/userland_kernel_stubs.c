// userland/compositor/userland_kernel_stubs.c
// Minimal kernel-linked stubs for userland compositor integration.

#include <stdint.h>

int syscall_opendir(const char *path) { (void)path; return -1; }
int syscall_readdir(int dh, void *entry) { (void)dh; (void)entry; return -1; }
int syscall_closedir(int dh) { (void)dh; return -1; }
int syscall_fork(void) { return -1; }
int syscall_execve(const char *path, char *const argv[], char *const envp[]) {
    (void)path;
    (void)argv;
    (void)envp;
    return -1;
}
int syscall_wait(int pid, int *status) { (void)pid; (void)status; return -1; }
