# build/toolchain.mk
CROSS_COMPILE ?=

ifeq ($(origin CROSS_COMPILE), undefined)
CROSS_COMPILE :=
endif

ifneq ($(strip $(CROSS_COMPILE)),)
ifneq ($(wildcard $(CROSS_COMPILE)gcc.exe),)
CC      := $(CROSS_COMPILE)gcc.exe
CXX     := $(CROSS_COMPILE)g++.exe
AS      := $(CROSS_COMPILE)as.exe
LD      := $(CROSS_COMPILE)ld.exe
OBJCOPY := $(CROSS_COMPILE)objcopy.exe
OBJDUMP := $(CROSS_COMPILE)objdump.exe
else ifneq ($(wildcard $(CROSS_COMPILE)gcc),)
CC      := $(CROSS_COMPILE)gcc
CXX     := $(CROSS_COMPILE)g++
AS      := $(CROSS_COMPILE)as
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
else
CC      :=
CXX     :=
AS      :=
LD      :=
OBJCOPY :=
OBJDUMP :=
endif
endif

# IMPORTANT: GNU Make defines built-in default values for CC (=cc), CXX (=g++),
# AS (=as) and LD (=ld) even when the user never set them. That means the old
# "ifeq ($(strip $(CC)),)" style checks below NEVER fired, because $(CC) was
# never actually empty -- it silently fell through to Make's implicit "cc"/"ld",
# which on this machine resolve to the MSYS2/MinGW toolchain (PE/COFF-oriented),
# not the x86_64-elf cross toolchain in C:/opt/cross/bin. That mismatch is what
# produced link errors like "ld: unrecognized option '-z'" and, more
# dangerously, could silently miscompile freestanding kernel code with the
# wrong ABI when the link happened to "succeed".
#
# Fix: test $(origin VAR) instead of emptiness. "default" means Make supplied
# its own built-in value and the user/CROSS_COMPILE branch above did not.
ifeq ($(origin CC),default)
CC :=
endif
ifeq ($(origin CXX),default)
CXX :=
endif
ifeq ($(origin AS),default)
AS :=
endif
ifeq ($(origin LD),default)
LD :=
endif

ifeq ($(strip $(CC)),)
ifneq ($(wildcard C:/opt/cross/bin/x86_64-elf-gcc.exe),)
CC      := C:/opt/cross/bin/x86_64-elf-gcc.exe
CXX     := C:/opt/cross/bin/x86_64-elf-g++.exe
AS      := C:/opt/cross/bin/x86_64-elf-as.exe
LD      := C:/opt/cross/bin/x86_64-elf-ld.exe
OBJCOPY := C:/opt/cross/bin/x86_64-elf-objcopy.exe
OBJDUMP := C:/opt/cross/bin/x86_64-elf-objdump.exe
else ifneq ($(shell command -v x86_64-elf-gcc 2>/dev/null),)
CC      := x86_64-elf-gcc
CXX     := x86_64-elf-g++
AS      := x86_64-elf-as
LD      := x86_64-elf-ld
OBJCOPY := x86_64-elf-objcopy
OBJDUMP := x86_64-elf-objdump
else ifneq ($(wildcard C:/msys64/mingw64/bin/gcc.exe),)
CC      := C:/msys64/mingw64/bin/gcc.exe
CXX     := C:/msys64/mingw64/bin/g++.exe
AS      := C:/msys64/mingw64/bin/as.exe
LD      := C:/msys64/mingw64/bin/ld.exe
OBJCOPY := C:/msys64/mingw64/bin/objcopy.exe
OBJDUMP := C:/msys64/mingw64/bin/objdump.exe
else ifneq ($(shell command -v clang 2>/dev/null),)
CC      := clang
CXX     := clang++
AS      := clang
LD      := ld.lld
OBJCOPY := llvm-objcopy
OBJDUMP := llvm-objdump
else ifneq ($(shell command -v gcc 2>/dev/null),)
CC      := gcc
CXX     := g++
AS      := as
LD      := ld
OBJCOPY := objcopy
OBJDUMP := objdump
else
$(error No suitable toolchain found. Install x86_64-elf-gcc or clang, or set CROSS_COMPILE to a valid prefix.)
endif
endif

$(info [toolchain] using CC=$(CC) LD=$(LD))

CFLAGS  := -O2 -ffreestanding -Wall -Wextra -fno-stack-protector -mno-red-zone -m64
CXXFLAGS:= $(CFLAGS) -fno-exceptions -fno-rtti
ifeq ($(findstring mingw,$(CC))$(findstring MinGW,$(CC)),)
LDFLAGS := -nostdlib -z max-page-size=0x1000
else
LDFLAGS := -nostdlib -m elf_x86_64
endif

ASMFLAGS:=

MKDIR   := mkdir -p
RM      := rm -rf
