MAKEFLAGS += --no-print-directory
ISO_DIR := iso_root
ISO_IMG := build/os.iso
KERNEL_ELF := build/kernel.elf
USER_ELF   := build/userland.elf

all: kernel userland

.PHONY: all iso kernel userland clean help

kernel:
	$(MAKE) -C . -f build/kernel.mk


userland:
	$(MAKE) -C . -f build/userland.mk

iso: $(ISO_IMG)

$(ISO_IMG): $(KERNEL_ELF) $(USER_ELF)
	@echo "[ISO] Preparant estructura..."
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	@cp $(USER_ELF)   $(ISO_DIR)/boot/userland.elf 2>/dev/null || true
	@printf 'set timeout=2\n'                           > $(ISO_DIR)/boot/grub/grub.cfg
	@printf 'set default=0\n'                          >> $(ISO_DIR)/boot/grub/grub.cfg
	@printf 'terminal_input console\n'                 >> $(ISO_DIR)/boot/grub/grub.cfg
	@printf 'terminal_output console\n'                >> $(ISO_DIR)/boot/grub/grub.cfg
	@printf 'set gfxpayload=text\n'                    >> $(ISO_DIR)/boot/grub/grub.cfg
	@printf 'menuentry "GPT-OS" {\n'                   >> $(ISO_DIR)/boot/grub/grub.cfg
	@printf '    multiboot2 /boot/kernel.elf\n'        >> $(ISO_DIR)/boot/grub/grub.cfg
	@printf '    module2 /boot/userland.elf initrd\n'  >> $(ISO_DIR)/boot/grub/grub.cfg
	@printf '    boot\n'                               >> $(ISO_DIR)/boot/grub/grub.cfg
	@printf '}\n'                                      >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "[ISO] Generant ISO..."
	@grub-mkrescue -o $@ $(ISO_DIR) 2>/dev/null || true

clean:
	rm -rf build/obj build/kernel.elf build/userland.elf build/os.iso

help:
	@echo "Available targets: all, kernel, userland, iso, clean"
