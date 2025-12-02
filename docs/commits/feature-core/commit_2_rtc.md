# Commit 6: Real-Time Clock (RTC)

## Overview
This commit implements a driver for the Real-Time Clock (RTC) chip (CMOS), allowing PenOS to read the current wall-clock date and time.

## Changes
- **`include/arch/x86/rtc.h`**: Defined the RTC interface and `rtc_time_t` structure.
- **`src/arch/x86/rtc.c`**: Implemented the RTC driver.
  - Reads from CMOS ports `0x70` and `0x71`.
  - Handles "Update In Progress" flag to ensure consistent reads.
  - Converts BCD (Binary Coded Decimal) to binary.
  - Handles 12-hour to 24-hour conversion.
- **`src/kernel.c`**: Initialized the RTC driver at boot.
- **`src/shell/shell.c`**: Added a `date` command to display the current time.
  - Supports manual timezone setting (e.g., `date +6`).
  - Handles hour rollover (0-23).
- **`Makefile`**: Added `rtc.o` to the build.
- **Version Bump**: Updated PenOS version to **0.11.0**.

## Usage
- In the shell, type `date` to see the current UTC date and time.

## Notes
- The driver assumes the century is 20xx.
- Time is read in UTC (as provided by QEMU/Hardware). Timezone handling is not yet implemented.
