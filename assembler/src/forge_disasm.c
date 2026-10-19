#include "forge_asm.h"
#include <stdio.h>
#include <string.h>

static uint16_t read_le16_bytes(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

forge_error_t forge_disassemble_binary(
    const uint8_t *bytes,
    size_t len,
    uint16_t default_load_addr,
    char *out_buf,
    size_t out_buf_size
) {
    if (bytes == NULL || out_buf == NULL || out_buf_size == 0U) {
        return FORGE_ERR_NULL_POINTER;
    }

    out_buf[0] = '\0';
    size_t offset = 0U;

    const uint8_t *payload = bytes;
    size_t payload_len = len;
    uint16_t base_addr = default_load_addr;
    uint16_t entry_pc  = default_load_addr;

    if (len >= FORGE_FBIN_HEADER_SIZE &&
        bytes[0] == FORGE_FBIN_MAGIC_0 &&
        bytes[1] == FORGE_FBIN_MAGIC_1 &&
        bytes[2] == FORGE_FBIN_MAGIC_2 &&
        bytes[3] == FORGE_FBIN_MAGIC_3) {
        uint16_t version   = read_le16_bytes(&bytes[4]);
        base_addr          = read_le16_bytes(&bytes[6]);
        entry_pc           = read_le16_bytes(&bytes[8]);
        uint16_t code_size = read_le16_bytes(&bytes[10]);

        if (version != FORGE_FBIN_VERSION || len < (size_t)FORGE_FBIN_HEADER_SIZE + (size_t)code_size) {
            return FORGE_ERR_LOAD_FAILURE;
        }
        payload = &bytes[FORGE_FBIN_HEADER_SIZE];
        payload_len = code_size;

        int hdr_written = snprintf(
            out_buf + offset,
            out_buf_size - offset,
            "; Forge-16 Disassembly (.fbin v%u, load=0x%04X, entry=0x%04X, size=%u bytes)\n",
            version,
            base_addr,
            entry_pc,
            code_size
        );
        if (hdr_written > 0 && (size_t)hdr_written < out_buf_size - offset) {
            offset += (size_t)hdr_written;
        }
    }

    size_t pos = 0U;
    while (pos < payload_len && offset + 1U < out_buf_size) {
        uint16_t addr = (uint16_t)(base_addr + (uint16_t)pos);
        int written = 0;

        if (pos + 4U <= payload_len) {
            uint16_t w0 = read_le16_bytes(&payload[pos]);
            uint16_t w1 = read_le16_bytes(&payload[pos + 2U]);
            forge_instruction_t instr;
            if (forge_decode_instruction(w0, w1, &instr) == FORGE_OK) {
                char mnem_buf[64];
                forge_disassemble_instruction(&instr, mnem_buf, sizeof(mnem_buf));
                written = snprintf(
                    out_buf + offset,
                    out_buf_size - offset,
                    "0x%04X: %04X %04X    %s\n",
                    addr,
                    w0,
                    w1,
                    mnem_buf
                );
                pos += 4U;
            } else if (pos + 2U <= payload_len) {
                written = snprintf(
                    out_buf + offset,
                    out_buf_size - offset,
                    "0x%04X: %04X         .word 0x%04X\n",
                    addr,
                    w0,
                    w0
                );
                pos += 2U;
            }
        } else if (pos + 2U <= payload_len) {
            uint16_t w0 = read_le16_bytes(&payload[pos]);
            written = snprintf(
                out_buf + offset,
                out_buf_size - offset,
                "0x%04X: %04X         .word 0x%04X\n",
                addr,
                w0,
                w0
            );
            pos += 2U;
        } else {
            written = snprintf(
                out_buf + offset,
                out_buf_size - offset,
                "0x%04X: %02X           .byte 0x%02X\n",
                addr,
                payload[pos],
                payload[pos]
            );
            pos += 1U;
        }

        if (written < 0 || (size_t)written >= out_buf_size - offset) {
            break;
        }
        offset += (size_t)written;
    }

    return FORGE_OK;
}

