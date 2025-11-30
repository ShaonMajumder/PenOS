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

**Supported Device Types:**
- ✅ SATA Hard Disk Drives (HDDs)
- ✅ SATA Solid State Drives (SSDs)
- ✅ M.2 SATA SSDs (M.2 drives using SATA protocol)
- ❌ M.2 NVMe SSDs (requires separate NVMe driver - not implemented)

The AHCI specification is interface-agnostic and works with any SATA-compatible storage device, regardless of the physical form factor or technology (spinning disk vs. solid state).

---

## Changes by Component
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

- **NVMe Not Supported**: M.2 NVMe SSDs require a separate NVMe driver (different protocol than AHCI/SATA)
- **No NCQ**: Native Command Queuing not implemented (single command at a time)
- **No Hot-plug**: Cannot detect drives added/removed while system is running
- **Single Controller**: Only supports the first found AHCI controller
- **No Partition Support**: Cannot parse MBR/GPT partition tables
- **No Filesystem Integration**: Cannot mount or access files (raw sector I/O only)

---

## Future Enhancements

1. **NVMe Driver**: Support for M.2 NVMe SSDs (PCIe-based, not SATA)
2. **Partition Parsing**: MBR/GPT support to access disk partitions
3. **FAT32 Driver**: Native filesystem for reading/writing files
4. **NCQ Support**: Native Command Queuing for better performance
5. **Hot-plug Detection**: Dynamic drive detection via AHCI interrupts
