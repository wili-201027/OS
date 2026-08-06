#include <efi.h>
#include <efilib.h>
#include <stdint.h>

extern EFI_STATUS load_kernel_elf(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_PHYSICAL_ADDRESS *entry_point
);

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    Print(L"[UEFI] BootX64 started\n");

    EFI_PHYSICAL_ADDRESS kernel_entry = 0;

    EFI_STATUS status = load_kernel_elf(SystemTable, &kernel_entry);
    if (EFI_ERROR(status)) {
        Print(L"[UEFI] Kernel ELF load failed\n");
        while (1);
    }

    Print(L"[UEFI] Kernel entry @ 0x%lx\n", kernel_entry);

    if (kernel_entry == 0) {
        Print(L"[UEFI] Invalid kernel entry point\n");
        while (1) {
            asm volatile("hlt");
        }
    }

    Print(L"[UEFI] Jumping to kernel\n");

    typedef void (*KernelEntry)(void *, uint32_t);
    KernelEntry kernel = (KernelEntry)(uintptr_t)kernel_entry;
    kernel(NULL, 0);

    while (1) {
        asm volatile("hlt");
    }
    return EFI_SUCCESS;
}
