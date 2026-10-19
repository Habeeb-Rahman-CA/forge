#include "forge_isa.h"
#include <stdio.h>

const char *forge_opcode_name(forge_opcode_t opcode) {
    switch (opcode) {
        case FORGE_OP_NOP:   return "NOP";
        case FORGE_OP_HALT:  return "HALT";
        case FORGE_OP_MOV:   return "MOV";
        case FORGE_OP_LOAD:  return "LOAD";
        case FORGE_OP_STORE: return "STORE";
        case FORGE_OP_ADD:   return "ADD";
        case FORGE_OP_SUB:   return "SUB";
        case FORGE_OP_MUL:   return "MUL";
        case FORGE_OP_DIV:   return "DIV";
        case FORGE_OP_AND:   return "AND";
        case FORGE_OP_OR:    return "OR";
        case FORGE_OP_XOR:   return "XOR";
        case FORGE_OP_NOT:   return "NOT";
        case FORGE_OP_SHL:   return "SHL";
        case FORGE_OP_SHR:   return "SHR";
        case FORGE_OP_CMP:   return "CMP";
        case FORGE_OP_JMP:   return "JMP";
        case FORGE_OP_JZ:    return "JZ";
        case FORGE_OP_JNZ:   return "JNZ";
        case FORGE_OP_CALL:  return "CALL";
        case FORGE_OP_RET:   return "RET";
        case FORGE_OP_PUSH:  return "PUSH";
        case FORGE_OP_POP:   return "POP";
        case FORGE_OP_IN:    return "IN";
        case FORGE_OP_OUT:   return "OUT";
        case FORGE_OP_JC:    return "JC";
        case FORGE_OP_JNC:   return "JNC";
        case FORGE_OP_JN:    return "JN";
        case FORGE_OP_JL:    return "JL";
        case FORGE_OP_JLE:   return "JLE";
        case FORGE_OP_JG:    return "JG";
        case FORGE_OP_JGE:   return "JGE";
        default:             return "INVALID";
    }
}

uint32_t forge_opcode_cycles(forge_opcode_t opcode) {
    switch (opcode) {
        case FORGE_OP_NOP:
        case FORGE_OP_HALT:
        case FORGE_OP_MOV:
        case FORGE_OP_ADD:
        case FORGE_OP_SUB:
        case FORGE_OP_AND:
        case FORGE_OP_OR:
        case FORGE_OP_XOR:
        case FORGE_OP_NOT:
        case FORGE_OP_SHL:
        case FORGE_OP_SHR:
        case FORGE_OP_CMP:
            return 1U;
        case FORGE_OP_LOAD:
        case FORGE_OP_STORE:
        case FORGE_OP_JMP:
        case FORGE_OP_JZ:
        case FORGE_OP_JNZ:
        case FORGE_OP_JC:
        case FORGE_OP_JNC:
        case FORGE_OP_JN:
        case FORGE_OP_JL:
        case FORGE_OP_JLE:
        case FORGE_OP_JG:
        case FORGE_OP_JGE:
        case FORGE_OP_PUSH:
        case FORGE_OP_POP:
        case FORGE_OP_IN:
        case FORGE_OP_OUT:
            return 2U;
        case FORGE_OP_MUL:
        case FORGE_OP_CALL:
        case FORGE_OP_RET:
            return 3U;
        case FORGE_OP_DIV:
            return 4U;
        default:
            return 1U;
    }
}

uint16_t forge_encode_word0(
    forge_opcode_t opcode,
    forge_addr_mode_t mode,
    uint8_t rd,
    uint8_t rs
) {
    uint16_t op_bits   = (uint16_t)(((uint16_t)opcode & 0x3FU) << 10);
    uint16_t mode_bits = (uint16_t)(((uint16_t)mode & 0x03U) << 8);
    uint16_t rd_bits   = (uint16_t)(((uint16_t)rd & 0x07U) << 5);
    uint16_t rs_bits   = (uint16_t)(((uint16_t)rs & 0x07U) << 2);
    return (uint16_t)(op_bits | mode_bits | rd_bits | rs_bits);
}

void forge_encode_instruction(
    forge_opcode_t opcode,
    forge_addr_mode_t mode,
    uint8_t rd,
    uint8_t rs,
    uint16_t imm16,
    uint16_t *out_word0,
    uint16_t *out_word1
) {
    if (out_word0 != NULL) {
        *out_word0 = forge_encode_word0(opcode, mode, rd, rs);
    }
    if (out_word1 != NULL) {
        *out_word1 = imm16;
    }
}

static bool is_mode_valid_for_opcode(forge_opcode_t opcode, forge_addr_mode_t mode) {
    switch (opcode) {
        case FORGE_OP_NOP:
        case FORGE_OP_HALT:
        case FORGE_OP_NOT:
        case FORGE_OP_RET:
        case FORGE_OP_POP:
            return mode == FORGE_MODE_REG;

        case FORGE_OP_MOV:
        case FORGE_OP_ADD:
        case FORGE_OP_SUB:
        case FORGE_OP_MUL:
        case FORGE_OP_DIV:
        case FORGE_OP_AND:
        case FORGE_OP_OR:
        case FORGE_OP_XOR:
        case FORGE_OP_SHL:
        case FORGE_OP_SHR:
        case FORGE_OP_CMP:
        case FORGE_OP_JMP:
        case FORGE_OP_JZ:
        case FORGE_OP_JNZ:
        case FORGE_OP_CALL:
        case FORGE_OP_PUSH:
        case FORGE_OP_IN:
        case FORGE_OP_OUT:
        case FORGE_OP_JC:
        case FORGE_OP_JNC:
        case FORGE_OP_JN:
        case FORGE_OP_JL:
        case FORGE_OP_JLE:
        case FORGE_OP_JG:
        case FORGE_OP_JGE:
            return (mode == FORGE_MODE_REG) || (mode == FORGE_MODE_IMM);

        case FORGE_OP_LOAD:
        case FORGE_OP_STORE:
            return (mode == FORGE_MODE_MEM_DIR) || (mode == FORGE_MODE_MEM_IDX);

        default:
            return false;
    }
}

forge_error_t forge_decode_instruction(
    uint16_t word0,
    uint16_t word1,
    forge_instruction_t *out_instr
) {
    if (out_instr == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }

    uint8_t raw_op   = (uint8_t)((word0 >> 10) & 0x3FU);
    uint8_t raw_mode = (uint8_t)((word0 >> 8) & 0x03U);
    uint8_t rd       = (uint8_t)((word0 >> 5) & 0x07U);
    uint8_t rs       = (uint8_t)((word0 >> 2) & 0x07U);
    uint8_t reserved = (uint8_t)(word0 & 0x03U);

    out_instr->word0    = word0;
    out_instr->imm16    = word1;
    out_instr->opcode   = (forge_opcode_t)raw_op;
    out_instr->mode     = (forge_addr_mode_t)raw_mode;
    out_instr->rd       = rd;
    out_instr->rs       = rs;
    out_instr->reserved = reserved;
    out_instr->cycles   = forge_opcode_cycles((forge_opcode_t)raw_op);

    if (raw_op >= (uint8_t)FORGE_OP_COUNT) {
        return FORGE_ERR_INVALID_INSTRUCTION;
    }
    if (reserved != 0U) {
        return FORGE_ERR_INVALID_INSTRUCTION;
    }
    if (!is_mode_valid_for_opcode(out_instr->opcode, out_instr->mode)) {
        return FORGE_ERR_INVALID_INSTRUCTION;
    }

    return FORGE_OK;
}

void forge_disassemble_instruction(
    const forge_instruction_t *instr,
    char *buf,
    size_t buf_size
) {
    if (buf == NULL || buf_size == 0U) {
        return;
    }
    if (instr == NULL) {
        snprintf(buf, buf_size, "<null>");
        return;
    }

    const char *op = forge_opcode_name(instr->opcode);
    const char *rd = forge_reg_name(instr->rd);
    const char *rs = forge_reg_name(instr->rs);

    switch (instr->opcode) {
        case FORGE_OP_NOP:
        case FORGE_OP_HALT:
        case FORGE_OP_RET:
            snprintf(buf, buf_size, "%s", op);
            break;

        case FORGE_OP_NOT:
        case FORGE_OP_POP:
            snprintf(buf, buf_size, "%s %s", op, rd);
            break;

        case FORGE_OP_PUSH:
            if (instr->mode == FORGE_MODE_IMM) {
                snprintf(buf, buf_size, "%s 0x%04X", op, instr->imm16);
            } else {
                snprintf(buf, buf_size, "%s %s", op, rd);
            }
            break;

        case FORGE_OP_JMP:
        case FORGE_OP_JZ:
        case FORGE_OP_JNZ:
        case FORGE_OP_CALL:
        case FORGE_OP_JC:
        case FORGE_OP_JNC:
        case FORGE_OP_JN:
        case FORGE_OP_JL:
        case FORGE_OP_JLE:
        case FORGE_OP_JG:
        case FORGE_OP_JGE:
            if (instr->mode == FORGE_MODE_IMM) {
                snprintf(buf, buf_size, "%s 0x%04X", op, instr->imm16);
            } else {
                snprintf(buf, buf_size, "%s %s", op, rs);
            }
            break;

        case FORGE_OP_LOAD:
        case FORGE_OP_STORE:
            if (instr->mode == FORGE_MODE_MEM_DIR) {
                snprintf(buf, buf_size, "%s %s, [0x%04X]", op, rd, instr->imm16);
            } else {
                int16_t offset = (int16_t)instr->imm16;
                if (offset == 0) {
                    snprintf(buf, buf_size, "%s %s, [%s]", op, rd, rs);
                } else if (offset > 0) {
                    snprintf(buf, buf_size, "%s %s, [%s + %d]", op, rd, rs, (int)offset);
                } else {
                    snprintf(buf, buf_size, "%s %s, [%s - %d]", op, rd, rs, -(int)offset);
                }
            }
            break;

        case FORGE_OP_MOV:
        case FORGE_OP_ADD:
        case FORGE_OP_SUB:
        case FORGE_OP_MUL:
        case FORGE_OP_DIV:
        case FORGE_OP_AND:
        case FORGE_OP_OR:
        case FORGE_OP_XOR:
        case FORGE_OP_SHL:
        case FORGE_OP_SHR:
        case FORGE_OP_CMP:
        case FORGE_OP_IN:
        case FORGE_OP_OUT:
            if (instr->mode == FORGE_MODE_IMM) {
                snprintf(buf, buf_size, "%s %s, 0x%04X", op, rd, instr->imm16);
            } else {
                snprintf(buf, buf_size, "%s %s, %s", op, rd, rs);
            }
            break;

        default:
            snprintf(buf, buf_size, "DW 0x%04X, 0x%04X", instr->word0, instr->imm16);
            break;
    }
}

