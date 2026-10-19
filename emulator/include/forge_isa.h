#ifndef FORGE_ISA_H
#define FORGE_ISA_H

#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 6-bit Opcodes (0x00 .. 0x1F) */
typedef enum {
    FORGE_OP_NOP   = 0x00,
    FORGE_OP_HALT  = 0x01,
    FORGE_OP_MOV   = 0x02,
    FORGE_OP_LOAD  = 0x03,
    FORGE_OP_STORE = 0x04,
    FORGE_OP_ADD   = 0x05,
    FORGE_OP_SUB   = 0x06,
    FORGE_OP_MUL   = 0x07,
    FORGE_OP_DIV   = 0x08,
    FORGE_OP_AND   = 0x09,
    FORGE_OP_OR    = 0x0A,
    FORGE_OP_XOR   = 0x0B,
    FORGE_OP_NOT   = 0x0C,
    FORGE_OP_SHL   = 0x0D,
    FORGE_OP_SHR   = 0x0E,
    FORGE_OP_CMP   = 0x0F,
    FORGE_OP_JMP   = 0x10,
    FORGE_OP_JZ    = 0x11,
    FORGE_OP_JNZ   = 0x12,
    FORGE_OP_CALL  = 0x13,
    FORGE_OP_RET   = 0x14,
    FORGE_OP_PUSH  = 0x15,
    FORGE_OP_POP   = 0x16,
    FORGE_OP_IN    = 0x17,
    FORGE_OP_OUT   = 0x18,
    FORGE_OP_JC    = 0x19,
    FORGE_OP_JNC   = 0x1A,
    FORGE_OP_JN    = 0x1B,
    FORGE_OP_JL    = 0x1C,
    FORGE_OP_JLE   = 0x1D,
    FORGE_OP_JG    = 0x1E,
    FORGE_OP_JGE   = 0x1F,
    FORGE_OP_COUNT = 0x20
} forge_opcode_t;

/* 2-bit Operand / Addressing Modes */
typedef enum {
    FORGE_MODE_REG     = 0x00, /* Register mode: Rs */
    FORGE_MODE_IMM     = 0x01, /* Immediate mode: imm16 */
    FORGE_MODE_MEM_DIR = 0x02, /* Direct memory mode: [imm16] */
    FORGE_MODE_MEM_IDX = 0x03  /* Indexed memory mode: [Rs + imm16] */
} forge_addr_mode_t;

/* Decoded 32-bit (2-word) Instruction */
typedef struct {
    uint16_t word0;         /* Raw first 16-bit word */
    uint16_t imm16;         /* Raw second 16-bit word (immediate/address/offset) */
    forge_opcode_t opcode;  /* Bits 15..10 of word0 */
    forge_addr_mode_t mode; /* Bits 9..8 of word0 */
    uint8_t rd;             /* Bits 7..5 of word0 (0..7) */
    uint8_t rs;             /* Bits 4..2 of word0 (0..7) */
    uint8_t reserved;       /* Bits 1..0 of word0 (must be 0) */
    uint32_t cycles;        /* Base cycle cost of this instruction */
} forge_instruction_t;

/* Encode a Forge-16 instruction into word0 and word1 (imm16) */
void forge_encode_instruction(
    forge_opcode_t opcode,
    forge_addr_mode_t mode,
    uint8_t rd,
    uint8_t rs,
    uint16_t imm16,
    uint16_t *out_word0,
    uint16_t *out_word1
);

/* Convenience helper returning Word 0 of an encoded instruction */
uint16_t forge_encode_word0(
    forge_opcode_t opcode,
    forge_addr_mode_t mode,
    uint8_t rd,
    uint8_t rs
);

/* Decode a pair of 16-bit words into a validated forge_instruction_t */
forge_error_t forge_decode_instruction(
    uint16_t word0,
    uint16_t word1,
    forge_instruction_t *out_instr
);

/* Format a decoded instruction into readable assembly text */
void forge_disassemble_instruction(
    const forge_instruction_t *instr,
    char *buf,
    size_t buf_size
);

/* Return mnemonic string for an opcode */
const char *forge_opcode_name(forge_opcode_t opcode);

/* Return cycle count for an opcode */
uint32_t forge_opcode_cycles(forge_opcode_t opcode);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_ISA_H */

