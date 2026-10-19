#include "forge_compiler.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void forge_compile_result_init(forge_compile_result_t *res) {
    if (res == NULL) {
        return;
    }
    memset(res, 0, sizeof(*res));
    forge_asm_result_init(&res->asm_result);
}

void forge_comp_add_error(
    forge_compile_result_t *res,
    const char *stage,
    uint32_t line,
    uint32_t col,
    const char *fmt,
    ...
) {
    if (res == NULL || fmt == NULL) {
        return;
    }
    res->success = false;
    if (res->error_count >= FORGE_COMP_MAX_ERRORS) {
        return;
    }
    forge_comp_error_t *err = &res->errors[res->error_count++];
    err->line = line;
    err->column = col;
    (void)snprintf(err->stage, sizeof(err->stage), "%s", stage != NULL ? stage : "compiler");

    va_list args;
    va_start(args, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, args);
    va_end(args);
}

static bool push_token(
    forge_compile_result_t *res,
    forge_token_type_t type,
    const char *text,
    int32_t int_val,
    uint32_t line,
    uint32_t col
) {
    if (res->tokens.count >= FORGE_COMP_MAX_TOKENS) {
        forge_comp_add_error(res, "lexer", line, col, "Token limit (%u) exceeded", FORGE_COMP_MAX_TOKENS);
        return false;
    }
    forge_token_t *tok = &res->tokens.tokens[res->tokens.count++];
    tok->type = type;
    (void)snprintf(tok->text, sizeof(tok->text), "%s", text != NULL ? text : "");
    tok->int_val = int_val;
    tok->line = line;
    tok->column = col;
    return true;
}

static forge_token_type_t classify_keyword(const char *ident) {
    if (strcmp(ident, "let") == 0)      return FORGE_TOK_LET;
    if (strcmp(ident, "fn") == 0)       return FORGE_TOK_FN;
    if (strcmp(ident, "return") == 0)   return FORGE_TOK_RETURN;
    if (strcmp(ident, "if") == 0)       return FORGE_TOK_IF;
    if (strcmp(ident, "else") == 0)     return FORGE_TOK_ELSE;
    if (strcmp(ident, "while") == 0)    return FORGE_TOK_WHILE;
    if (strcmp(ident, "for") == 0)      return FORGE_TOK_FOR;
    if (strcmp(ident, "break") == 0)    return FORGE_TOK_BREAK;
    if (strcmp(ident, "continue") == 0) return FORGE_TOK_CONTINUE;
    if (strcmp(ident, "print") == 0)    return FORGE_TOK_PRINT;
    if (strcmp(ident, "putc") == 0)     return FORGE_TOK_PUTC;
    if (strcmp(ident, "in") == 0)       return FORGE_TOK_IN;
    return FORGE_TOK_IDENT;
}

bool forge_lex_source(const char *source, forge_compile_result_t *res) {
    if (res == NULL) {
        return false;
    }
    if (source == NULL) {
        forge_comp_add_error(res, "lexer", 1U, 1U, "Source string is NULL");
        return false;
    }

    res->tokens.count = 0U;
    uint32_t line = 1U;
    uint32_t col  = 1U;
    size_t i = 0U;

    while (source[i] != '\0') {
        char c = source[i];

        /* Whitespace and newlines */
        if (c == '\n') {
            line++;
            col = 1U;
            i++;
            continue;
        }
        if (isspace((unsigned char)c)) {
            col++;
            i++;
            continue;
        }

        /* Single-line comments: // or # */
        if ((c == '/' && source[i + 1U] == '/') || c == '#') {
            while (source[i] != '\0' && source[i] != '\n') {
                i++;
            }
            continue;
        }

        uint32_t start_col = col;

        /* Character literal: 'A' or '\n' */
        if (c == '\'') {
            size_t start = i++;
            col++;
            int32_t ch_val = 0;
            if (source[i] == '\\') {
                i++;
                col++;
                switch (source[i]) {
                    case 'n':  ch_val = '\n'; break;
                    case 't':  ch_val = '\t'; break;
                    case 'r':  ch_val = '\r'; break;
                    case '0':  ch_val = '\0'; break;
                    case '\\': ch_val = '\\'; break;
                    case '\'': ch_val = '\''; break;
                    default:
                        forge_comp_add_error(res, "lexer", line, start_col, "Invalid character escape '\\%c'", source[i]);
                        return false;
                }
                i++;
                col++;
            } else if (source[i] != '\0' && source[i] != '\'') {
                ch_val = (int32_t)(uint8_t)source[i];
                i++;
                col++;
            } else {
                forge_comp_add_error(res, "lexer", line, start_col, "Empty or unterminated character literal");
                return false;
            }
            if (source[i] != '\'') {
                forge_comp_add_error(res, "lexer", line, start_col, "Unterminated character literal");
                return false;
            }
            i++;
            col++;
            char lit_buf[16];
            size_t len = i - start;
            if (len >= sizeof(lit_buf)) len = sizeof(lit_buf) - 1U;
            memcpy(lit_buf, &source[start], len);
            lit_buf[len] = '\0';
            if (!push_token(res, FORGE_TOK_NUMBER, lit_buf, ch_val, line, start_col)) {
                return false;
            }
            continue;
        }

        /* Numeric literals (decimal or 0x hex) */
        if (isdigit((unsigned char)c)) {
            size_t start = i;
            if (c == '0' && (source[i + 1U] == 'x' || source[i + 1U] == 'X')) {
                i += 2U;
                col += 2U;
                if (!isxdigit((unsigned char)source[i])) {
                    forge_comp_add_error(res, "lexer", line, start_col, "Invalid hexadecimal literal");
                    return false;
                }
                while (isxdigit((unsigned char)source[i])) {
                    i++;
                    col++;
                }
            } else {
                while (isdigit((unsigned char)source[i])) {
                    i++;
                    col++;
                }
            }
            if (isalpha((unsigned char)source[i]) || source[i] == '_') {
                forge_comp_add_error(res, "lexer", line, start_col, "Invalid trailing characters in numeric literal");
                return false;
            }
            char num_buf[64];
            size_t len = i - start;
            if (len >= sizeof(num_buf)) len = sizeof(num_buf) - 1U;
            memcpy(num_buf, &source[start], len);
            num_buf[len] = '\0';

            long val = strtol(num_buf, NULL, 0);
            if (val < 0L || val > 65535L) {
                forge_comp_add_error(res, "lexer", line, start_col, "Numeric literal '%s' exceeds 16-bit range", num_buf);
                return false;
            }
            if (!push_token(res, FORGE_TOK_NUMBER, num_buf, (int32_t)val, line, start_col)) {
                return false;
            }
            continue;
        }

        /* Identifiers and keywords */
        if (isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (isalnum((unsigned char)source[i]) || source[i] == '_') {
                i++;
                col++;
            }
            char id_buf[64];
            size_t len = i - start;
            if (len >= sizeof(id_buf)) {
                forge_comp_add_error(res, "lexer", line, start_col, "Identifier exceeds maximum length (63)");
                return false;
            }
            memcpy(id_buf, &source[start], len);
            id_buf[len] = '\0';

            forge_token_type_t kw_type = classify_keyword(id_buf);
            if (!push_token(res, kw_type, id_buf, 0, line, start_col)) {
                return false;
            }
            continue;
        }

        /* Two-character operators: ==, !=, <=, >=, <<, >>, &&, || */
        char next = source[i + 1U];
        if (c == '=' && next == '=') {
            push_token(res, FORGE_TOK_EQ, "==", 0, line, start_col);
            i += 2U; col += 2U; continue;
        }
        if (c == '!' && next == '=') {
            push_token(res, FORGE_TOK_NEQ, "!=", 0, line, start_col);
            i += 2U; col += 2U; continue;
        }
        if (c == '<' && next == '=') {
            push_token(res, FORGE_TOK_LTE, "<=", 0, line, start_col);
            i += 2U; col += 2U; continue;
        }
        if (c == '>' && next == '=') {
            push_token(res, FORGE_TOK_GTE, ">=", 0, line, start_col);
            i += 2U; col += 2U; continue;
        }
        if (c == '<' && next == '<') {
            push_token(res, FORGE_TOK_SHL, "<<", 0, line, start_col);
            i += 2U; col += 2U; continue;
        }
        if (c == '>' && next == '>') {
            push_token(res, FORGE_TOK_SHR, ">>", 0, line, start_col);
            i += 2U; col += 2U; continue;
        }
        if (c == '&' && next == '&') {
            push_token(res, FORGE_TOK_LAND, "&&", 0, line, start_col);
            i += 2U; col += 2U; continue;
        }
        if (c == '|' && next == '|') {
            push_token(res, FORGE_TOK_LOR, "||", 0, line, start_col);
            i += 2U; col += 2U; continue;
        }

        /* Single-character operators and delimiters */
        forge_token_type_t single_type = FORGE_TOK_EOF;
        char single_str[2] = { c, '\0' };
        switch (c) {
            case '+': single_type = FORGE_TOK_PLUS;      break;
            case '-': single_type = FORGE_TOK_MINUS;     break;
            case '*': single_type = FORGE_TOK_STAR;      break;
            case '/': single_type = FORGE_TOK_SLASH;     break;
            case '%': single_type = FORGE_TOK_MOD;       break;
            case '&': single_type = FORGE_TOK_AMP;       break;
            case '|': single_type = FORGE_TOK_PIPE;      break;
            case '^': single_type = FORGE_TOK_CARET;     break;
            case '~': single_type = FORGE_TOK_TILDE;     break;
            case '!': single_type = FORGE_TOK_BANG;      break;
            case '=': single_type = FORGE_TOK_ASSIGN;    break;
            case '<': single_type = FORGE_TOK_LT;        break;
            case '>': single_type = FORGE_TOK_GT;        break;
            case '(': single_type = FORGE_TOK_LPAREN;    break;
            case ')': single_type = FORGE_TOK_RPAREN;    break;
            case '{': single_type = FORGE_TOK_LBRACE;    break;
            case '}': single_type = FORGE_TOK_RBRACE;    break;
            case ',': single_type = FORGE_TOK_COMMA;     break;
            case ';': single_type = FORGE_TOK_SEMICOLON; break;
            default:
                forge_comp_add_error(res, "lexer", line, start_col, "Unexpected character '%c'", c);
                return false;
        }

        if (!push_token(res, single_type, single_str, 0, line, start_col)) {
            return false;
        }
        i++;
        col++;
    }

    push_token(res, FORGE_TOK_EOF, "<eof>", 0, line, col);
    return res->error_count == 0U;
}

