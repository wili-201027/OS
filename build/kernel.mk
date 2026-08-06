# build/kernel.mk
include build/toolchain.mk

.PHONY: all clean

# 1. Definir directorios (path absoluto para robustez en Windows/MSYS)
MAKEFILE_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)/..)

OBJ_DIR := $(abspath $(ROOT_DIR)/build/obj/kernel)
SRC_DIR := $(abspath $(ROOT_DIR)/kernel)
COMPOSITOR_SRC_DIR := $(abspath $(ROOT_DIR)/userland/compositor)
KERNEL_ELF := $(abspath $(ROOT_DIR)/build/kernel.elf)

# 2. Encontrar todos los archivos fuente (C, C++, ASM)
KERNEL_SRC_C   := $(shell find $(SRC_DIR) -name '*.c')
KERNEL_SRC_CPP := $(shell find $(SRC_DIR) -name '*.cpp')
KERNEL_SRC_S   := $(shell find $(SRC_DIR) -name '*.S')
COMPOSITOR_SRC_CPP := $(shell find $(COMPOSITOR_SRC_DIR) -name '*.cpp' 2>/dev/null)

# 3. Transformar fuentes en objetos manteniendo la estructura de carpetas
# Esto mapea kernel/arch/x86/gdt.c -> build/obj/kernel/arch/x86/gdt.o
# y userland/compositor/compositor.cpp -> build/obj/kernel/compositor/compositor.o
KERNEL_OBJS := $(KERNEL_SRC_C:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) \
               $(KERNEL_SRC_CPP:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o) \
               $(KERNEL_SRC_S:$(SRC_DIR)/%.S=$(OBJ_DIR)/%.o) \
               $(COMPOSITOR_SRC_CPP:$(COMPOSITOR_SRC_DIR)/%.cpp=$(OBJ_DIR)/compositor/%.o)

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

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "[AS] $<"
	$(AS) $(ASMFLAGS) -c $< -o $@

# Regla específica para compositor
$(OBJ_DIR)/compositor/%.o: $(COMPOSITOR_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf build/obj/kernel $(KERNEL_ELF)

debug:
	@echo "KERNEL_ELF:  $(KERNEL_ELF)"
	@echo "SRC_DIR:     $(SRC_DIR)"
	@echo "SOURCES:     $(KERNEL_SRC_C) $(KERNEL_SRC_CPP)"
	@echo "OBJECTS:     $(KERNEL_OBJS)"