# Feature: AHCI Hot-Plug Support

## Commit Details
**Title:** drivers: Implement AHCI hot-plug detection and shell commands
**Date:** 2025-11-30
**Author:** Antigravity (Assistant)

## Description
This commit adds hot-plug support to the AHCI driver, allowing PenOS to detect SATA drives being added or removed while the system is running. It also introduces new shell commands to monitor and manage SATA ports.

## Key Changes

### 1. AHCI Driver (`src/drivers/ahci.c`, `include/drivers/ahci.h`)
- **Interrupt Handling**: Added support for `AHCI_PORT_IS_PCS` (Port Connect Status) and `AHCI_PORT_IS_PRCS` (PhyRdy Change Status) interrupts.
- **Port Status Tracking**: Implemented `port_status` and `port_initialized` arrays to track the state of all 32 ports.
- **Dynamic Initialization**: Refactored port initialization logic into `ahci_port_rebase` to support initializing drives after boot.
- **Hot-Plug Logic**:
  - **Insertion**: Detects connection, rebases the port, allocates memory, and marks as connected.
  - **Removal**: Detects disconnection, stops the command engine, and marks as disconnected.
- **Scanning**: Added `ahci_scan_ports()` for manual rescanning.

### 2. Shell Integration (`src/shell/shell.c`)
- **`satastatus`**: Displays the status of all connected SATA ports, including:
  - Connection state (CONNECTED/Not Connected)
  - Drive Model (via IDENTIFY command)
  - Drive Size (in MB)
- **`satarescan`**: Manually triggers a full port scan.

## Testing
- **Verification**: Verified using QEMU Monitor (`drive_add`, `device_add`, `device_del`).
- **Walkthrough**: See `docs/walkthroughs/hotplug_walkthrough.md` (if created) or the artifacts for detailed steps.

## Limitations
- **No Automount**: Filesystems on hot-plugged drives are not automatically mounted.
- **Boot Drive**: Removing the boot drive will cause system instability.
- **Port Multipliers**: Not supported.

## Future Work
- Implement automatic filesystem mounting upon detection.
- Add support for safely removing drives (spin-down).
