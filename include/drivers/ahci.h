#ifndef DRIVERS_AHCI_H
#define DRIVERS_AHCI_H

#include <stdint.h>
#include <drivers/pci.h>

// Forward declaration
typedef struct block_device block_device_t;

// AHCI PCI Class/Subclass
#define PCI_CLASS_STORAGE    0x01
#define PCI_SUBCLASS_SATA    0x06
#define PCI_PROG_IF_AHCI     0x01

// Generic Host Control
#define AHCI_GHC_AE          (1 << 31)  // AHCI Enable
#define AHCI_GHC_MRSM        (1 << 2)   // MSI Revert to Single Message
#define AHCI_GHC_IE          (1 << 1)   // Interrupt Enable
#define AHCI_GHC_HR          (1 << 0)   // HBA Reset

// Port Command Register
#define AHCI_CMD_ST          (1 << 0)   // Start
#define AHCI_CMD_SUD         (1 << 1)   // Spin-Up Device
#define AHCI_CMD_POD         (1 << 2)   // Power On Device
#define AHCI_CMD_CLO         (1 << 3)   // Command List Override
#define AHCI_CMD_FRE         (1 << 4)   // FIS Receive Enable
#define AHCI_CMD_FR          (1 << 14)  // FIS Receive Running
#define AHCI_CMD_CR          (1 << 15)  // Command List Running

// Port Status Register
#define AHCI_STS_DET_PRESENT 0x3
#define AHCI_STS_IPM_ACTIVE  0x1

// FIS Types
typedef enum {
    FIS_TYPE_REG_H2D   = 0x27,  // Host to Device
    FIS_TYPE_REG_D2H   = 0x34,  // Device to Host
    FIS_TYPE_DMA_ACT   = 0x39,  // DMA Activate
    FIS_TYPE_DMA_SETUP = 0x41,  // DMA Setup
    FIS_TYPE_DATA      = 0x46,  // Data
    FIS_TYPE_BIST      = 0x58,  // BIST Activate
    FIS_TYPE_PIO_SETUP = 0x5F,  // PIO Setup
    FIS_TYPE_DEV_BITS  = 0xA1   // Set Device Bits
} FIS_TYPE;

// ATA Commands
#define ATA_CMD_READ_DMA_EXT     0x25
#define ATA_CMD_WRITE_DMA_EXT    0x35
#define ATA_CMD_IDENTIFY         0xEC

// ATA Status
#define ATA_SR_BSY               0x80
#define ATA_SR_DRQ               0x08
#define ATA_SR_ERR               0x01

// Port Interrupt Status Bit
#define AHCI_PORT_IS_TFES        (1 << 30) // Task File Error Status

// Capability flags
#define AHCI_CAP_S64A       (1 << 31)  // Supports 64-bit Addressing
#define AHCI_CAP_SNCQ       (1 << 30)  // Supports Native Command Queuing
#define AHCI_CAP_SSNTF      (1 << 29)  // Supports SNotification Register
#define AHCI_CAP_SMPS       (1 << 28)  // Supports Mechanical Presence Switch
#define AHCI_CAP_SSS        (1 << 27)  // Supports Staggered Spin-up
#define AHCI_CAP_SALP       (1 << 26)  // Supports Aggressive Link Power Management
#define AHCI_CAP_SAL        (1 << 25)  // Supports Activity LED
#define AHCI_CAP_SCLO       (1 << 24)  // Supports Command List Override
#define AHCI_CAP_SXS        (1 << 5)   // Supports External SATA
#define AHCI_CAP_SAM        (1 << 18)  // Supports AHCI mode only

// Global HBA Control flags
#define AHCI_GHC_AE         (1 << 31)  // AHCI Enable
#define AHCI_GHC_IE         (1 << 1)   // Interrupt Enable
#define AHCI_GHC_HR         (1 << 0)   // HBA Reset

// Port Command flags
#define AHCI_CMD_ST         (1 << 0)   // Start
#define AHCI_CMD_SUD        (1 << 1)   // Spin-Up Device
#define AHCI_CMD_POD        (1 << 2)   // Power On Device
#define AHCI_CMD_FRE        (1 << 4)   // FIS Receive Enable
#define AHCI_CMD_FR         (1 << 14)  // FIS Receive Running
#define AHCI_CMD_CR         (1 << 15)  // Command List Running

// Port Interrupt Status/Enable flags
#define AHCI_PORT_IS_DHRS   (1 << 0)   // Device to Host Register FIS Interrupt
#define AHCI_PORT_IS_PSS    (1 << 1)   // PIO Setup FIS Interrupt
#define AHCI_PORT_IS_DSS    (1 << 2)   // DMA Setup FIS Interrupt
#define AHCI_PORT_IS_SDBS   (1 << 3)   // Set Device Bits Interrupt
#define AHCI_PORT_IS_UFS    (1 << 4)   // Unknown FIS Interrupt
#define AHCI_PORT_IS_DPS    (1 << 5)   // Descriptor Processed
#define AHCI_PORT_IS_PCS    (1 << 6)   // Port Connect Change Status
#define AHCI_PORT_IS_DMPS   (1 << 7)   // Device Mechanical Presence Status
#define AHCI_PORT_IS_PRCS   (1 << 22)  // PhyRdy Change Status
#define AHCI_PORT_IS_IPMS   (1 << 23)  // Incorrect Port Multiplier Status
#define AHCI_PORT_IS_OFS    (1 << 24)  // Overflow Status
#define AHCI_PORT_IS_INFS   (1 << 26)  // Interface Non-fatal Error Status
#define AHCI_PORT_IS_IFS    (1 << 27)  // Interface Fatal Error Status
#define AHCI_PORT_IS_HBDS   (1 << 28)  // Host Bus Data Error Status
#define AHCI_PORT_IS_HBFS   (1 << 29)  // Host Bus Fatal Error Status
#define AHCI_PORT_IS_TFES   (1 << 30)  // Task File Error Status

// Port Interrupt Enable bits (same as IS bits)
#define AHCI_PORT_IE_DHRE   AHCI_PORT_IS_DHRS
#define AHCI_PORT_IE_PSE    AHCI_PORT_IS_PSS
#define AHCI_PORT_IE_DSE    AHCI_PORT_IS_DSS
#define AHCI_PORT_IE_SDBE   AHCI_PORT_IS_SDBS
#define AHCI_PORT_IE_UFE    AHCI_PORT_IS_UFS
#define AHCI_PORT_IE_DPE    AHCI_PORT_IS_DPS
#define AHCI_PORT_IE_PCE    AHCI_PORT_IS_PCS   // Port Connect Change Enable
#define AHCI_PORT_IE_DMPE   AHCI_PORT_IS_DMPS
#define AHCI_PORT_IE_PRCE   AHCI_PORT_IS_PRCS  // PhyRdy Change Enable
#define AHCI_PORT_IE_IPME   AHCI_PORT_IS_IPMS
#define AHCI_PORT_IE_OFE    AHCI_PORT_IS_OFS
#define AHCI_PORT_IE_INFE   AHCI_PORT_IS_INFS
#define AHCI_PORT_IE_IFE    AHCI_PORT_IS_IFS
#define AHCI_PORT_IE_HBDE   AHCI_PORT_IS_HBDS
#define AHCI_PORT_IE_HBFE   AHCI_PORT_IS_HBFS
#define AHCI_PORT_IE_TFEE   AHCI_PORT_IS_TFES

// HBA Port Registers
typedef struct {
    uint32_t clb;        // Command list base
    uint32_t clbu;       // Command list base upper
    uint32_t fb;         // FIS base
    uint32_t fbu;        // FIS base upper
    uint32_t is;         // Interrupt status
    uint32_t ie;         // Interrupt enable
    uint32_t cmd;        // Command and status
    uint32_t rsv0;       // Reserved
    uint32_t tfd;        // Task file data
    uint32_t sig;        // Signature
    uint32_t ssts;       // SATA status
    uint32_t sctl;       // SATA control
    uint32_t serr;       // SATA error
    uint32_t sact;       // SATA active
    uint32_t ci;         // Command issue
    uint32_t sntf;       // SATA notification
    uint32_t fbs;        // FIS-based switch control
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed)) hba_port_t;

// HBA Memory Registers
typedef struct {
    uint32_t cap;        // Host capabilities
    uint32_t ghc;        // Global host control
    uint32_t is;         // Interrupt status
    uint32_t pi;         // Ports implemented
    uint32_t vs;         // Version
    uint32_t ccc_ctl;    // Command completion coalescing control
    uint32_t ccc_pts;    // Command completion coalescing ports
    uint32_t em_loc;     // Enclosure management location
    uint32_t em_ctl;     // Enclosure management control
    uint32_t cap2;       // Host capabilities extended
    uint32_t bohc;       // BIOS/OS handoff control
    uint8_t  rsv[0xA0-0x2C];
    uint8_t  vendor[0x100-0xA0];
    hba_port_t ports[1]; // 1-32 ports
} __attribute__((packed)) hba_mem_t;

// Command Header
typedef struct {
    uint8_t  cfl:5;      // Command FIS length (in DWORDs)
    uint8_t  a:1;        // ATAPI
    uint8_t  w:1;        // Write (1=H2D, 0=D2H)
    uint8_t  p:1;        // Prefetchable
    uint8_t  r:1;        // Reset
    uint8_t  b:1;        // BIST
    uint8_t  c:1;        // Clear busy
    uint8_t  rsv0:1;
    uint8_t  pmp:4;      // Port multiplier port
    uint16_t prdtl;      // PRDT length
    uint32_t prdbc;      // PRD byte count
    uint32_t ctba;       // Command table base (low)
    uint32_t ctbau;      // Command table base (high)
    uint32_t rsv1[4];
} __attribute__((packed)) hba_cmd_header_t;

// PRDT Entry
typedef struct {
    uint32_t dba;        // Data base address
    uint32_t dbau;       // Data base address upper
    uint32_t rsv0;
    uint32_t dbc:22;     // Byte count (0-based, max 4MB)
    uint32_t rsv1:9;
    uint32_t i:1;        // Interrupt on completion
} __attribute__((packed)) hba_prdt_entry_t;

// Command Table
typedef struct {
    uint8_t  cfis[64];   // Command FIS
    uint8_t  acmd[16];   // ATAPI command
    uint8_t  rsv[48];
    hba_prdt_entry_t prdt_entry[1];  // Variable length
} __attribute__((packed)) hba_cmd_table_t;

// Register FIS - Host to Device
typedef struct {
    uint8_t  fis_type;   // 0x27
    uint8_t  pmport:4;
    uint8_t  rsv0:3;
    uint8_t  c:1;        // 1=Command, 0=Control
    uint8_t  command;    // ATA command
    uint8_t  featurel;
    uint8_t  lba0;       // LBA bits 0-7
    uint8_t  lba1;       // LBA bits 8-15
    uint8_t  lba2;       // LBA bits 16-23
    uint8_t  device;
    uint8_t  lba3;       // LBA bits 24-31
    uint8_t  lba4;       // LBA bits 32-39
    uint8_t  lba5;       // LBA bits 40-47
    uint8_t  featureh;
    uint16_t count;      // Sector count
    uint8_t  icc;
    uint8_t  control;
    uint32_t rsv1;
} __attribute__((packed)) fis_reg_h2d_t;

// Function Prototypes
int ahci_init(void);
int ahci_identify(int port, uint16_t *buffer);
int ahci_read(int port, uint64_t lba, uint16_t count, void *buffer);
int ahci_write(int port, uint64_t lba, uint16_t count, const void *buffer);
int ahci_get_block_device(int port, block_device_t *dev);
void ahci_scan_ports(void);
int ahci_port_is_connected(int port_num);

#endif
