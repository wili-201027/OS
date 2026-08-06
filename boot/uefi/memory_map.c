#include <efi.h>
#include <efilib.h>

EFI_MEMORY_DESCRIPTOR *uefi_memory_map = NULL;
UINTN uefi_memory_map_size = 0;
UINTN uefi_memory_map_desc_size = 0;

void capture_memory_map(EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS status;
    UINTN map_key;
    UINT32 desc_version;

    uefi_memory_map = NULL;
    uefi_memory_map_size = 0;
    uefi_memory_map_desc_size = 0;

    status = SystemTable->BootServices->GetMemoryMap(
        &uefi_memory_map_size,
        uefi_memory_map,
        &map_key,
        &uefi_memory_map_desc_size,
        &desc_version
    );

    if (status != EFI_BUFFER_TOO_SMALL) {
        Print(L"[UEFI] GetMemoryMap initial call failed\n");
        while (1);
    }

    for (int retry = 0; retry < 4; ++retry) {
        do {
            uefi_memory_map_size += 2 * uefi_memory_map_desc_size;
            if (uefi_memory_map) {
                SystemTable->BootServices->FreePool(uefi_memory_map);
                uefi_memory_map = NULL;
            }

            status = SystemTable->BootServices->AllocatePool(
                EfiLoaderData,
                uefi_memory_map_size,
                (void**)&uefi_memory_map
            );
            if (EFI_ERROR(status)) {
                Print(L"[UEFI] Memory map allocation failed\n");
                while (1);
            }

            status = SystemTable->BootServices->GetMemoryMap(
                &uefi_memory_map_size,
                uefi_memory_map,
                &map_key,
                &uefi_memory_map_desc_size,
                &desc_version
            );
        } while (status == EFI_BUFFER_TOO_SMALL);

        if (EFI_ERROR(status)) {
            Print(L"[UEFI] GetMemoryMap failed\n");
            while (1);
        }

        status = SystemTable->BootServices->ExitBootServices(
            SystemTable->ImageHandle,
            map_key
        );
        if (!EFI_ERROR(status))
            return;

        if (status != EFI_INVALID_PARAMETER)
            break;

        Print(L"[UEFI] ExitBootServices retrying due memory map change\n");
        uefi_memory_map_size = 0;
        uefi_memory_map_desc_size = 0;
    }

    if (EFI_ERROR(status)) {
        Print(L"[UEFI] ExitBootServices failed\n");
        while (1);
    }
}
