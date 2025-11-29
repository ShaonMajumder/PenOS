ARCH=i386
CC ?= gcc
LD ?= ld
VERSION_STRING := $(strip $(shell cat VERSION))
CFLAGS := -m32 -ffreestanding -fno-stack-protector -fno-pic -fno-builtin-memset -fno-builtin-memcpy -fno-builtin-memmove -O0 -Wall -Wextra -Iinclude -D_FORTIFY_SOURCE=0 -DPENOS_VERSION=\"$(VERSION_STRING)\"
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib
BUILD := build
SRCS := $(shell find src -name "*.c")
ASMS := $(shell find src -name "*.S" -o -name "*.s")
ASM_OBJS := $(patsubst src/%, $(BUILD)/%, $(ASMS))
ASM_OBJS := $(ASM_OBJS:.S=.o)
ASM_OBJS := $(ASM_OBJS:.s=.o)
OBJS := $(patsubst src/%, $(BUILD)/%, $(SRCS:.c=.o)) \
         $(ASM_OBJS)

all: $(BUILD)/kernel.bin iso

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(BUILD)/kernel.bin grub/grub.cfg grub/themes/penos/theme.txt grub/themes/penos/penos-boot-splash.png
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/kernel.bin $(BUILD)/iso/boot/kernel.bin
	cp grub/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	if [ -d grub/themes ]; then cp -r grub/themes $(BUILD)/iso/boot/grub/; fi
	grub-mkrescue -o PenOS.iso $(BUILD)/iso

run: all
	qemu-system-i386 -cdrom PenOS.iso \
		-device virtio-9p-pci,fsdev=wsl,mount_tag=wsl \
		-fsdev local,id=wsl,path=/mnt/wslg/distro/root,security_model=none

clean:
	rm -rf $(BUILD) PenOS.iso

.PHONY: all iso run clean
