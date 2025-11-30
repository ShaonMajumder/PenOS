#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include <stdint.h>

// PCI Configuration Space Addresses
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// PCI Configuration Registers
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_CLASS_CODE      0x0B
#define PCI_SUBCLASS        0x0A
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_SUBSYSTEM_VID   0x2C
#define PCI_SUBSYSTEM_ID    0x2E

// VirtIO Vendor ID
#define PCI_VENDOR_VIRTIO   0x1AF4

// PCI Device Structure
typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsystem_id;
    uint8_t class_code;
    uint8_t subclass;
    uint32_t bar0;
} pci_device_t;

// Functions
void pci_init(void);
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value);
pci_device_t* pci_find_virtio_device(uint16_t device_id);
pci_device_t* pci_find_ahci(void);
int pci_get_device_count(void);
pci_device_t* pci_get_device(int index);

#endif
