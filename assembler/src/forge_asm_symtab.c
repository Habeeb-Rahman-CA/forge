#include "forge_asm.h"
#include <ctype.h>
#include <string.h>

const forge_asm_symbol_t *forge_asm_find_symbol(
    const forge_asm_result_t *res,
    const char *name
) {
    if (res == NULL || name == NULL) {
        return NULL;
    }
    for (size_t i = 0U; i < res->symbol_count; i++) {
        if (strcmp(res->symbols[i].name, name) == 0) {
            return &res->symbols[i];
        }
    }
    return NULL;
}

bool forge_asm_symtab_add(
    forge_asm_result_t *res,
    const char *name,
    uint16_t value,
    uint32_t line,
    bool is_constant
) {
    if (res == NULL || name == NULL) {
        return false;
    }
    if (!forge_asm_is_valid_ident(name)) {
        forge_asm_add_error(res, line, 1U, "Invalid symbol or label name '%s'", name);
        return false;
    }

    const forge_asm_symbol_t *existing = forge_asm_find_symbol(res, name);
    if (existing != NULL) {
        forge_asm_add_error(
            res,
            line,
            1U,
            "Duplicate symbol '%s' (previously defined on line %u)",
            name,
            existing->line
        );
        return false;
    }

    if (res->symbol_count >= FORGE_ASM_MAX_SYMBOLS) {
        forge_asm_add_error(res, line, 1U, "Symbol table capacity (%u) exceeded", FORGE_ASM_MAX_SYMBOLS);
        return false;
    }

    forge_asm_symbol_t *sym = &res->symbols[res->symbol_count++];
    strncpy(sym->name, name, FORGE_ASM_MAX_NAME_LEN - 1U);
    sym->name[FORGE_ASM_MAX_NAME_LEN - 1U] = '\0';
    sym->value = value;
    sym->line = line;
    sym->is_constant = is_constant;
    return true;
}

bool forge_asm_resolve_value(
    const forge_asm_result_t *res,
    const char *token,
    uint16_t *out_val
) {
    if (res == NULL || token == NULL || out_val == NULL) {
        return false;
    }

    int32_t num = 0;
    if (forge_asm_parse_number(token, &num)) {
        *out_val = (uint16_t)((uint32_t)num & 0xFFFFU);
        return true;
    }

    const forge_asm_symbol_t *sym = forge_asm_find_symbol(res, token);
    if (sym != NULL) {
        *out_val = sym->value;
        return true;
    }

    /* Support simple symbol +/- offset expressions, e.g. "table + 4" or "table-2" */
    const char *op_pos = NULL;
    for (const char *p = token + 1; *p != '\0'; p++) {
        if ((*p == '+' || *p == '-') && token[0] != '\'') {
            op_pos = p;
            break;
        }
    }

    if (op_pos != NULL) {
        char lhs[FORGE_ASM_MAX_NAME_LEN];
        char rhs[FORGE_ASM_MAX_NAME_LEN];

        size_t lhs_len = (size_t)(op_pos - token);
        while (lhs_len > 0U && isspace((unsigned char)token[lhs_len - 1U])) {
            lhs_len--;
        }
        if (lhs_len == 0U || lhs_len >= sizeof(lhs)) {
            return false;
        }
        memcpy(lhs, token, lhs_len);
        lhs[lhs_len] = '\0';

        const char *rhs_start = op_pos + 1;
        while (*rhs_start != '\0' && isspace((unsigned char)*rhs_start)) {
            rhs_start++;
        }
        strncpy(rhs, rhs_start, sizeof(rhs) - 1U);
        rhs[sizeof(rhs) - 1U] = '\0';

        uint16_t base_val = 0U;
        int32_t offset_val = 0;
        const forge_asm_symbol_t *base_sym = forge_asm_find_symbol(res, lhs);
        if (base_sym != NULL && forge_asm_parse_number(rhs, &offset_val)) {
            if (*op_pos == '+') {
                base_val = (uint16_t)(base_sym->value + (uint16_t)offset_val);
            } else {
                base_val = (uint16_t)(base_sym->value - (uint16_t)offset_val);
            }
            *out_val = base_val;
            return true;
        }
    }

    return false;
}

