#ifndef FORGE_MEMORY_H
#define FORGE_MEMORY_H

#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t data[FORGE_MEM_SIZE];
    bool write_protect_system; /* If true, writes to 0x0000..0x03FF fault */
    uint32_t last_read_addr;
    uint32_t last_write_addr;
    bool has_read;
    bool has_write;
} forge_memory_t;

/* Initialize/clear all 64 KB of memory */
void forge_mem_init(forge_memory_t *mem);

/* Read an 8-bit byte at addr */
forge_error_t forge_mem_read8(forge_memory_t *mem, uint16_t addr, uint8_t *out_val);

/* Write an 8-bit byte at addr */
forge_error_t forge_mem_write8(forge_memory_t *mem, uint16_t addr, uint8_t val);

/* Read a 16-bit little-endian word at addr (fails if addr == 0xFFFF) */
forge_error_t forge_mem_read16(forge_memory_t *mem, uint16_t addr, uint16_t *out_val);

/* Read a 16-bit little-endian word without updating access tracking (for inspection/disasm) */
forge_error_t forge_mem_peek16(const forge_memory_t *mem, uint16_t addr, uint16_t *out_val);

/* Write a 16-bit little-endian word at addr (fails if addr == 0xFFFF or protected) */
forge_error_t forge_mem_write16(forge_memory_t *mem, uint16_t addr, uint16_t val);

/* Copy a block of bytes into memory starting at start_addr (bounds-checked) */
forge_error_t forge_mem_load_block(
    forge_memory_t *mem,
    uint16_t start_addr,
    const uint8_t *src,
    size_t len
);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_MEMORY_H */

