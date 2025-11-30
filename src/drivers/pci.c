#include <drivers/pci.h>
#include <arch/x86/io.h>
#include <ui/console.h>
#include <string.h>

#define MAX_PCI_DEVICES 32

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

// Read from PCI configuration space
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC)
    );
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

// Write to PCI configuration space
void pci_write_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)(
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC)
    );
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

// Enumerate PCI devices
void pci_init(void) {
    console_write("PCI: Enumerating devices...\n");
    pci_device_count = 0;

    for (uint8_t bus = 0; bus < 1; bus++) {  // Only scan bus 0 for simplicity
        for (uint8_t device = 0; device < 32; device++) {
            uint32_t vendor_device = pci_read_config(bus, device, 0, PCI_VENDOR_ID);
            uint16_t vendor_id = vendor_device & 0xFFFF;
            uint16_t device_id = (vendor_device >> 16) & 0xFFFF;

            if (vendor_id == 0xFFFF) continue;  // No device

            if (pci_device_count >= MAX_PCI_DEVICES) {
                console_write("PCI: Max devices reached\n");
                return;
            }

            pci_device_t *dev = &pci_devices[pci_device_count++];
            dev->bus = bus;
            dev->device = device;
            dev->function = 0;
            dev->vendor_id = vendor_id;
            dev->device_id = device_id;

            uint32_t class_reg = pci_read_config(bus, device, 0, 0x08);
            dev->class_code = (class_reg >> 24) & 0xFF;
            dev->subclass = (class_reg >> 16) & 0xFF;

            dev->bar0 = pci_read_config(bus, device, 0, PCI_BAR0);

            uint32_t subsystem = pci_read_config(bus, device, 0, PCI_SUBSYSTEM_VID);
            dev->subsystem_id = (subsystem >> 16) & 0xFFFF;

            console_write("PCI: Found ");
            console_write_hex(vendor_id);
            console_write(":");
            console_write_hex(device_id);
            console_putc('\n');
        }
    }

    console_write("PCI: Found ");
    console_write_dec(pci_device_count);
    console_write(" device(s)\n");
}

// Find VirtIO device by device ID
pci_device_t* pci_find_virtio_device(uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == PCI_VENDOR_VIRTIO &&
            pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Find AHCI controller
pci_device_t* pci_find_ahci(void) {
    for (int i = 0; i < pci_device_count; i++) {
        // Class 01 (Mass Storage), Subclass 06 (SATA), ProgIF 01 (AHCI)
        // Note: We need to check ProgIF which isn't currently stored in pci_device_t
        // So we'll read it from config space again
        if (pci_devices[i].class_code == 0x01 && pci_devices[i].subclass == 0x06) {
            uint32_t class_reg = pci_read_config(pci_devices[i].bus, pci_devices[i].device, pci_devices[i].function, 0x08);
            uint8_t prog_if = (class_reg >> 8) & 0xFF;
            if (prog_if == 0x01) {
                return &pci_devices[i];
            }
        }
    }
    return NULL;
}

int pci_get_device_count(void) {
    return pci_device_count;
}

pci_device_t* pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count) {
        return NULL;
    }
    return &pci_devices[index];
}
