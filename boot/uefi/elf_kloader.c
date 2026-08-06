#include <efi.h>
#include <efilib.h>
#include <elf.h>

extern void capture_memory_map(EFI_SYSTEM_TABLE*);

static int elf_validate(Elf64_Ehdr *hdr)
{
    return hdr->e_ident[EI_MAG0] == ELFMAG0 &&
           hdr->e_ident[EI_MAG1] == ELFMAG1 &&
           hdr->e_ident[EI_MAG2] == ELFMAG2 &&
           hdr->e_ident[EI_MAG3] == ELFMAG3 &&
           hdr->e_ident[EI_CLASS] == ELFCLASS64 &&
           hdr->e_machine == EM_X86_64;
}

EFI_STATUS load_kernel_elf(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_PHYSICAL_ADDRESS *entry_point
)
{
    EFI_FILE_IO_INTERFACE *fs;
    EFI_FILE_HANDLE root, kernel;
    EFI_STATUS status;

    status = SystemTable->BootServices->LocateProtocol(
        &FileSystemProtocol, NULL, (void**)&fs
    );
    if (EFI_ERROR(status)) return status;

    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) return status;

    status = root->Open(
        root,
        &kernel,
        L"\\kernel.elf",
        EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR(status)) return status;

    Elf64_Ehdr ehdr;
    UINTN size = sizeof(ehdr);
    status = kernel->Read(kernel, &size, &ehdr);
    if (EFI_ERROR(status) || size != sizeof(ehdr)) return EFI_LOAD_ERROR;

    if (!elf_validate(&ehdr)) return EFI_LOAD_ERROR;

    status = kernel->SetPosition(kernel, ehdr.e_phoff);
    if (EFI_ERROR(status)) return status;

    if (ehdr.e_phnum == 0 || ehdr.e_phnum > 1024) return EFI_LOAD_ERROR;

    for (UINT16 i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr ph;
        size = sizeof(ph);
        status = kernel->Read(kernel, &size, &ph);
        if (EFI_ERROR(status) || size != sizeof(ph)) return EFI_LOAD_ERROR;

        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;
        if (ph.p_filesz > ph.p_memsz) return EFI_LOAD_ERROR;

        EFI_PHYSICAL_ADDRESS seg_addr = ph.p_paddr;
        if (seg_addr == 0) return EFI_LOAD_ERROR;

        UINTN pages = EFI_SIZE_TO_PAGES(ph.p_memsz);
        status = SystemTable->BootServices->AllocatePages(
            AllocateAddress,
            EfiLoaderData,
            pages,
            &seg_addr
        );
        if (EFI_ERROR(status)) return status;

        status = kernel->SetPosition(kernel, ph.p_offset);
        if (EFI_ERROR(status)) return status;

        size = ph.p_filesz;
        status = kernel->Read(kernel, &size, (void*)(uintptr_t)seg_addr);
        if (EFI_ERROR(status) || size != ph.p_filesz) return EFI_LOAD_ERROR;

        for (UINT64 j = ph.p_filesz; j < ph.p_memsz; j++)
            ((uint8_t*)(uintptr_t)seg_addr)[j] = 0;
    }

    capture_memory_map(SystemTable);

    *entry_point = ehdr.e_entry;
    return EFI_SUCCESS;
}
