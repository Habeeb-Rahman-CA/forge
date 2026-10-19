#include "forge_memory.h"
#include <string.h>

void forge_mem_init(forge_memory_t *mem) {
    if (mem == NULL) {
        return;
    }
    memset(mem->data, 0, sizeof(mem->data));
    mem->write_protect_system = false;
    mem->last_read_addr = 0U;
    mem->last_write_addr = 0U;
    mem->has_read = false;
    mem->has_write = false;
}

forge_error_t forge_mem_read8(forge_memory_t *mem, uint16_t addr, uint8_t *out_val) {
    if (mem == NULL || out_val == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    *out_val = mem->data[addr];
    mem->last_read_addr = addr;
    mem->has_read = true;
    return FORGE_OK;
}

forge_error_t forge_mem_write8(forge_memory_t *mem, uint16_t addr, uint8_t val) {
    if (mem == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (mem->write_protect_system && addr <= FORGE_SYSTEM_END) {
        return FORGE_ERR_MEMORY_OUT_OF_BOUNDS;
    }
    mem->data[addr] = val;
    mem->last_write_addr = addr;
    mem->has_write = true;
    return FORGE_OK;
}

forge_error_t forge_mem_peek16(const forge_memory_t *mem, uint16_t addr, uint16_t *out_val) {
    if (mem == NULL || out_val == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (addr == 0xFFFFU) {
        return FORGE_ERR_MEMORY_OUT_OF_BOUNDS;
    }
    uint16_t lo = (uint16_t)mem->data[addr];
    uint16_t hi = (uint16_t)mem->data[(uint16_t)(addr + 1U)];
    *out_val = (uint16_t)(lo | (uint16_t)(hi << 8));
    return FORGE_OK;
}

forge_error_t forge_mem_read16(forge_memory_t *mem, uint16_t addr, uint16_t *out_val) {
    forge_error_t err = forge_mem_peek16(mem, addr, out_val);
    if (err != FORGE_OK) {
        return err;
    }
    mem->last_read_addr = addr;
    mem->has_read = true;
    return FORGE_OK;
}

forge_error_t forge_mem_write16(forge_memory_t *mem, uint16_t addr, uint16_t val) {
    if (mem == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (addr == 0xFFFFU) {
        return FORGE_ERR_MEMORY_OUT_OF_BOUNDS;
    }
    if (mem->write_protect_system && (addr <= FORGE_SYSTEM_END || (uint16_t)(addr + 1U) <= FORGE_SYSTEM_END)) {
        return FORGE_ERR_MEMORY_OUT_OF_BOUNDS;
    }
    mem->data[addr] = (uint8_t)(val & 0xFFU);
    mem->data[(uint16_t)(addr + 1U)] = (uint8_t)((val >> 8) & 0xFFU);
    mem->last_write_addr = addr;
    mem->has_write = true;
    return FORGE_OK;
}

forge_error_t forge_mem_load_block(
    forge_memory_t *mem,
    uint16_t start_addr,
    const uint8_t *src,
    size_t len
) {
    if (mem == NULL || (src == NULL && len > 0U)) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (len == 0U) {
        return FORGE_OK;
    }
    if ((size_t)start_addr + len > (size_t)FORGE_MEM_SIZE) {
        return FORGE_ERR_MEMORY_OUT_OF_BOUNDS;
    }
    if (mem->write_protect_system && start_addr <= FORGE_SYSTEM_END) {
        return FORGE_ERR_MEMORY_OUT_OF_BOUNDS;
    }
    memcpy(&mem->data[start_addr], src, len);
    return FORGE_OK;
}

