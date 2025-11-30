# Git Commit Documentation - AHCI/SATA Driver

## Branch Name
```
feature/ahci-sata
```

## Commit Message
```
feat: Add AHCI/SATA driver for storage support

- Implement AHCI HBA detection and initialization
- Add AHCI register definitions and structures
- Implement port memory rebasing and command engine start
- Add QEMU AHCI device configuration
- Support SATA disk detection
```

## Detailed Description

### Feature: AHCI/SATA Driver

This commit introduces a native AHCI (Advanced Host Controller Interface) driver to support SATA storage devices. This enables PenOS to interact with physical SATA disks and QEMU's emulated AHCI controller.

---

## Changes by Component

### 1. AHCI Definitions

**Files Created:**
- `include/drivers/ahci.h`

**Changes:**
- Defined HBA memory registers (`hba_mem_t`)
- Defined Port registers (`hba_port_t`)
- Defined FIS types and structures
- Defined Command List and Command Table structures

### 2. AHCI Driver Implementation

**Files Created:**
- `src/drivers/ahci.c`

**Implementation:**
- `ahci_init()`: Finds AHCI controller, maps ABAR, enables AHCI mode, and scans ports.
- `ahci_port_rebase()`: Allocates memory for Command List and FIS, and starts the command engine.
- `check_type()`: Identifies connected devices (SATA, ATAPI, etc.).

### 3. PCI Integration

**Files Modified:**
- `src/drivers/pci.c`
- `include/drivers/pci.h`

**Changes:**
- Added `pci_find_ahci()` to locate the AHCI controller (Class 01, Subclass 06, ProgIF 01).

### 4. Kernel Integration

**Files Modified:**
- `src/kernel.c`

**Changes:**
- Added `ahci_init()` call during kernel initialization.

### 5. QEMU Configuration

**Files Modified:**
- `Makefile`

**Changes:**
- Added `-device ahci,id=ahci`
- Added `-drive id=ahci-disk,file=disk.img,if=none,format=raw`
- Added `-device ide-hd,drive=ahci-disk,bus=ahci.0`
- Added `disk.img` generation target (128MB zero-filled).

---

## Fixes Implemented

### 1. ABAR Mapping Fix
- **Issue**: System halted with Page Fault when accessing ABAR.
- **Cause**: Driver attempted to access physical address `0xFEBB5000` directly.
- **Fix**: Mapped physical ABAR address to virtual address `0xE0000000` using `paging_map()`.

### 2. Heap Mapping Fix
- **Issue**: System halted with Page Fault during `memset`.
- **Cause**: Heap pages were mapped without `PAGE_PRESENT` flag.
- **Fix**: Updated `heap.c` to use `PAGE_RW | PAGE_PRESENT` in `paging_map()`.

### 3. DMA Address Fix
- **Issue**: Hardware requires physical addresses for DMA.
- **Cause**: Driver was passing virtual heap addresses to AHCI registers.
- **Fix**: Used `paging_virt_to_phys()` to convert addresses for `CLB`, `FB`, and `CTBA`.

---

## Testing Performed

### Build Test
```bash
make clean && make iso
```
**Expected**: Clean compilation.

### Manual Verification
- Boot PenOS in QEMU.
- Verify "AHCI: Initializing..." message.
- Verify "AHCI: ABAR mapped at 0xE0000000 (phys 0xFEBB5000)".
- Verify "AHCI: SATA drive found at port X".
- Verify "AHCI: Port X rebased".
- **Success**: No system halt/panic.

**Note**: To view previous output that scrolled off the screen, check your host terminal window. The console output is redirected to stdout via the VirtIO serial console.

---

## Limitations

- **Read/Write Not Implemented**: Currently only initialization and detection are implemented. `ahci_read` and `ahci_write` are stubs.
- **Polling Only**: Interrupts are not yet supported.
- **Single Controller**: Only supports the first found AHCI controller.
- **Memory Allocation**: Uses `kmalloc_aligned` which assumes contiguous physical memory (valid for current simple heap).

---

## Future Enhancements

1. Implement `ahci_read` and `ahci_write` using DMA.
2. Implement `IDENTIFY DEVICE` to get disk capacity and model.
3. Add interrupt support.
4. Integrate with a filesystem (e.g., FAT32).
