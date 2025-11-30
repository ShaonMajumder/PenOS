@echo off
REM PenOS - Run in Windows QEMU with Audio Support
REM This script runs PenOS directly in Windows with working PC speaker audio

echo ========================================
echo PenOS - Windows QEMU Launcher
echo ========================================
echo.

REM Check if PenOS.iso exists
if not exist "PenOS.iso" (
    echo ERROR: PenOS.iso not found!
    echo Please build PenOS first from WSL:
    echo   wsl make clean ^&^& make iso
    echo.
    pause
    exit /b 1
)

REM Check if disk.img exists, create if needed
if not exist "disk.img" (
    echo Creating disk.img...
    wsl dd if=/dev/zero of=disk.img bs=1M count=128
)

REM Try to find QEMU installation
set QEMU_EXE=
if exist "C:\Program Files\qemu\qemu-system-i386.exe" (
    set QEMU_EXE=C:\Program Files\qemu\qemu-system-i386.exe
    echo Found QEMU at: C:\Program Files\qemu
) else if exist "C:\qemu\qemu-system-i386.exe" (
    set QEMU_EXE=C:\qemu\qemu-system-i386.exe
    echo Found QEMU at: C:\qemu
) else if exist "%ProgramFiles%\qemu\qemu-system-i386.exe" (
    set QEMU_EXE=%ProgramFiles%\qemu\qemu-system-i386.exe
    echo Found QEMU at: %ProgramFiles%\qemu
) else (
    echo ERROR: QEMU for Windows not found!
    echo.
    echo Searched in:
    echo   - C:\Program Files\qemu
    echo   - C:\qemu
    echo   - %ProgramFiles%\qemu
    echo.
    echo Please download and install QEMU for Windows from:
    echo https://qemu.weilnetz.de/w64/
    echo.
    echo Or edit this script to set the correct path.
    echo.
    pause
    exit /b 1
)

echo Starting PenOS with audio support...
echo You should hear startup beeps! 🔊
echo.
echo Note: VirtIO-9p filesystem disabled (not supported in Windows QEMU)
echo.

"%QEMU_EXE%" ^
    -cdrom PenOS.iso ^
    -device virtio-keyboard-pci,disable-modern=on ^
    -device virtio-mouse-pci,disable-modern=on ^
    -device virtio-serial-pci,disable-modern=on ^
    -chardev stdio,id=cons0,mux=on ^
    -device virtconsole,chardev=cons0 ^
    -drive id=ahci-disk,file=disk.img,if=none,format=raw ^
    -device ahci,id=ahci ^
    -device ide-hd,drive=ahci-disk,bus=ahci.0 ^
    -audiodev dsound,id=snd0 ^
    -machine pcspk-audiodev=snd0

echo.
echo PenOS exited.
pause
