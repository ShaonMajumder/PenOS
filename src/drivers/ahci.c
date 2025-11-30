#include <drivers/ahci.h>
#include <drivers/pci.h>
#include <ui/console.h>
#include <mem/heap.h>
#include <mem/paging.h>
#include <string.h>

// Global HBA memory pointer
static hba_mem_t *abar = NULL;

// Port structures
static hba_port_t *ports[32];
static int port_count = 0;

// Helper to check if port is implemented
static int check_type(hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;

    if (det != AHCI_STS_DET_PRESENT) return 0;
    if (ipm != AHCI_STS_IPM_ACTIVE) return 0;

    switch (port->sig) {
        case 0xEB140101: return 2; // ATAPI
        case 0xC33C0101: return 3; // SEMB
        case 0x96690101: return 4; // PM
        default: return 1; // SATA
    }
}

// Stop port command engine
static void stop_cmd(hba_port_t *port) {
    port->cmd &= ~AHCI_CMD_ST;
    port->cmd &= ~AHCI_CMD_FRE;
    
    // Wait for FR and CR to clear
    while (1) {
        if (port->cmd & AHCI_CMD_FR) continue;
        if (port->cmd & AHCI_CMD_CR) continue;
        break;
    }
}

// Start port command engine
static void start_cmd(hba_port_t *port) {
    while (port->cmd & AHCI_CMD_CR);
    port->cmd |= AHCI_CMD_FRE;
    port->cmd |= AHCI_CMD_ST;
}

// Rebase port memory (allocate command lists and FIS)
static int ahci_port_rebase(hba_port_t *port, int portno) {
    stop_cmd(port);

    // Command list (1K aligned)
    // Note: In a real OS, we need physically contiguous memory
    // For now, we assume kmalloc returns contiguous memory in our simple heap
    uint32_t cmd_list_addr = (uint32_t)kmalloc_aligned(1024, 1024);
    port->clb = paging_virt_to_phys(cmd_list_addr);
    port->clbu = 0;
    memset((void*)cmd_list_addr, 0, 1024);

    // FIS (256 bytes aligned)
    uint32_t fis_addr = (uint32_t)kmalloc_aligned(256, 256);
    port->fb = paging_virt_to_phys(fis_addr);
    port->fbu = 0;
    memset((void*)fis_addr, 0, 256);

    // Command table (one per command slot, we support 32 slots)
    // Each table is 128 bytes + PRDT
    hba_cmd_header_t *cmd_header = (hba_cmd_header_t*)cmd_list_addr;
    for (int i = 0; i < 32; i++) {
        cmd_header[i].prdtl = 8; // 8 PRDT entries per command
        
        uint32_t cmd_table_addr = (uint32_t)kmalloc_aligned(256, 256);
        uint32_t addr = cmd_table_addr + (i << 8);
        cmd_header[i].ctba = paging_virt_to_phys(cmd_table_addr);
        cmd_header[i].ctbau = 0;
        memset((void*)cmd_table_addr, 0, 256);
    }

    start_cmd(port);
    
    console_write("AHCI: Port ");
    console_write_dec(portno);
    console_write(" rebased\n");
    
    return 0;
}

// Initialize AHCI controller
int ahci_init(void) {
    console_write("AHCI: Initializing...\n");

    pci_device_t *pci_dev = pci_find_ahci();
    if (!pci_dev) {
        console_write("AHCI: No controller found\n");
        return -1;
    }

    // Map ABAR (BAR5)
    // Note: In a real OS, we need to map this physical address to virtual
    // For now, assuming identity mapping or flat memory model
    abar = (hba_mem_t *)pci_dev->bar0; // Actually BAR5 for AHCI usually, but let's check
    // Wait, pci_dev structure only has bar0. We need to read BAR5.
    // Let's read BAR5 from config space
    // Let's read BAR5 from config space
    uint32_t bar5 = pci_read_config(pci_dev->bus, pci_dev->device, pci_dev->function, 0x24); // BAR5 is at 0x24
    uint32_t abar_phys = bar5 & 0xFFFFFFF0; // Mask out flag bits
    
    // Map ABAR to virtual memory
    // We'll use a fixed location in kernel space for now, or allocate a page
    // For simplicity, let's map it to 0xE0000000 (arbitrary kernel space address)
    uint32_t abar_virt = 0xE0000000;
    
    // ABAR is usually small (< 4KB), so one page is enough
    paging_map(abar_virt, abar_phys, PAGE_RW | PAGE_PRESENT);
    
    abar = (hba_mem_t *)abar_virt;

    console_write("AHCI: ABAR mapped at 0x");
    console_write_hex((uint32_t)abar);
    console_write(" (phys 0x");
    console_write_hex(abar_phys);
    console_write(")\n");

    // Enable AHCI mode
    abar->ghc |= AHCI_GHC_AE;
    
    // Scan ports
    uint32_t pi = abar->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & 1) {
            int type = check_type(&abar->ports[i]);
            if (type == 1) { // SATA
                console_write("AHCI: SATA drive found at port ");
                console_write_dec(i);
                console_write("\n");
                
                ports[port_count++] = &abar->ports[i];
                ahci_port_rebase(&abar->ports[i], i);
            }
        }
        pi >>= 1;
    }

    return 0;
}

// Read from SATA disk
int ahci_read(int port, uint64_t lba, uint16_t count, void *buffer) {
    // TODO: Implement read
    (void)port; (void)lba; (void)count; (void)buffer;
    return -1;
}

// Write to SATA disk
int ahci_write(int port, uint64_t lba, uint16_t count, const void *buffer) {
    // TODO: Implement write
    (void)port; (void)lba; (void)count; (void)buffer;
    return -1;
}
