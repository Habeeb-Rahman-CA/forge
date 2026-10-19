#include "forge_asm.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int str_icmp(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) {
            return ca - cb;
        }
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

void forge_asm_result_init(forge_asm_result_t *res) {
    if (res == NULL) {
        return;
    }
    memset(res, 0, sizeof(*res));
    res->load_addr = FORGE_DEFAULT_PC;
    res->entry_pc  = FORGE_DEFAULT_PC;
}

static void strip_comment(char *line) {
    bool in_single_quote = false;
    bool in_double_quote = false;
    for (size_t i = 0U; line[i] != '\0'; i++) {
        if (line[i] == '\\' && line[i + 1U] != '\0') {
            i++;
            continue;
        }
        if (line[i] == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (line[i] == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (!in_single_quote && !in_double_quote) {
            if (line[i] == ';') {
                line[i] = '\0';
                break;
            }
            if (line[i] == '/' && line[i + 1U] == '/') {
                line[i] = '\0';
                break;
            }
        }
    }
}

static char *trim_whitespace(char *s) {
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '\0') {
        return s;
    }
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

/* Split comma-separated operands while respecting quotes and brackets */
static size_t split_operands(char *str, char **out_ops, size_t max_ops, bool *out_trailing_comma) {
    if (out_trailing_comma != NULL) {
        *out_trailing_comma = false;
    }
    str = trim_whitespace(str);
    if (*str == '\0') {
        return 0U;
    }

    size_t count = 0U;
    bool in_sq = false;
    bool in_dq = false;
    int bracket_depth = 0;
    char *start = str;

    for (char *p = str; ; p++) {
        char c = *p;
        if (c == '\\' && p[1] != '\0') {
            p++;
            continue;
        }
        if (c == '\'' && !in_dq) {
            in_sq = !in_sq;
        } else if (c == '"' && !in_sq) {
            in_dq = !in_dq;
        } else if (!in_sq && !in_dq) {
            if (c == '[') {
                bracket_depth++;
            } else if (c == ']') {
                bracket_depth--;
            } else if ((c == ',' && bracket_depth == 0) || c == '\0') {
                if (c == ',') {
                    *p = '\0';
                }
                if (count < max_ops) {
                    out_ops[count++] = trim_whitespace(start);
                }
                if (c == '\0') {
                    break;
                }
                start = p + 1;
                if (* trim_whitespace(start) == '\0' && out_trailing_comma != NULL) {
                    *out_trailing_comma = true;
                }
            }
        }
        if (c == '\0') {
            break;
        }
    }
    return count;
}

static bool emit_byte(forge_asm_result_t *res, uint32_t *curr_addr, uint8_t b, bool is_pass2, uint32_t line) {
    if (*curr_addr >= FORGE_MEM_SIZE || res->code_size >= FORGE_ASM_MAX_CODE_SIZE) {
        if (is_pass2) {
            forge_asm_add_error(res, line, 1U, "Program exceeds 64 KB address space");
        }
        return false;
    }
    if (is_pass2) {
        res->code[res->code_size++] = b;
    }
    (*curr_addr)++;
    return true;
}

static bool emit_word16(forge_asm_result_t *res, uint32_t *curr_addr, uint16_t w, bool is_pass2, uint32_t line) {
    if (!emit_byte(res, curr_addr, (uint8_t)(w & 0xFFU), is_pass2, line)) {
        return false;
    }
    if (!emit_byte(res, curr_addr, (uint8_t)((w >> 8) & 0xFFU), is_pass2, line)) {
        return false;
    }
    return true;
}

/* Parse a memory operand "[addr]" or "[Rs]" or "[Rs + offset]" or "[Rs - offset]" */
static bool parse_memory_operand(
    forge_asm_result_t *res,
    const char *op_str,
    uint32_t line,
    forge_addr_mode_t *out_mode,
    uint8_t *out_rs,
    uint16_t *out_imm16
) {
    size_t len = strlen(op_str);
    if (len < 3U || op_str[0] != '[' || op_str[len - 1U] != ']') {
        forge_asm_add_error(res, line, 1U, "Expected memory operand enclosed in '[...]', got '%s'", op_str);
        return false;
    }

    char inner[128];
    if (len - 2U >= sizeof(inner)) {
        forge_asm_add_error(res, line, 1U, "Memory operand too long");
        return false;
    }
    memcpy(inner, op_str + 1, len - 2U);
    inner[len - 2U] = '\0';
    char *trimmed = trim_whitespace(inner);
    if (*trimmed == '\0') {
        forge_asm_add_error(res, line, 1U, "Empty memory operand '[]'");
        return false;
    }

    /* Check if it starts with a register: [Rs], [Rs + imm], [Rs - imm] */
    char first_tok[64];
    size_t i = 0U;
    while (trimmed[i] != '\0' && !isspace((unsigned char)trimmed[i]) &&
           trimmed[i] != '+' && trimmed[i] != '-' && i + 1U < sizeof(first_tok)) {
        first_tok[i] = trimmed[i];
        i++;
    }
    first_tok[i] = '\0';

    uint8_t base_reg = 0U;
    if (forge_asm_parse_reg(first_tok, &base_reg)) {
        *out_mode = FORGE_MODE_MEM_IDX;
        *out_rs   = base_reg;
        char *rest = trim_whitespace(trimmed + i);
        if (*rest == '\0') {
            *out_imm16 = 0U;
            return true;
        }
        if (*rest != '+' && *rest != '-') {
            forge_asm_add_error(res, line, 1U, "Invalid indexed memory offset '%s'", op_str);
            return false;
        }
        char sign_char = *rest;
        char *offset_tok = trim_whitespace(rest + 1);
        if (*offset_tok == '\0') {
            forge_asm_add_error(res, line, 1U, "Missing offset after '%c' in '%s'", sign_char, op_str);
            return false;
        }
        uint16_t raw_off = 0U;
        if (!forge_asm_resolve_value(res, offset_tok, &raw_off)) {
            forge_asm_add_error(res, line, 1U, "Undefined symbol or invalid offset '%s'", offset_tok);
            return false;
        }
        *out_imm16 = (sign_char == '-') ? (uint16_t)(-(int32_t)raw_off) : raw_off;
        return true;
    }

    /* Otherwise direct memory mode: [addr_or_symbol] */
    uint16_t direct_addr = 0U;
    if (!forge_asm_resolve_value(res, trimmed, &direct_addr)) {
        forge_asm_add_error(res, line, 1U, "Undefined symbol or invalid address '%s' in memory operand", trimmed);
        return false;
    }
    *out_mode  = FORGE_MODE_MEM_DIR;
    *out_rs    = 0U;
    *out_imm16 = direct_addr;
    return true;
}

static bool parse_quoted_string(
    forge_asm_result_t *res,
    const char *str_operand,
    uint32_t line,
    uint8_t *out_bytes,
    size_t max_bytes,
    size_t *out_len
) {
    size_t slen = strlen(str_operand);
    if (slen < 2U || str_operand[0] != '"' || str_operand[slen - 1U] != '"') {
        forge_asm_add_error(res, line, 1U, "Expected double-quoted string literal, got '%s'", str_operand);
        return false;
    }

    size_t count = 0U;
    for (size_t i = 1U; i + 1U < slen; i++) {
        uint8_t b = (uint8_t)str_operand[i];
        if (b == '\\') {
            i++;
            if (i + 1U >= slen) {
                forge_asm_add_error(res, line, 1U, "Unterminated escape sequence in string");
                return false;
            }
            switch (str_operand[i]) {
                case 'n':  b = '\n'; break;
                case 't':  b = '\t'; break;
                case 'r':  b = '\r'; break;
                case '0':  b = '\0'; break;
                case '\\': b = '\\'; break;
                case '"':  b = '"';  break;
                default:
                    forge_asm_add_error(res, line, 1U, "Unknown escape sequence '\\%c'", str_operand[i]);
                    return false;
            }
        }
        if (count >= max_bytes) {
            forge_asm_add_error(res, line, 1U, "String literal too long");
            return false;
        }
        if (out_bytes != NULL) {
            out_bytes[count] = b;
        }
        count++;
    }
    *out_len = count;
    return true;
}

static void process_directive(
    forge_asm_result_t *res,
    const char *directive,
    char *operands_str,
    uint32_t *curr_addr,
    bool *has_emitted,
    bool *has_explicit_entry,
    bool is_pass2,
    uint32_t line
) {
    char *ops[64];
    bool trailing_comma = false;
    size_t op_count = split_operands(operands_str, ops, 64U, &trailing_comma);

    if (trailing_comma && is_pass2) {
        forge_asm_add_error(res, line, 1U, "Trailing comma in directive '%s'", directive);
        return;
    }

    if (str_icmp(directive, ".org") == 0) {
        if (op_count != 1U) {
            if (!is_pass2) {
                forge_asm_add_error(res, line, 1U, ".org expects exactly 1 address argument");
            }
            return;
        }
        uint16_t new_addr = 0U;
        if (!forge_asm_resolve_value(res, ops[0], &new_addr)) {
            if (!is_pass2) {
                forge_asm_add_error(res, line, 1U, "Invalid .org address '%s'", ops[0]);
            }
            return;
        }
        if (!(*has_emitted)) {
            res->load_addr = new_addr;
            if (!(*has_explicit_entry)) {
                res->entry_pc = new_addr;
            }
            *curr_addr = new_addr;
        } else {
            if (new_addr < *curr_addr) {
                if (is_pass2) {
                    forge_asm_add_error(res, line, 1U, ".org address 0x%04X is behind current address 0x%04X", new_addr, *curr_addr);
                }
                return;
            }
            uint32_t pad = (uint32_t)new_addr - *curr_addr;
            for (uint32_t i = 0U; i < pad; i++) {
                emit_byte(res, curr_addr, 0U, is_pass2, line);
            }
        }
        return;
    }

    if (str_icmp(directive, ".entry") == 0) {
        if (op_count != 1U) {
            if (is_pass2) {
                forge_asm_add_error(res, line, 1U, ".entry expects 1 label or address operand");
            }
            return;
        }
        if (is_pass2) {
            uint16_t entry = 0U;
            if (!forge_asm_resolve_value(res, ops[0], &entry)) {
                forge_asm_add_error(res, line, 1U, "Undefined entry symbol '%s'", ops[0]);
                return;
            }
            res->entry_pc = entry;
            *has_explicit_entry = true;
        }
        return;
    }

    if (str_icmp(directive, ".equ") == 0 || str_icmp(directive, ".const") == 0) {
        /* Support both ".equ NAME, VAL" and ".equ NAME VAL" */
        char name_buf[FORGE_ASM_MAX_NAME_LEN];
        char val_buf[FORGE_ASM_MAX_NAME_LEN];
        if (op_count == 2U) {
            strncpy(name_buf, ops[0], sizeof(name_buf) - 1U);
            name_buf[sizeof(name_buf) - 1U] = '\0';
            strncpy(val_buf, ops[1], sizeof(val_buf) - 1U);
            val_buf[sizeof(val_buf) - 1U] = '\0';
        } else if (op_count == 1U) {
            char *p = ops[0];
            size_t i = 0U;
            while (p[i] != '\0' && !isspace((unsigned char)p[i]) && i + 1U < sizeof(name_buf)) {
                name_buf[i] = p[i];
                i++;
            }
            name_buf[i] = '\0';
            char *rest = trim_whitespace(p + i);
            if (*rest == '\0') {
                if (!is_pass2) {
                    forge_asm_add_error(res, line, 1U, "%s requires a symbol name and value", directive);
                }
                return;
            }
            strncpy(val_buf, rest, sizeof(val_buf) - 1U);
            val_buf[sizeof(val_buf) - 1U] = '\0';
        } else {
            if (!is_pass2) {
                forge_asm_add_error(res, line, 1U, "%s requires a symbol name and value", directive);
            }
            return;
        }

        if (!is_pass2) {
            uint16_t val = 0U;
            if (!forge_asm_resolve_value(res, val_buf, &val)) {
                forge_asm_add_error(res, line, 1U, "Invalid constant value '%s' in %s", val_buf, directive);
                return;
            }
            forge_asm_symtab_add(res, name_buf, val, line, true);
        }
        return;
    }

    if (str_icmp(directive, ".word") == 0 || str_icmp(directive, ".dw") == 0) {
        if (op_count == 0U) {
            if (is_pass2) {
                forge_asm_add_error(res, line, 1U, "%s requires at least 1 value", directive);
            }
            return;
        }
        *has_emitted = true;
        for (size_t i = 0U; i < op_count; i++) {
            uint16_t w = 0U;
            if (is_pass2 && !forge_asm_resolve_value(res, ops[i], &w)) {
                forge_asm_add_error(res, line, 1U, "Undefined symbol or invalid word literal '%s'", ops[i]);
                return;
            }
            emit_word16(res, curr_addr, w, is_pass2, line);
        }
        return;
    }

    if (str_icmp(directive, ".byte") == 0 || str_icmp(directive, ".db") == 0) {
        if (op_count == 0U) {
            if (is_pass2) {
                forge_asm_add_error(res, line, 1U, "%s requires at least 1 value", directive);
            }
            return;
        }
        *has_emitted = true;
        for (size_t i = 0U; i < op_count; i++) {
            int32_t num = 0;
            uint16_t resolved = 0U;
            if (is_pass2) {
                if (forge_asm_parse_number(ops[i], &num)) {
                    if (num < -128 || num > 255) {
                        forge_asm_add_error(res, line, 1U, "Byte literal '%s' out of 8-bit range", ops[i]);
                        return;
                    }
                    resolved = (uint16_t)(num & 0xFF);
                } else if (forge_asm_resolve_value(res, ops[i], &resolved)) {
                    if (resolved > 255U) {
                        forge_asm_add_error(res, line, 1U, "Symbol '%s' value %u exceeds 8-bit byte range", ops[i], resolved);
                        return;
                    }
                } else {
                    forge_asm_add_error(res, line, 1U, "Invalid byte operand '%s'", ops[i]);
                    return;
                }
            }
            emit_byte(res, curr_addr, (uint8_t)(resolved & 0xFFU), is_pass2, line);
        }
        return;
    }

    if (str_icmp(directive, ".asciz") == 0 || str_icmp(directive, ".string") == 0) {
        char *trimmed = trim_whitespace(operands_str);
        uint8_t str_bytes[512];
        size_t str_len = 0U;
        if (!parse_quoted_string(res, trimmed, line, str_bytes, sizeof(str_bytes), &str_len)) {
            return;
        }
        *has_emitted = true;
        for (size_t i = 0U; i < str_len; i++) {
            emit_byte(res, curr_addr, str_bytes[i], is_pass2, line);
        }
        emit_byte(res, curr_addr, 0U, is_pass2, line);
        return;
    }

    if (str_icmp(directive, ".space") == 0 || str_icmp(directive, ".resb") == 0) {
        if (op_count != 1U) {
            if (is_pass2) {
                forge_asm_add_error(res, line, 1U, "%s expects 1 byte count operand", directive);
            }
            return;
        }
        uint16_t count = 0U;
        if (!forge_asm_resolve_value(res, ops[0], &count)) {
            if (is_pass2) {
                forge_asm_add_error(res, line, 1U, "Invalid byte count '%s' in %s", ops[0], directive);
            }
            return;
        }
        *has_emitted = true;
        for (uint16_t i = 0U; i < count; i++) {
            emit_byte(res, curr_addr, 0U, is_pass2, line);
        }
        return;
    }

    if (!is_pass2) {
        forge_asm_add_error(res, line, 1U, "Unknown assembler directive '%s'", directive);
    }
}

static void process_instruction(
    forge_asm_result_t *res,
    const char *mnemonic,
    char *operands_str,
    uint32_t *curr_addr,
    bool *has_emitted,
    bool is_pass2,
    uint32_t line
) {
    forge_opcode_t opcode = FORGE_OP_NOP;
    if (!forge_asm_parse_opcode(mnemonic, &opcode)) {
        if (!is_pass2) {
            forge_asm_add_error(res, line, 1U, "Unknown instruction mnemonic '%s'", mnemonic);
        }
        return;
    }

    *has_emitted = true;
    uint16_t instr_addr = (uint16_t)(*curr_addr);

    if (!is_pass2) {
        *curr_addr += FORGE_INSTRUCTION_BYTES;
        return;
    }

    char *ops[8];
    bool trailing_comma = false;
    size_t op_count = split_operands(operands_str, ops, 8U, &trailing_comma);
    if (trailing_comma) {
        forge_asm_add_error(res, line, 1U, "Trailing comma in '%s' instruction", mnemonic);
        return;
    }

    forge_addr_mode_t mode = FORGE_MODE_REG;
    uint8_t rd = 0U;
    uint8_t rs = 0U;
    uint16_t imm16 = 0U;

    switch (opcode) {
        case FORGE_OP_NOP:
        case FORGE_OP_HALT:
        case FORGE_OP_RET:
            if (op_count != 0U) {
                forge_asm_add_error(res, line, 1U, "'%s' takes no operands", mnemonic);
                return;
            }
            break;

        case FORGE_OP_NOT:
        case FORGE_OP_POP:
            if (op_count != 1U) {
                forge_asm_add_error(res, line, 1U, "'%s' expects 1 register operand", mnemonic);
                return;
            }
            if (!forge_asm_parse_reg(ops[0], &rd)) {
                forge_asm_add_error(res, line, 1U, "Invalid register '%s' for '%s'", ops[0], mnemonic);
                return;
            }
            mode = FORGE_MODE_REG;
            break;

        case FORGE_OP_PUSH:
            if (op_count != 1U) {
                forge_asm_add_error(res, line, 1U, "'PUSH' expects 1 register or immediate operand");
                return;
            }
            if (forge_asm_parse_reg(ops[0], &rd)) {
                mode = FORGE_MODE_REG;
            } else if (forge_asm_resolve_value(res, ops[0], &imm16)) {
                mode = FORGE_MODE_IMM;
            } else {
                forge_asm_add_error(res, line, 1U, "Invalid operand '%s' for 'PUSH'", ops[0]);
                return;
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
            if (op_count != 1U) {
                forge_asm_add_error(res, line, 1U, "'%s' expects 1 target address, label, or register operand", mnemonic);
                return;
            }
            if (forge_asm_parse_reg(ops[0], &rs)) {
                mode = FORGE_MODE_REG;
            } else if (forge_asm_resolve_value(res, ops[0], &imm16)) {
                mode = FORGE_MODE_IMM;
            } else {
                forge_asm_add_error(res, line, 1U, "Undefined label or invalid target '%s' for '%s'", ops[0], mnemonic);
                return;
            }
            break;

        case FORGE_OP_IN:
        case FORGE_OP_OUT:
            if (op_count != 1U && op_count != 2U) {
                forge_asm_add_error(res, line, 1U, "'%s' expects 1 or 2 operands (Rd [, port])", mnemonic);
                return;
            }
            if (!forge_asm_parse_reg(ops[0], &rd)) {
                forge_asm_add_error(res, line, 1U, "First operand of '%s' must be a register, got '%s'", mnemonic, ops[0]);
                return;
            }
            if (op_count == 1U) {
                mode = FORGE_MODE_IMM;
                imm16 = FORGE_PORT_CONSOLE_INT;
            } else {
                if (forge_asm_parse_reg(ops[1], &rs)) {
                    mode = FORGE_MODE_REG;
                } else if (forge_asm_resolve_value(res, ops[1], &imm16)) {
                    if (imm16 >= FORGE_IO_PORT_COUNT) {
                        forge_asm_add_error(res, line, 1U, "I/O port %u out of range (0..255)", imm16);
                        return;
                    }
                    mode = FORGE_MODE_IMM;
                } else {
                    forge_asm_add_error(res, line, 1U, "Invalid I/O port operand '%s'", ops[1]);
                    return;
                }
            }
            break;

        case FORGE_OP_LOAD:
        case FORGE_OP_STORE:
            if (op_count != 2U) {
                forge_asm_add_error(res, line, 1U, "'%s' expects 2 operands: %s Rd, [addr]", mnemonic, mnemonic);
                return;
            }
            if (!forge_asm_parse_reg(ops[0], &rd)) {
                forge_asm_add_error(res, line, 1U, "First operand of '%s' must be a register, got '%s'", mnemonic, ops[0]);
                return;
            }
            if (!parse_memory_operand(res, ops[1], line, &mode, &rs, &imm16)) {
                return;
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
            if (op_count != 2U) {
                forge_asm_add_error(res, line, 1U, "'%s' expects 2 operands: %s Rd, Rs_or_imm", mnemonic, mnemonic);
                return;
            }
            if (!forge_asm_parse_reg(ops[0], &rd)) {
                forge_asm_add_error(res, line, 1U, "Destination operand of '%s' must be a register, got '%s'", mnemonic, ops[0]);
                return;
            }
            if (forge_asm_parse_reg(ops[1], &rs)) {
                mode = FORGE_MODE_REG;
            } else if (forge_asm_resolve_value(res, ops[1], &imm16)) {
                mode = FORGE_MODE_IMM;
            } else {
                forge_asm_add_error(res, line, 1U, "Undefined symbol or invalid source operand '%s' for '%s'", ops[1], mnemonic);
                return;
            }
            break;

        default:
            forge_asm_add_error(res, line, 1U, "Unhandled opcode '%s'", mnemonic);
            return;
    }

    uint16_t w0 = 0U;
    uint16_t w1 = 0U;
    forge_encode_instruction(opcode, mode, rd, rs, imm16, &w0, &w1);

    emit_word16(res, curr_addr, w0, true, line);
    emit_word16(res, curr_addr, w1, true, line);

    if (res->line_map_count < FORGE_ASM_MAX_LINE_MAP) {
        forge_asm_line_map_t *lm = &res->line_map[res->line_map_count++];
        lm->address = instr_addr;
        lm->line    = line;
        lm->word0   = w0;
        lm->word1   = w1;
    }
}

static void run_pass(
    forge_asm_result_t *res,
    const char *source,
    bool is_pass2,
    bool *has_explicit_entry
) {
    uint32_t curr_addr = res->load_addr;
    bool has_emitted = false;
    uint32_t line_num = 0U;

    const char *line_start = source;
    while (*line_start != '\0') {
        line_num++;
        const char *line_end = strchr(line_start, '\n');
        size_t line_len = (line_end != NULL)
                              ? (size_t)(line_end - line_start)
                              : strlen(line_start);

        char line_buf[512];
        if (line_len >= sizeof(line_buf)) {
            if (!is_pass2) {
                forge_asm_add_error(res, line_num, 1U, "Source line exceeds maximum length (511 chars)");
            }
            line_start = (line_end != NULL) ? (line_end + 1) : (line_start + line_len);
            continue;
        }

        memcpy(line_buf, line_start, line_len);
        line_buf[line_len] = '\0';
        line_start = (line_end != NULL) ? (line_end + 1) : (line_start + line_len);

        strip_comment(line_buf);
        char *cur = trim_whitespace(line_buf);
        if (*cur == '\0') {
            continue;
        }

        /* Extract any leading label definitions: "label:" */
        while (true) {
            char *colon = NULL;
            bool in_sq = false;
            bool in_dq = false;
            for (char *p = cur; *p != '\0'; p++) {
                if (*p == '\'' && !in_dq) in_sq = !in_sq;
                else if (*p == '"' && !in_sq) in_dq = !in_dq;
                else if (!in_sq && !in_dq && *p == ':') {
                    colon = p;
                    break;
                }
            }
            if (colon == NULL) {
                break;
            }
            *colon = '\0';
            char *label_name = trim_whitespace(cur);
            if (!is_pass2) {
                forge_asm_symtab_add(res, label_name, (uint16_t)curr_addr, line_num, false);
            }
            cur = trim_whitespace(colon + 1);
            if (*cur == '\0') {
                break;
            }
        }

        if (*cur == '\0') {
            continue;
        }

        /* Split first token (mnemonic or directive) from operands */
        char first_tok[64];
        size_t ti = 0U;
        while (cur[ti] != '\0' && !isspace((unsigned char)cur[ti]) && ti + 1U < sizeof(first_tok)) {
            first_tok[ti] = cur[ti];
            ti++;
        }
        first_tok[ti] = '\0';
        char *operands_str = trim_whitespace(cur + ti);

        if (first_tok[0] == '.') {
            process_directive(
                res,
                first_tok,
                operands_str,
                &curr_addr,
                &has_emitted,
                has_explicit_entry,
                is_pass2,
                line_num
            );
        } else {
            process_instruction(
                res,
                first_tok,
                operands_str,
                &curr_addr,
                &has_emitted,
                is_pass2,
                line_num
            );
        }
    }
}

static void build_fbin_image(forge_asm_result_t *res) {
    uint16_t checksum = 0U;
    for (uint32_t i = 0U; i < res->code_size; i++) {
        checksum = (uint16_t)(checksum + res->code[i]);
    }

    res->fbin[0] = FORGE_FBIN_MAGIC_0;
    res->fbin[1] = FORGE_FBIN_MAGIC_1;
    res->fbin[2] = FORGE_FBIN_MAGIC_2;
    res->fbin[3] = FORGE_FBIN_MAGIC_3;

    uint16_t version = FORGE_FBIN_VERSION;
    res->fbin[4] = (uint8_t)(version & 0xFFU);
    res->fbin[5] = (uint8_t)((version >> 8) & 0xFFU);

    res->fbin[6] = (uint8_t)(res->load_addr & 0xFFU);
    res->fbin[7] = (uint8_t)((res->load_addr >> 8) & 0xFFU);

    res->fbin[8] = (uint8_t)(res->entry_pc & 0xFFU);
    res->fbin[9] = (uint8_t)((res->entry_pc >> 8) & 0xFFU);

    uint16_t csize16 = (uint16_t)(res->code_size & 0xFFFFU);
    res->fbin[10] = (uint8_t)(csize16 & 0xFFU);
    res->fbin[11] = (uint8_t)((csize16 >> 8) & 0xFFU);

    /* Flags (0) */
    res->fbin[12] = 0U;
    res->fbin[13] = 0U;

    /* Checksum */
    res->fbin[14] = (uint8_t)(checksum & 0xFFU);
    res->fbin[15] = (uint8_t)((checksum >> 8) & 0xFFU);

    if (res->code_size > 0U) {
        memcpy(&res->fbin[FORGE_FBIN_HEADER_SIZE], res->code, res->code_size);
    }
    res->fbin_size = FORGE_FBIN_HEADER_SIZE + res->code_size;
}

bool forge_assemble_source(const char *source, forge_asm_result_t *out_result) {
    if (out_result == NULL) {
        return false;
    }
    forge_asm_result_init(out_result);
    if (source == NULL) {
        forge_asm_add_error(out_result, 1U, 1U, "Source string is NULL");
        return false;
    }

    bool has_explicit_entry = false;
    /* Pass 1: Collect symbol definitions and compute addresses */
    run_pass(out_result, source, false, &has_explicit_entry);
    if (out_result->error_count > 0U) {
        out_result->success = false;
        return false;
    }

    /* Pass 2: Resolve symbols/forward references and emit machine code */
    run_pass(out_result, source, true, &has_explicit_entry);
    if (out_result->error_count > 0U) {
        out_result->success = false;
        return false;
    }

    /* If no explicit .entry directive was given, check for _start or main label */
    if (!has_explicit_entry) {
        const forge_asm_symbol_t *start_sym = forge_asm_find_symbol(out_result, "_start");
        if (start_sym == NULL) {
            start_sym = forge_asm_find_symbol(out_result, "main");
        }
        if (start_sym != NULL && !start_sym->is_constant) {
            out_result->entry_pc = start_sym->value;
        }
    }

    build_fbin_image(out_result);
    out_result->success = true;
    return true;
}

bool forge_assemble_file(const char *filepath, forge_asm_result_t *out_result) {
    if (out_result == NULL) {
        return false;
    }
    forge_asm_result_init(out_result);
    if (filepath == NULL) {
        forge_asm_add_error(out_result, 1U, 1U, "Filepath is NULL");
        return false;
    }

    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        forge_asm_add_error(out_result, 1U, 1U, "Failed to open source file '%s'", filepath);
        return false;
    }
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        forge_asm_add_error(out_result, 1U, 1U, "Failed to seek in file '%s'", filepath);
        return false;
    }
    long fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        forge_asm_add_error(out_result, 1U, 1U, "Failed to determine file size of '%s'", filepath);
        return false;
    }
    rewind(fp);

    char *buf = (char *)malloc((size_t)fsize + 1U);
    if (buf == NULL) {
        fclose(fp);
        forge_asm_add_error(out_result, 1U, 1U, "Out of memory reading '%s'", filepath);
        return false;
    }
    size_t nread = fread(buf, 1U, (size_t)fsize, fp);
    fclose(fp);
    buf[nread] = '\0';

    bool ok = forge_assemble_source(buf, out_result);
    free(buf);
    return ok;
}

bool forge_asm_write_fbin(const forge_asm_result_t *res, const char *filepath) {
    if (res == NULL || !res->success || filepath == NULL) {
        return false;
    }
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        return false;
    }
    size_t written = fwrite(res->fbin, 1U, res->fbin_size, fp);
    fclose(fp);
    return written == res->fbin_size;
}

bool forge_asm_write_raw(const forge_asm_result_t *res, const char *filepath) {
    if (res == NULL || !res->success || filepath == NULL) {
        return false;
    }
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        return false;
    }
    size_t written = fwrite(res->code, 1U, res->code_size, fp);
    fclose(fp);
    return written == res->code_size;
}

