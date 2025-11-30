#include <drivers/ahci.h>
#include <drivers/pci.h>
#include <drivers/block.h>
#include <ui/console.h>
#include <mem/heap.h>
#include <mem/paging.h>
#include <string.h>
#include <arch/x86/interrupts.h>

// Global HBA memory pointer
static hba_mem_t *abar = NULL;

// Port structures
static hba_port_t *ports[32];
static int port_count = 0;

// Virtual addresses for port structures (needed by driver)
static struct {
    uint32_t clb;
    uint32_t fb;
    uint32_t ctba[32];
} port_virt[32];

// Interrupt Handler
static void ahci_handler(interrupt_frame_t *frame) {
    (void)frame;
    
    if (!abar) return;
    
    // Check global IS
    uint32_t is = abar->is;
    if (is == 0) return;
    
    // Acknowledge global IS
    abar->is = is;
    
    // Check ports
    for (int i = 0; i < 32; i++) {
        if (is & (1 << i)) {
            hba_port_t *port = &abar->ports[i];
            uint32_t pis = port->is;
            
            // Clear port interrupts
            port->is = pis;
            
            // In a real driver, we'd wake up waiting threads here
        }
    }
}

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
    uint32_t cmd_list_addr = (uint32_t)kmalloc_aligned(1024, 1024);
    port_virt[portno].clb = cmd_list_addr;
    port->clb = paging_virt_to_phys(cmd_list_addr);
    port->clbu = 0;
    memset((void*)cmd_list_addr, 0, 1024);

    // FIS (256 bytes aligned)
    uint32_t fis_addr = (uint32_t)kmalloc_aligned(256, 256);
    port_virt[portno].fb = fis_addr;
    port->fb = paging_virt_to_phys(fis_addr);
    port->fbu = 0;
    memset((void*)fis_addr, 0, 256);

    // Command table (one per command slot, we support 32 slots)
    hba_cmd_header_t *cmd_header = (hba_cmd_header_t*)cmd_list_addr;
    for (int i = 0; i < 32; i++) {
        cmd_header[i].prdtl = 8; // 8 PRDT entries per command
        
        uint32_t cmd_table_addr = (uint32_t)kmalloc_aligned(256, 256);
        port_virt[portno].ctba[i] = cmd_table_addr;
        
        cmd_header[i].ctba = paging_virt_to_phys(cmd_table_addr);
        cmd_header[i].ctbau = 0;
        memset((void*)cmd_table_addr, 0, 256);
    }
    
    // Enable Port Interrupts
    port->ie = 0xFFFFFFFF;
    
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
    uint32_t bar5 = pci_read_config(pci_dev->bus, pci_dev->device, pci_dev->function, 0x24);
    uint32_t abar_phys = bar5 & 0xFFFFFFF0;
    
    // Map ABAR to virtual memory
    uint32_t abar_virt = 0xE0000000;
    paging_map(abar_virt, abar_phys, PAGE_RW | PAGE_PRESENT);
    
    abar = (hba_mem_t *)abar_virt;

    console_write("AHCI: ABAR mapped at 0x");
    console_write_hex((uint32_t)abar);
    console_write(" (phys 0x");
    console_write_hex(abar_phys);
    console_write(")\n");

    // Enable AHCI mode
    abar->ghc |= AHCI_GHC_AE;
    
    // Enable Interrupts
    uint32_t irq_line = pci_read_config(pci_dev->bus, pci_dev->device, pci_dev->function, 0x3C) & 0xFF;
    console_write("AHCI: IRQ ");
    console_write_dec(irq_line);
    console_write("\n");
    
    register_interrupt_handler(32 + irq_line, ahci_handler);
    abar->ghc |= AHCI_GHC_IE;
    
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

// Find a free command list slot (Phase 5: Command Slot Management)
static int find_cmdslot(hba_port_t *port) {
    // Check both SACT (active NCQ commands) and CI (issued commands)
    // A slot is free if neither bit is set
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < 32; i++) {
        if ((slots & 1) == 0) {
            return i;
        }
        slots >>= 1;
    }
    console_write("AHCI: Cannot find free command list slot\n");
    return -1;
}

// Identify SATA device
int ahci_identify(int port, uint16_t *buffer) {
    hba_port_t *hba_port = ports[port];
    
    hba_port->is = (uint32_t)-1;
    
    int slot = find_cmdslot(hba_port);
    if (slot == -1) return -1;
    
    hba_cmd_header_t *cmd_header = (hba_cmd_header_t*)port_virt[port].clb;
    cmd_header += slot;
    
    cmd_header->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_header->w = 0;
    cmd_header->prdtl = 1;
    
    hba_cmd_table_t *cmd_table = (hba_cmd_table_t*)port_virt[port].ctba[slot];
    memset(cmd_table, 0, sizeof(hba_cmd_table_t) + (cmd_header->prdtl - 1) * sizeof(hba_prdt_entry_t));
    
    cmd_table->prdt_entry[0].dba = paging_virt_to_phys((uint32_t)buffer);
    cmd_table->prdt_entry[0].dbc = 511;
    cmd_table->prdt_entry[0].i = 1;
    
    fis_reg_h2d_t *fis = (fis_reg_h2d_t*)(&cmd_table->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = ATA_CMD_IDENTIFY;
    
    int spin = 0;
    while ((hba_port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) {
        console_write("AHCI: Port hung\n");
        return -1;
    }
    
    hba_port->ci = 1 << slot;
    
    while (1) {
        if ((hba_port->ci & (1 << slot)) == 0) break;
        if (hba_port->is & AHCI_PxIS_TFES) {
            console_write("AHCI: Identify disk error\n");
            return -1;
        }
    }
    
    return 0;
}

// Read from SATA disk
int ahci_read(int port, uint64_t lba, uint16_t count, void *buffer) {
    hba_port_t *hba_port = ports[port];
    hba_port->is = (uint32_t)-1;
    
    int slot = find_cmdslot(hba_port);
    if (slot == -1) return -1;
    
    hba_cmd_header_t *cmd_header = (hba_cmd_header_t*)port_virt[port].clb;
    cmd_header += slot;
    
    cmd_header->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_header->w = 0;
    cmd_header->prdtl = 1;
    
    hba_cmd_table_t *cmd_table = (hba_cmd_table_t*)port_virt[port].ctba[slot];
    memset(cmd_table, 0, sizeof(hba_cmd_table_t) + (cmd_header->prdtl - 1) * sizeof(hba_prdt_entry_t));
    
    cmd_table->prdt_entry[0].dba = paging_virt_to_phys((uint32_t)buffer);
    cmd_table->prdt_entry[0].dbc = (count * 512) - 1;
    cmd_table->prdt_entry[0].i = 1;
    
    fis_reg_h2d_t *fis = (fis_reg_h2d_t*)(&cmd_table->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = ATA_CMD_READ_DMA_EXT;
    
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6;
    
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    
    fis->count = count;
    
    int spin = 0;
    while ((hba_port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) {
        console_write("AHCI: Port hung\n");
        return -1;
    }
    
    hba_port->ci = 1 << slot;
    
    while (1) {
        if ((hba_port->ci & (1 << slot)) == 0) break;
        if (hba_port->is & AHCI_PxIS_TFES) {
            console_write("AHCI: Read error\n");
            return -1;
        }
    }
    
    return 0;
}

// Write to SATA disk
int ahci_write(int port, uint64_t lba, uint16_t count, const void *buffer) {
    hba_port_t *hba_port = ports[port];
    hba_port->is = (uint32_t)-1;
    
    int slot = find_cmdslot(hba_port);
    if (slot == -1) return -1;
    
    hba_cmd_header_t *cmd_header = (hba_cmd_header_t*)port_virt[port].clb;
    cmd_header += slot;
    
    cmd_header->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_header->w = 1;
    cmd_header->prdtl = 1;
    
    hba_cmd_table_t *cmd_table = (hba_cmd_table_t*)port_virt[port].ctba[slot];
    memset(cmd_table, 0, sizeof(hba_cmd_table_t) + (cmd_header->prdtl - 1) * sizeof(hba_prdt_entry_t));
    
    cmd_table->prdt_entry[0].dba = paging_virt_to_phys((uint32_t)buffer);
    cmd_table->prdt_entry[0].dbc = (count * 512) - 1;
    cmd_table->prdt_entry[0].i = 1;
    
    fis_reg_h2d_t *fis = (fis_reg_h2d_t*)(&cmd_table->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = ATA_CMD_WRITE_DMA_EXT;
    
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6;
    
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    
    fis->count = count;
    
    int spin = 0;
    while ((hba_port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) {
        console_write("AHCI: Port hung\n");
        return -1;
    }
    
    hba_port->ci = 1 << slot;
    
    while (1) {
        if ((hba_port->ci & (1 << slot)) == 0) break;
        if (hba_port->is & AHCI_PxIS_TFES) {
            console_write("AHCI: Write error\n");
            return -1;
        }
    }
    
    return 0;
}

// Block Device Interface Wrappers (Phase 7: Integration)
static int ahci_block_read(block_device_t *dev, uint64_t sector, uint32_t count, void *buffer) {
    int port = (int)dev->driver_data;
    return ahci_read(port, sector, (uint16_t)count, buffer);
}

static int ahci_block_write(block_device_t *dev, uint64_t sector, uint32_t count, const void *buffer) {
    int port = (int)dev->driver_data;
    return ahci_write(port, sector, (uint16_t)count, buffer);
}

// Get Block Device
int ahci_get_block_device(int port, block_device_t *dev) {
    if (port >= port_count) return -1;
    
    uint16_t *id_buf = (uint16_t*)kmalloc(512);
    if (ahci_identify(port, id_buf) != 0) {
        kfree(id_buf);
        return -1;
    }
    
    uint64_t sectors = *(uint64_t*)&id_buf[100];
    kfree(id_buf);
    
    strcpy(dev->name, "sata0");
    dev->sector_count = sectors;
    dev->sector_size = 512;
    dev->read = ahci_block_read;
    dev->write = ahci_block_write;
    dev->driver_data = (void*)port;
    
    return 0;
}
