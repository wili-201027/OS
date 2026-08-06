# build/kernel.mk
include build/toolchain.mk

.PHONY: all clean

# 1. Definir directorios (path absoluto para robustez en Windows/MSYS)
MAKEFILE_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)/..)

OBJ_DIR := $(abspath $(ROOT_DIR)/build/obj/kernel)
SRC_DIR := $(abspath $(ROOT_DIR)/kernel)
COMPOSITOR_SRC_DIR := $(abspath $(ROOT_DIR)/userland/compositor)
USERLAND_SRC_DIR := $(abspath $(ROOT_DIR)/userland)
KERNEL_ELF := $(abspath $(ROOT_DIR)/build/kernel.elf)

# 2. Encontrar todos los archivos fuente (C, C++, ASM)
KERNEL_SRC_C   := $(shell find $(SRC_DIR) -name '*.c')
KERNEL_SRC_S   := $(shell find $(SRC_DIR) -name '*.S')
COMPOSITOR_SRC_C := $(shell find $(COMPOSITOR_SRC_DIR) -name '*.c' 2>/dev/null)
COMPOSITOR_DEPS_SRC := $(ROOT_DIR)/userland/file_manager/file_manager.c \
                       $(ROOT_DIR)/userland/app_launcher/app_launcher.c \
                       $(ROOT_DIR)/userland/libc/stdio.c \
                       $(ROOT_DIR)/kernel/userland_kernel_stubs.c
COMPOSITOR_DEPS_OBJS := $(OBJ_DIR)/userland/file_manager/file_manager.o \
                       $(OBJ_DIR)/userland/app_launcher/app_launcher.o \
                       $(OBJ_DIR)/userland/libc/stdio.o \
                       $(OBJ_DIR)/userland_kernel_stubs.o

# 3. Transformar fuentes en objetos manteniendo la estructura de carpetas
# Esto mapea kernel/arch/x86/gdt.c -> build/obj/kernel/arch/x86/gdt.o
# y userland/compositor/compositor.c -> build/obj/kernel/compositor/compositor.o
KERNEL_OBJS := $(KERNEL_SRC_C:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) \
               $(KERNEL_SRC_S:$(SRC_DIR)/%.S=$(OBJ_DIR)/%.o) \
               $(COMPOSITOR_SRC_C:$(COMPOSITOR_SRC_DIR)/%.c=$(OBJ_DIR)/compositor/%.o) \
               $(COMPOSITOR_DEPS_OBJS)

all: $(KERNEL_ELF)

# Regla de enlace final
$(KERNEL_ELF): $(KERNEL_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD] $@"
	$(LD) $(LDFLAGS) -T boot/uefi/linker.ld -o $@ $^

# --- REGLAS GENÉRICAS ---
# Esta única regla maneja CUALQUIER .c en CUALQUIER subdirectorio
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "[AS] $<"
	$(AS) $(ASMFLAGS) -c $< -o $@

# Regla específica para compositor
$(OBJ_DIR)/compositor/%.o: $(COMPOSITOR_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para dependencias de userland usadas por el compositor
$(OBJ_DIR)/userland/%.o: $(USERLAND_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf build/obj/kernel $(KERNEL_ELF)

debug:
	@echo "KERNEL_ELF:  $(KERNEL_ELF)"
	@echo "SRC_DIR:     $(SRC_DIR)"
	@echo "SOURCES:     $(KERNEL_SRC_C)"
	@echo "OBJECTS:     $(KERNEL_OBJS)"