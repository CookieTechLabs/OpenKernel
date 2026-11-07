
CC = gcc
AS = nasm
AR = ar
RANLIB = ranlib
HOST_CC ?= cc

RELVERSION   := 0
SUBVERSION   := 2
PATCHVERSION := 0
EXTRAVERSION := -rc2

PRETTY_NAME := Cyber Diva

BUILD_DIR = build
KERNEL = $(BUILD_DIR)/kernel.bin
PLAIN_KERNEL = ./openkernel
OPENKERNEL_A = $(BUILD_DIR)/openkernel.a
LINUX_PROGRAM ?= $(BUILD_DIR)/openkernel
HOST_SOURCES = host/main.c

BUILD_NUMBER_FILE = .build_number

VERSION_DIR = include/generated
VERSION_HEADER = $(VERSION_DIR)/version.h
BUILDENV_HEADER = $(VERSION_DIR)/buildenv.h
VERSION = $(RELVERSION).$(SUBVERSION).$(PATCHVERSION)$(EXTRAVERSION)

MOD_DIR = $(BUILD_DIR)/mod
MOD_SOURCES = $(wildcard modules/*.c)
MOD_OBJS = $(patsubst modules/%.c,$(MOD_DIR)/%.mo,$(MOD_SOURCES))
MODULE_INSTALL_DIR = $(BUILD_DIR)/modules/$(VERSION)
MODULE_IMAGES = $(patsubst $(MOD_DIR)/%.mo,$(MODULE_INSTALL_DIR)/%.ko,$(MOD_OBJS))

# oconf configuration tools.
OCONF_DIR = scripts/oconf
OCONF_CONFIG = .config
OCONF_PARSER = $(OCONF_DIR)/config-parser
OCONF_BUILDER = $(OCONF_DIR)/config-build

# Compiler flags.
CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
	-fno-pie -Iinclude -MMD -MP -DCONFIG_DRM_KERNELPANIC

LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 \
	-nostdlib -fno-pie -no-pie

PLAIN_LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 \
	-nostdlib -fno-pie -no-pie -Wl,-e,kernel_main

ASFLAGS = -f elf32
HOST_CFLAGS = -std=gnu99 -O2 -Wall -Wextra -Iinclude
OCONF_CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror

# Find every .Ocfg file in the repository.
OCONF_FILES = $(shell find . -type f -name "*.Ocfg" \
	-not -path "./.git/*" \
	-not -path "./$(BUILD_DIR)/*")

# Collect all source paths declared in .Ocfg files.
OCONF_ALL_SOURCES = $(shell \
	for cfg in $(OCONF_FILES); do \
		dir=$$(dirname "$$cfg"); \
		awk -v dir="$$dir" '\
			function trim(s) { \
				sub(/^[ \t]+/, "", s); \
				sub(/[ \t\r]+$$/, "", s); \
				return s; \
			} \
			{ \
				line = trim($$0); \
				if (line == "" || line ~ /^#/) next; \
				eq = index(line, "="); \
				if (!eq) next; \
				key = trim(substr(line, 1, eq - 1)); \
				value = trim(substr(line, eq + 1)); \
				if (key == "SOURCE") print dir "/" value; \
			}' "$$cfg"; \
	done)

# Collect sources whose active CONFIG symbol is enabled in .config.
OCONF_ENABLED_SOURCES = $(shell \
	for cfg in $(OCONF_FILES); do \
		dir=$$(dirname "$$cfg"); \
		awk -v dir="$$dir" -v config_file="$(OCONF_CONFIG)" '\
			function trim(s) { \
				sub(/^[ \t]+/, "", s); \
				sub(/[ \t\r]+$$/, "", s); \
				return s; \
			} \
			BEGIN { \
				while ((getline line < config_file) > 0) { \
					if (line ~ /^CONFIG_[A-Za-z0-9_]+=y$$/) { \
						eq = index(line, "="); \
						enabled[substr(line, 1, eq - 1)] = 1; \
					} \
				} \
				close(config_file); \
			} \
			{ \
				line = trim($$0); \
				if (line == "" || line ~ /^#/) next; \
				eq = index(line, "="); \
				if (!eq) next; \
				key = trim(substr(line, 1, eq - 1)); \
				value = trim(substr(line, eq + 1)); \
				if (key == "CONFIG") config = value; \
				else if (key == "SOURCE" && enabled[config]) \
					print dir "/" value; \
			}' "$$cfg"; \
	done)

# Find ordinary C sources, excluding sources managed by .Ocfg files.
ALL_C_SOURCES = $(shell find . -name "*.c" \
	-not -path "./$(BUILD_DIR)/*" \
	-not -path "./modules/*" \
	-not -path "./host/*" \
	-not -path "./bionicbox/*" \
	-not -path "./scripts/*" \
	-not -path "./.git/*")

C_SOURCES = $(filter-out $(OCONF_ALL_SOURCES),$(ALL_C_SOURCES)) \
	$(sort $(OCONF_ENABLED_SOURCES))

# Find assembly sources.
ASM_SOURCES = $(shell find . -name "*.asm" \
	-not -path "./$(BUILD_DIR)/*" \
	-not -path "./.git/*")

C_OBJECTS = $(patsubst ./%.c,$(BUILD_DIR)/c/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst ./%.asm,$(BUILD_DIR)/asm/%.o,$(ASM_SOURCES))

# kernel/entry.asm provides the Multiboot entry point.
ENTRY_ASM = kernel/entry.asm
ENTRY_OBJ = $(BUILD_DIR)/asm/kernel/entry.o

# The plain image does not include the Multiboot entry point.
PLAIN_ASM_OBJECTS = $(filter-out $(ENTRY_OBJ),$(ASM_OBJECTS))

LOGO_OBJ = $(BUILD_DIR)/logo_bmp.o

# Kernel objects; loadable module objects are built only by the modules target.
PLAIN_OBJECTS = $(PLAIN_ASM_OBJECTS) $(C_OBJECTS) $(LOGO_OBJ)

# Objects included in the bootable kernel.
BOOT_OBJECTS = $(ENTRY_OBJ) $(PLAIN_OBJECTS)

# Collect object directories for per-directory built-in.a archives.
C_OBJECT_DIRS = $(sort $(dir $(C_OBJECTS)))
ASM_OBJECT_DIRS = $(sort $(dir $(ASM_OBJECTS)))

C_BUILTIN_ARCHIVES = $(addsuffix built-in.a,$(C_OBJECT_DIRS))
ASM_BUILTIN_ARCHIVES = $(addsuffix built-in.a,$(ASM_OBJECT_DIRS))
BUILTIN_ARCHIVES = $(C_BUILTIN_ARCHIVES) $(ASM_BUILTIN_ARCHIVES)

# Automatic dependency files.
DEPS = $(C_OBJECTS:.o=.d) $(MOD_OBJS:.mo=.d)

.PHONY: all configured-build clean linux run-linux os.iso patch_modules modules \
	oconf-tools olddefconfig config-build version

# Generate .config first, then restart Make so source lists use it.
all:
	@$(MAKE) --no-print-directory olddefconfig
	@$(MAKE) --no-print-directory configured-build

version:
	@echo "$(VERSION)"

configured-build: $(VERSION_HEADER) $(BUILDENV_HEADER) \
	$(OPENKERNEL_A) $(PLAIN_KERNEL) $(KERNEL)

# Generate the kernel version header.
$(VERSION_HEADER):
	@mkdir -p $(VERSION_DIR)
	@echo "GEN $@"
	@printf '%s\n' \
		'#ifndef OPENKERNEL_VERSION_H' \
		'#define OPENKERNEL_VERSION_H' \
		'' \
		'#define OPENKERNEL_VERSION_MAJOR $(RELVERSION)' \
		'#define OPENKERNEL_VERSION_MINOR $(SUBVERSION)' \
		'#define OPENKERNEL_VERSION_PATCH $(PATCHVERSION)' \
		'#define OPENKERNEL_VERSION_EXTRA "$(EXTRAVERSION)"' \
		'#define OPENKERNEL_VERSION "$(VERSION)"' \
		'#define OPENKERNEL_PRETTYNAME "$(PRETTY_NAME)"' \
		'' \
		'#endif' > $@

$(BUILDENV_HEADER):
	@mkdir -p $(VERSION_DIR)
	@echo "GEN $@"
	@printf '%s\n' \
		'#ifndef OPENKERNEL_BUILDENV_H' \
		'#define OPENKERNEL_BUILDENV_H' \
		'' \
		'#define OPENKERNEL_BUILDENV "$(shell uname -a)"' \
		'' \
		'#endif' > $@

# Compile C sources.
$(BUILD_DIR)/c/%.o: %.c $(VERSION_HEADER) $(BUILDENV_HEADER)
	@mkdir -p $(dir $@)
	@echo "CC $@"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assemble NASM sources.
$(BUILD_DIR)/asm/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "AS $@"
	@$(AS) $(ASFLAGS) $< -o $@

# Compile loadable module objects.
$(MOD_DIR)/%.mo: modules/%.c $(VERSION_HEADER)
	@mkdir -p $(dir $@)
	@echo "CC $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(MODULE_INSTALL_DIR)/%.ko: $(MOD_DIR)/%.mo
	@mkdir -p $(dir $@)
	@echo "MODULE $@"
	@cp "$<" "$@"

modules: $(MODULE_IMAGES)

# Convert the bitmap logo into an ELF object.
$(LOGO_OBJ): kernel/video/logo.bmp
	@mkdir -p $(dir $@)
	@echo "OBJCOPY $@"
	@objcopy -I binary -O elf32-i386 -B i386 $< $@

# Generate a built-in.a archive for each C object directory.
define C_BUILTIN_RULE
$(1)built-in.a: $(filter $(1)%.o,$(C_OBJECTS))
	@rm -f "$$@"
	@echo "AR $$@"
	@$$(AR) rcsP "$$@" $$^
	@$$(RANLIB) "$$@"
endef

$(foreach dir,$(C_OBJECT_DIRS),$(eval $(call C_BUILTIN_RULE,$(dir))))

# Generate a built-in.a archive for each assembly object directory.
define ASM_BUILTIN_RULE
$(1)built-in.a: $(filter $(1)%.o,$(ASM_OBJECTS))
	@rm -f "$$@"
	@echo "AR $$@"
	@$$(AR) rcsP "$$@" $$^
	@$$(RANLIB) "$$@"
endef

$(foreach dir,$(ASM_OBJECT_DIRS),$(eval $(call ASM_BUILTIN_RULE,$(dir))))

# Combine all objects into openkernel.a.
$(OPENKERNEL_A): $(PLAIN_OBJECTS) $(BUILTIN_ARCHIVES)
	@mkdir -p $(dir $@)
	@rm -f "$@"
	@echo "AR $@"
	@$(AR) rcsP "$@" $(PLAIN_OBJECTS)
	@$(RANLIB) "$@"

# Build the plain kernel image.
$(PLAIN_KERNEL): $(OPENKERNEL_A) linker.ld
	@echo "LD $@"
	@$(CC) $(PLAIN_LDFLAGS) -o "$@" $(OPENKERNEL_A)
	@test -f "$@"

# Build the bootable Multiboot kernel image.
$(KERNEL): $(ENTRY_OBJ) $(OPENKERNEL_A) linker.ld
	@mkdir -p $(dir $@)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o "$@" $(ENTRY_OBJ) $(OPENKERNEL_A)
	@test -f "$@"
	@nm -n $(KERNEL) > Kernel.syms
	@if [ ! -f "$(BUILD_NUMBER_FILE)" ]; then echo 0 > "$(BUILD_NUMBER_FILE)"; fi
	@build=$$(cat "$(BUILD_NUMBER_FILE)"); \
	build=$$((build + 1)); \
	echo $$build > "$(BUILD_NUMBER_FILE)"; \
	echo "Kernel Image is ready! (build #$$build)"

# Build the host-side Linux program.
linux: all $(LINUX_PROGRAM)

$(LINUX_PROGRAM): $(HOST_SOURCES)
	@mkdir -p $(dir $@)
	@echo "HOST CC $@"
	@$(HOST_CC) $(HOST_CFLAGS) -o $@ $(HOST_SOURCES)

# Build the ISO after configuration has been generated and applied.
os.iso: $(shell find initramfs -type f) \
	boot/grub.cfg build_fs.sh copy_kernel.sh make_iso.sh
	@$(MAKE) --no-print-directory all
	@$(MAKE) --no-print-directory modules
	@mkdir -p iso/boot/grub
	@cp boot/grub.cfg iso/boot/grub/grub.cfg
	@./copy_kernel.sh
	@./build_fs.sh "$(VERSION)"
	@./make_iso.sh

run-linux:
	@$(MAKE) --no-print-directory linux
	@$(MAKE) --no-print-directory os.iso
	@./$(LINUX_PROGRAM) $(QEMU_ARGS)

# Build the oconf configuration tools.
$(OCONF_PARSER): $(OCONF_DIR)/config-parser.c
	@echo "HOST CC $<"
	@$(HOST_CC) $(OCONF_CFLAGS) -o $@ $<

$(OCONF_BUILDER): $(OCONF_DIR)/config-build.c
	@echo "HOST CC $<"
	@$(HOST_CC) $(OCONF_CFLAGS) -o $@ $<

oconf-tools: $(OCONF_PARSER) $(OCONF_BUILDER)

# Generate configuration defaults from all .Ocfg files.
# Preserve existing .config values and default new options to n.
olddefconfig: $(OCONF_PARSER)
	@set -eu; \
	tmp_defaults=$$(mktemp); \
	tmp_unique=$$(mktemp); \
	trap 'rm -f "$$tmp_defaults" "$$tmp_unique"' EXIT; \
	find . -type f -name '*.Ocfg' \
		-not -path './.git/*' \
		-not -path './$(BUILD_DIR)/*' \
		-exec awk '\
			function trim(s) { \
				sub(/^[ \t]+/, "", s); \
				sub(/[ \t\r]+$$/, "", s); \
				return s; \
			} \
			function emit() { \
				if (config != "") \
					print config "=" default_value; \
			} \
			FNR == 1 { \
				if (NR > 1) emit(); \
				config = ""; \
				default_value = "n"; \
			} \
			{ \
				line = trim($$0); \
				if (line == "" || line ~ /^#/) next; \
				eq = index(line, "="); \
				if (!eq) next; \
				key = trim(substr(line, 1, eq - 1)); \
				value = trim(substr(line, eq + 1)); \
				if (key == "CONFIG") { \
					emit(); \
					config = value; \
					default_value = "n"; \
				} else if (key == "DEFAULT" && config != "") { \
					if (value == "y" || value == "n") \
						default_value = value; \
					else { \
						printf "oconf: invalid DEFAULT=%s\n", value > "/dev/stderr"; \
						exit 1; \
					} \
				} \
			} \
			END { emit(); }' {} + > "$$tmp_defaults"; \
	awk -F= '!seen[$$1]++' "$$tmp_defaults" > "$$tmp_unique"; \
	if [ ! -s "$$tmp_unique" ]; then \
		echo "oconf: no CONFIG entries found in .Ocfg files"; \
		exit 1; \
	fi; \
	echo "OCONF: generating $(OCONF_CONFIG) from .Ocfg files"; \
	./$(OCONF_PARSER) --olddefconfig \
		"$(OCONF_CONFIG)" "$$tmp_unique"

# Discover .Ocfg files and compile their enabled source entries.
config-build: olddefconfig $(OCONF_BUILDER)
	@./$(OCONF_BUILDER) .

# Report module sources.
patch_modules:
	@mkdir -p $(MOD_DIR)
	@echo "Checking modules in modules/..."
	@if [ -z "$(MOD_SOURCES)" ]; then \
		echo "No modules found in modules/."; \
	else \
		echo "Found module sources: $(MOD_SOURCES)"; \
	fi

# Remove generated build products.
clean:
	rm -rf $(BUILD_DIR) Kernel.syms openkernel \
		$(VERSION_HEADER) $(BUILDENV_HEADER)

DSK ?=

install:
	@if [ -z "$(DSK)" ]; then \
		echo "Error: Target disk (DSK) is not set."; \
		echo "Usage: make install DSK=sdb"; \
		exit 1; \
	fi
	sudo dd if=build/kernel.bin of=/dev/$(DSK) conv=fsync status=progress

-include $(DEPS)
