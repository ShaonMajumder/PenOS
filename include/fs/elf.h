#ifndef FS_ELF_H
#define FS_ELF_H

#include <stdint.h>

// ELF Magic Number
#define ELF_MAGIC 0x464C457F  // "\x7FELF"

// ELF Class
#define ELFCLASS32 1
#define ELFCLASS64 2

// ELF Data Encoding
#define ELFDATA2LSB 1  // Little-endian
#define ELFDATA2MSB 2  // Big-endian

// ELF Type
#define ET_NONE   0  // No file type
#define ET_REL    1  // Relocatable
#define ET_EXEC   2  // Executable
#define ET_DYN    3  // Shared object
#define ET_CORE   4  // Core file

// ELF Machine
#define EM_386    3  // Intel 80386

// Program Header Types
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7

// Program Header Flags
#define PF_X 0x1  // Execute
#define PF_W 0x2  // Write
#define PF_R 0x4  // Read

// ELF Header (32-bit)
typedef struct {
    uint8_t  e_ident[16];     // Magic number and other info
    uint16_t e_type;          // Object file type
    uint16_t e_machine;       // Architecture
    uint32_t e_version;       // Object file version
    uint32_t e_entry;         // Entry point virtual address
    uint32_t e_phoff;         // Program header table file offset
    uint32_t e_shoff;         // Section header table file offset
    uint32_t e_flags;         // Processor-specific flags
    uint16_t e_ehsize;        // ELF header size in bytes
    uint16_t e_phentsize;     // Program header table entry size
    uint16_t e_phnum;         // Program header table entry count
    uint16_t e_shentsize;     // Section header table entry size
    uint16_t e_shnum;         // Section header table entry count
    uint16_t e_shstrndx;      // Section header string table index
} __attribute__((packed)) Elf32_Ehdr;

// Program Header (32-bit)
typedef struct {
    uint32_t p_type;          // Segment type
    uint32_t p_offset;        // Segment file offset
    uint32_t p_vaddr;         // Segment virtual address
    uint32_t p_paddr;         // Segment physical address
    uint32_t p_filesz;        // Segment size in file
    uint32_t p_memsz;         // Segment size in memory
    uint32_t p_flags;         // Segment flags
    uint32_t p_align;         // Segment alignment
} __attribute__((packed)) Elf32_Phdr;

// Section Header (32-bit)
typedef struct {
    uint32_t sh_name;         // Section name (string tbl index)
    uint32_t sh_type;         // Section type
    uint32_t sh_flags;        // Section flags
    uint32_t sh_addr;         // Section virtual addr at execution
    uint32_t sh_offset;       // Section file offset
    uint32_t sh_size;         // Section size in bytes
    uint32_t sh_link;         // Link to another section
    uint32_t sh_info;         // Additional section information
    uint32_t sh_addralign;    // Section alignment
    uint32_t sh_entsize;      // Entry size if section holds table
} __attribute__((packed)) Elf32_Shdr;

// ELF Loader Functions
int elf_validate(const void *data, uint32_t size);
uint32_t elf_load(const char *path, uint32_t *entry_point);
void elf_unload(uint32_t base_addr);

#endif
