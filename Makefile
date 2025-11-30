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
	$(LD) $(LDFLAGS) -o $@ \
		$(BUILD)/apps/sysinfo.o \
		$(BUILD)/arch/x86/gdt.o \
		$(BUILD)/arch/x86/idt.o \
		$(BUILD)/arch/x86/interrupts.o \
		$(BUILD)/arch/x86/pic.o \
		$(BUILD)/arch/x86/timer.o \
		$(BUILD)/arch/x86/tss.o \
		$(BUILD)/drivers/keyboard.o \
		$(BUILD)/drivers/mouse.o \
		$(BUILD)/drivers/pci.o \
		$(BUILD)/drivers/speaker.o \
		$(BUILD)/drivers/virtio.o \
		$(BUILD)/drivers/virtio_console.o \
		$(BUILD)/drivers/virtio_input.o \
		$(BUILD)/drivers/ahci.o \
		$(BUILD)/fs/9p.o \
		$(BUILD)/fs/fs.o \
		$(BUILD)/fs/penfs.o \
		$(BUILD)/kernel.o \
		$(BUILD)/lib/string.o \
		$(BUILD)/lib/syscall.o \
		$(BUILD)/mem/heap.o \
		$(BUILD)/mem/paging.o \
		$(BUILD)/mem/pmm.o \
		$(BUILD)/sched/sched.o \
		$(BUILD)/shell/shell.o \
		$(BUILD)/sys/power.o \
		$(BUILD)/sys/syscall.o \
		$(BUILD)/ui/console.o \
		$(BUILD)/ui/framebuffer.o \
		$(BUILD)/arch/x86/gdt_flush.o \
		$(BUILD)/arch/x86/isr_stubs.o \
		$(BUILD)/boot.o

iso: $(BUILD)/kernel.bin grub/grub.cfg grub/themes/penos/theme.txt grub/themes/penos/penos-boot-splash.png
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/kernel.bin $(BUILD)/iso/boot/kernel.bin
	cp grub/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	if [ -d grub/themes ]; then cp -r grub/themes $(BUILD)/iso/boot/grub/; fi
	grub-mkrescue -o PenOS.iso $(BUILD)/iso

run: all disk.img
	qemu-system-i386 -cdrom PenOS.iso \
		-device virtio-9p-pci,disable-modern=on,fsdev=wsl,mount_tag=wsl \
		-fsdev local,id=wsl,path=/,security_model=none \
		-device virtio-keyboard-pci,disable-modern=on \
		-device virtio-mouse-pci,disable-modern=on \
		-device virtio-serial-pci,disable-modern=on \
		-chardev stdio,id=cons0,mux=on \
		-device virtconsole,chardev=cons0 \
		-drive id=ahci-disk,file=disk.img,if=none,format=raw \
		-device ahci,id=ahci \
		-device ide-hd,drive=ahci-disk,bus=ahci.0

run-windows: all disk.img
	@echo "Running PenOS in Windows QEMU with audio support..."
	@echo "Make sure QEMU for Windows is installed!"
	@echo "Download from: https://qemu.weilnetz.de/w64/"
	cmd.exe /c "C:\Program Files\qemu\qemu-system-i386.exe" \
		-cdrom PenOS.iso \
		-device virtio-9p-pci,disable-modern=on,fsdev=wsl,mount_tag=wsl \
		-fsdev local,id=wsl,path=\\\\wsl$$\\Ubuntu\\,security_model=none \
		-device virtio-keyboard-pci,disable-modern=on \
		-device virtio-mouse-pci,disable-modern=on \
		-device virtio-serial-pci,disable-modern=on \
		-chardev stdio,id=cons0,mux=on \
		-device virtconsole,chardev=cons0 \
		-drive id=ahci-disk,file=disk.img,if=none,format=raw \
		-device ahci,id=ahci \
		-device ide-hd,drive=ahci-disk,bus=ahci.0 \
		-audiodev dsound,id=snd0 \
		-machine pcspk-audiodev=snd0

disk.img:
	dd if=/dev/zero of=disk.img bs=1M count=128

clean:
	rm -rf $(BUILD) PenOS.iso

.PHONY: all iso run run-windows clean
