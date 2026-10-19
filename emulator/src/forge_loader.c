#include "forge_cpu.h"
#include <stdio.h>
#include <stdlib.h>

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

forge_error_t forge_cpu_load_raw(
    forge_cpu_t *cpu,
    const uint8_t *bytes,
    size_t len,
    uint16_t load_addr,
    uint16_t entry_pc
) {
    if (cpu == NULL || (bytes == NULL && len > 0U)) {
        return FORGE_ERR_NULL_POINTER;
    }
    if ((size_t)load_addr + len > (size_t)FORGE_MEM_SIZE) {
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }

    forge_error_t err = forge_mem_load_block(&cpu->mem, load_addr, bytes, len);
    if (err != FORGE_OK) {
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }

    cpu->pc = entry_pc;
    cpu->halted = false;
    cpu->error = FORGE_OK;
    return FORGE_OK;
}

forge_error_t forge_cpu_load_fbin(
    forge_cpu_t *cpu,
    const uint8_t *bytes,
    size_t len
) {
    if (cpu == NULL || bytes == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (len < FORGE_FBIN_HEADER_SIZE) {
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }

    if (bytes[0] != FORGE_FBIN_MAGIC_0 ||
        bytes[1] != FORGE_FBIN_MAGIC_1 ||
        bytes[2] != FORGE_FBIN_MAGIC_2 ||
        bytes[3] != FORGE_FBIN_MAGIC_3) {
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }

    uint16_t version   = read_le16(&bytes[4]);
    uint16_t load_addr = read_le16(&bytes[6]);
    uint16_t entry_pc  = read_le16(&bytes[8]);
    uint16_t code_size = read_le16(&bytes[10]);

    if (version != FORGE_FBIN_VERSION) {
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }
    if (len < (size_t)FORGE_FBIN_HEADER_SIZE + (size_t)code_size) {
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }

    return forge_cpu_load_raw(
        cpu,
        &bytes[FORGE_FBIN_HEADER_SIZE],
        code_size,
        load_addr,
        entry_pc
    );
}

forge_error_t forge_cpu_load_binary(
    forge_cpu_t *cpu,
    const uint8_t *bytes,
    size_t len,
    uint16_t default_load_addr
) {
    if (cpu == NULL || bytes == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (len >= FORGE_FBIN_HEADER_SIZE &&
        bytes[0] == FORGE_FBIN_MAGIC_0 &&
        bytes[1] == FORGE_FBIN_MAGIC_1 &&
        bytes[2] == FORGE_FBIN_MAGIC_2 &&
        bytes[3] == FORGE_FBIN_MAGIC_3) {
        return forge_cpu_load_fbin(cpu, bytes, len);
    }
    return forge_cpu_load_raw(cpu, bytes, len, default_load_addr, default_load_addr);
}

forge_error_t forge_cpu_load_file(
    forge_cpu_t *cpu,
    const char *filepath,
    uint16_t default_load_addr
) {
    if (cpu == NULL || filepath == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }
    long fsize = ftell(fp);
    if (fsize < 0 || fsize > (long)(FORGE_MEM_SIZE + FORGE_FBIN_HEADER_SIZE)) {
        fclose(fp);
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }

    size_t size = (size_t)fsize;
    uint8_t *buf = (uint8_t *)malloc(size > 0U ? size : 1U);
    if (buf == NULL) {
        fclose(fp);
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }

    size_t nread = fread(buf, 1U, size, fp);
    fclose(fp);
    if (nread != size) {
        free(buf);
        cpu->error = FORGE_ERR_LOAD_FAILURE;
        return FORGE_ERR_LOAD_FAILURE;
    }

    forge_error_t err = forge_cpu_load_binary(cpu, buf, size, default_load_addr);
    free(buf);
    return err;
}

forge_error_t forge_cpu_emit_instr(
    forge_cpu_t *cpu,
    uint16_t *inout_addr,
    forge_opcode_t opcode,
    forge_addr_mode_t mode,
    uint8_t rd,
    uint8_t rs,
    uint16_t imm16
) {
    if (cpu == NULL || inout_addr == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (*inout_addr > 0xFFFCU) {
        return FORGE_ERR_MEMORY_OUT_OF_BOUNDS;
    }
    uint16_t w0 = 0U;
    uint16_t w1 = 0U;
    forge_encode_instruction(opcode, mode, rd, rs, imm16, &w0, &w1);

    forge_error_t err = forge_mem_write16(&cpu->mem, *inout_addr, w0);
    if (err != FORGE_OK) {
        return err;
    }
    err = forge_mem_write16(&cpu->mem, (uint16_t)(*inout_addr + 2U), w1);
    if (err != FORGE_OK) {
        return err;
    }
    *inout_addr = (uint16_t)(*inout_addr + 4U);
    return FORGE_OK;
}

