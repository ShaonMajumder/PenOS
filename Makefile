ARCH=i386
CC ?= gcc
LD ?= ld
CFLAGS := -m32 -ffreestanding -fno-stack-protector -fno-pic -O2 -Wall -Wextra -Iinclude
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib
BUILD := build
SRCS := $(shell find src -name "*.c")
ASMS := $(shell find src -name "*.S" -o -name "*.s")
OBJS := $(patsubst src/%, $(BUILD)/%, $(SRCS:.c=.o)) \
         $(patsubst src/%, $(BUILD)/%, $(ASMS:.S=.o))

all: $(BUILD)/kernel.bin iso

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(BUILD)/kernel.bin grub/grub.cfg
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/kernel.bin $(BUILD)/iso/boot/kernel.bin
	cp grub/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o PenOS.iso $(BUILD)/iso

run: all
	qemu-system-i386 -cdrom PenOS.iso

clean:
	rm -rf $(BUILD) PenOS.iso

.PHONY: all iso run clean
