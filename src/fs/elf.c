#include "fs/elf.h"
#include "fs/fs.h"
#include "mem/heap.h"
#include "mem/paging.h"
#include "ui/console.h"
#include <string.h>

// Validate ELF header
int elf_validate(const void *data, uint32_t size)
{
    if (size < sizeof(Elf32_Ehdr)) {
        console_write("[ELF] File too small\n");
        return -1;
    }

    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)data;

    // Check magic number
    if (ehdr->e_ident[0] != 0x7F ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        console_write("[ELF] Invalid magic number\n");
        return -1;
    }

    // Check class (32-bit)
    if (ehdr->e_ident[4] != ELFCLASS32) {
        console_write("[ELF] Not a 32-bit ELF\n");
        return -1;
    }

    // Check endianness (little-endian)
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        console_write("[ELF] Not little-endian\n");
        return -1;
    }

    // Check type (executable)
    if (ehdr->e_type != ET_EXEC) {
        console_write("[ELF] Not an executable\n");
        return -1;
    }

    // Check machine (x86)
    if (ehdr->e_machine != EM_386) {
        console_write("[ELF] Not an x86 binary\n");
        return -1;
    }

    return 0;
}

// Load ELF binary from filesystem
uint32_t elf_load(const char *path, uint32_t *entry_point)
{
    // Read file from filesystem
    const char *file_data = fs_find(path);
    if (!file_data) {
        console_write("[ELF] File not found: ");
        console_write(path);
        console_write("\n");
        return 0;
    }

    // Get file size (we need a way to get this from fs_find)
    // For now, assume it's null-terminated or we read a fixed size
    // This is a limitation - we need fs_stat or similar
    uint32_t file_size = strlen(file_data); // HACK: This won't work for binary data!
    
    // Validate ELF
    if (elf_validate(file_data, file_size) != 0) {
        return 0;
    }

    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)file_data;
    
    console_write("[ELF] Loading ");
    console_write(path);
    console_write("...\n");
    console_write("[ELF] Entry point: 0x");
    console_write_hex(ehdr->e_entry);
    console_write("\n");

    // Parse program headers
    const Elf32_Phdr *phdr = (const Elf32_Phdr *)(file_data + ehdr->e_phoff);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            console_write("[ELF] LOAD segment: vaddr=0x");
            console_write_hex(phdr[i].p_vaddr);
            console_write(" filesz=");
            console_write_dec(phdr[i].p_filesz);
            console_write(" memsz=");
            console_write_dec(phdr[i].p_memsz);
            console_write("\n");

            // Allocate memory for segment
            uint32_t vaddr = phdr[i].p_vaddr;
            uint32_t memsz = phdr[i].p_memsz;
            uint32_t filesz = phdr[i].p_filesz;
            
            // Allocate pages for this segment
            uint32_t num_pages = (memsz + 0xFFF) / 0x1000;
            for (uint32_t j = 0; j < num_pages; j++) {
                uint32_t page_vaddr = (vaddr & 0xFFFFF000) + (j * 0x1000);
                uint32_t page_phys = (uint32_t)kmalloc(0x1000);
                
                // Map as user-accessible
                uint32_t flags = PAGE_PRESENT | PAGE_RW | PAGE_USER;
                if (!(phdr[i].p_flags & PF_W)) {
                    flags &= ~PAGE_RW; // Read-only if not writable
                }
                
                paging_map(page_vaddr, paging_virt_to_phys(page_phys), flags);
            }

            // Copy segment data
            if (filesz > 0) {
                memcpy((void *)vaddr, file_data + phdr[i].p_offset, filesz);
            }

            // Zero BSS (memsz > filesz)
            if (memsz > filesz) {
                memset((void *)(vaddr + filesz), 0, memsz - filesz);
            }
        }
    }

    *entry_point = ehdr->e_entry;
    console_write("[ELF] Load complete\n");
    
    return ehdr->e_entry; // Return entry point as base address
}

// Unload ELF binary (free memory)
void elf_unload(uint32_t base_addr)
{
    // TODO: Implement memory cleanup
    // This requires tracking which pages belong to the process
    (void)base_addr;
}
