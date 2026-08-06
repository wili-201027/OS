include build/toolchain.mk

# 1. Ajustamos la ruta base: Ahora apunta a la carpeta local 'userland'
USER_SRC_ROOT := userland
USER_OBJ_ROOT := build/obj/userland
USER_ELF      := build/userland.elf

# 2. Enumerar fuentes (usando la nueva ruta sin ../)
# Exclusión: compositor se compila en el kernel, no en userland.
# Además, la mayoría de los programas de userland se construyen como ejecutables
# independientes y no deben enlazarse en el ELF monolítico de userland.
# Solo se agregan shell y dns_resolver porque process_launcher los referencia.
USER_C   := $(shell find $(USER_SRC_ROOT) -type f -name '*.c' -not -path '*/compositor/*' -not -path '*/programs/*' -not -name 'syscall_complete.c')
USER_S   := $(shell find $(USER_SRC_ROOT) -type f -name '*.S' -not -path '*/compositor/*' -not -path '*/programs/*')
USER_C_EXTRA := $(USER_SRC_ROOT)/programs/shell.c $(USER_SRC_ROOT)/programs/dns_resolver.c

# 3. Mapear a objetos
USER_OBJS_C   := $(patsubst $(USER_SRC_ROOT)/%.c,  $(USER_OBJ_ROOT)/%.o,$(USER_C))
USER_OBJS_S   := $(patsubst $(USER_SRC_ROOT)/%.S,  $(USER_OBJ_ROOT)/%.o,$(USER_S))
USER_OBJS_C_EXTRA := $(patsubst $(USER_SRC_ROOT)/%.c,$(USER_OBJ_ROOT)/%.o,$(USER_C_EXTRA))

USER_OBJS := $(USER_OBJS_C) $(USER_OBJS_S) $(USER_OBJS_C_EXTRA)

#USER_OBJS += build/obj/userland/libc/syscall_arch.o

.PHONY: all clean

all: $(USER_ELF)

$(USER_ELF): $(USER_OBJS)
	@echo "[LD] $@"
	$(LD) $(LDFLAGS) -o $@ $^

# 4. Reglas de compilación corregidas
$(USER_OBJ_ROOT)/%.o: $(USER_SRC_ROOT)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $< -> $@"
	$(CC) $(CFLAGS) \
		-I$(USER_SRC_ROOT) \
		-I$(USER_SRC_ROOT)/file_manager \
		-I$(USER_SRC_ROOT)/ui_lib \
		-I$(USER_SRC_ROOT)/app_launcher \
		-I$(USER_SRC_ROOT)/compositor \
		-I$(USER_SRC_ROOT)/libc \
		-c $< -o $@

$(USER_OBJ_ROOT)/%.o: $(USER_SRC_ROOT)/%.S
	@mkdir -p $(dir $@)
	@echo "[AS] $< -> $@"
	$(AS) $(ASMFLAGS) -o $@ $<

clean:
	@echo "Cleaning userland objects..."
	@rm -rf $(USER_OBJ_ROOT)
	@rm -f $(USER_ELF)