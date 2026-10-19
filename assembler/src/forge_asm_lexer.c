#include "forge_asm.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
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

bool forge_asm_parse_reg(const char *token, uint8_t *out_reg) {
    if (token == NULL || out_reg == NULL) {
        return false;
    }
    if (str_icmp(token, "FP") == 0) {
        *out_reg = FORGE_FP;
        return true;
    }
    if ((token[0] == 'R' || token[0] == 'r') &&
        token[1] >= '0' && token[1] <= '7' &&
        token[2] == '\0') {
        *out_reg = (uint8_t)(token[1] - '0');
        return true;
    }
    return false;
}

bool forge_asm_parse_opcode(const char *token, forge_opcode_t *out_opcode) {
    if (token == NULL || out_opcode == NULL) {
        return false;
    }
    for (uint8_t op = 0U; op < (uint8_t)FORGE_OP_COUNT; op++) {
        const char *name = forge_opcode_name((forge_opcode_t)op);
        if (str_icmp(token, name) == 0) {
            *out_opcode = (forge_opcode_t)op;
            return true;
        }
    }
    return false;
}

bool forge_asm_parse_number(const char *token, int32_t *out_val) {
    if (token == NULL || token[0] == '\0' || out_val == NULL) {
        return false;
    }

    /* Character literal: 'A' or '\n' */
    size_t len = strlen(token);
    if (len >= 3U && token[0] == '\'' && token[len - 1U] == '\'') {
        if (len == 3U && token[1] != '\\') {
            *out_val = (int32_t)(uint8_t)token[1];
            return true;
        }
        if (len == 4U && token[1] == '\\') {
            switch (token[2]) {
                case 'n':  *out_val = '\n'; return true;
                case 't':  *out_val = '\t'; return true;
                case 'r':  *out_val = '\r'; return true;
                case '0':  *out_val = '\0'; return true;
                case '\\': *out_val = '\\'; return true;
                case '\'': *out_val = '\''; return true;
                default:   return false;
            }
        }
        return false;
    }

    const char *p = token;
    int32_t sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    if (*p == '\0') {
        return false;
    }

    /* Binary literal: 0b... or 0B... */
    if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
        p += 2;
        if (*p == '\0') {
            return false;
        }
        int32_t accum = 0;
        while (*p != '\0') {
            if (*p != '0' && *p != '1') {
                return false;
            }
            accum = (accum << 1) | (*p - '0');
            if (accum > 0xFFFF) {
                return false;
            }
            p++;
        }
        *out_val = sign * accum;
        return (*out_val >= -32768 && *out_val <= 65535);
    }

    /* Hexadecimal or Decimal */
    char *endptr = NULL;
    errno = 0;
    long parsed = strtol(p, &endptr, 0);
    if (errno != 0 || endptr == p || *endptr != '\0') {
        return false;
    }

    long signed_val = (long)sign * parsed;
    if (signed_val < -32768L || signed_val > 65535L) {
        return false;
    }

    *out_val = (int32_t)signed_val;
    return true;
}

bool forge_asm_is_valid_ident(const char *token) {
    if (token == NULL || token[0] == '\0') {
        return false;
    }
    if (!(isalpha((unsigned char)token[0]) || token[0] == '_')) {
        return false;
    }
    for (size_t i = 1U; token[i] != '\0'; i++) {
        if (!(isalnum((unsigned char)token[i]) || token[i] == '_')) {
            return false;
        }
    }
    uint8_t dummy_reg = 0U;
    if (forge_asm_parse_reg(token, &dummy_reg)) {
        return false;
    }
    forge_opcode_t dummy_op = FORGE_OP_NOP;
    if (forge_asm_parse_opcode(token, &dummy_op)) {
        return false;
    }
    return true;
}

void forge_asm_add_error(
    forge_asm_result_t *res,
    uint32_t line,
    uint32_t col,
    const char *fmt,
    ...
) {
    if (res == NULL || fmt == NULL) {
        return;
    }
    res->success = false;
    if (res->error_count >= FORGE_ASM_MAX_ERRORS) {
        return;
    }
    forge_asm_error_t *err = &res->errors[res->error_count++];
    err->line = line;
    err->column = col;

    va_list args;
    va_start(args, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, args);
    va_end(args);
}

