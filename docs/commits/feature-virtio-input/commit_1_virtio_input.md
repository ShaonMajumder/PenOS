# Git Commit Documentation - Modern Input Support

## Branch Name
```
feature/virtio-input
```

## Commit Message
```
feat: Implement VirtIO-Input driver for modern keyboard support

- Add VirtIO-Input device ID and structs
- Implement VirtIO-Input driver with event polling
- Refactor keyboard driver to support external input injection
- Enable virtio-keyboard-pci in QEMU configuration
- Map Linux EV_KEY codes to ASCII for shell input
```

## Detailed Description

### Feature: Modern Paravirtualized Input

This commit introduces support for the **VirtIO Input** standard, allowing PenOS to receive input from modern virtualization interfaces (`virtio-keyboard-pci`) instead of relying solely on legacy PS/2 emulation. This is the standard way to handle input in cloud and VM environments (QEMU/KVM).

---

## Changes by Component

### 1. VirtIO Driver Enhancements

**Files Modified:**
- `include/drivers/virtio.h`
- `src/drivers/virtio_input.c` (NEW)

**Changes:**
- Added `VIRTIO_DEV_INPUT` (0x1052) device ID
- Defined `virtio_input_event` structure compatible with Linux Input Subsystem
- Implemented `virtio_input_init()` to find and initialize the device
- Implemented `virtio_input_poll()` to read events from the event queue
- Implemented translation from Linux `EV_KEY` codes to ASCII characters

**Architecture Impact**: Adds a new class of VirtIO devices (Input) alongside Block and 9P.

---

### 2. Keyboard Subsystem Refactoring

**Files Modified:**
- `src/drivers/keyboard.c`
- `include/drivers/keyboard.h`

**Changes:**
- Exposed `keyboard_push_char()` (previously static `buffer_put`)
- Allows external drivers (like VirtIO Input) to inject keystrokes into the shared input buffer
- Preserves existing PS/2 interrupt handler functionality (both can work simultaneously)

---

### 3. Kernel Integration

**Files Modified:**
- `src/kernel.c`
- `src/shell/shell.c`

**Changes:**
- Called `virtio_input_init()` during kernel startup
- Added `virtio_input_poll()` to the shell's idle loop
- Note: In a fully interrupt-driven system, polling wouldn't be needed, but for this initial implementation, we poll the virtqueue while waiting for input.

---

### 4. Build Configuration

**Files Modified:**
- `Makefile`

**Changes:**
- Added `-device virtio-keyboard-pci,disable-modern=on` to QEMU flags
- Ensures the VM presents a VirtIO keyboard device

---

## Testing Performed

### Manual Verification
- ✅ Boot PenOS in QEMU with new flags
- ✅ Type in shell: Input works
- ✅ Verify both PS/2 (legacy) and VirtIO paths work (QEMU often emulates both)
- ✅ Test special keys (Enter, Backspace)

---

## Architecture Update

### Previous Architecture
```
[PS/2 Hardware] -> [IRQ 1] -> [PS/2 Driver] -> [Input Buffer] -> [Shell]
```

### New Architecture
```
[PS/2 Hardware] -> [IRQ 1] -> [PS/2 Driver] --+
                                              |
                                              v
                                       [Input Buffer] -> [Shell]
                                              ^
                                              |
[VirtIO Kbd] -> [VirtQueue] -> [VirtIO Driver]
```

This hybrid approach ensures compatibility with both legacy hardware and modern VMs.
