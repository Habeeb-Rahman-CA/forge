#ifndef FORGE_ASM_H
#define FORGE_ASM_H

#include "forge_cpu.h"
#include "forge_isa.h"
#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FORGE_ASM_MAX_SYMBOLS     512U
#define FORGE_ASM_MAX_NAME_LEN    64U
#define FORGE_ASM_MAX_ERRORS      64U
#define FORGE_ASM_MAX_ERROR_MSG   160U
#define FORGE_ASM_MAX_LINE_MAP    4096U
#define FORGE_ASM_MAX_CODE_SIZE   65536U
#define FORGE_ASM_MAX_FBIN_SIZE   (FORGE_FBIN_HEADER_SIZE + FORGE_ASM_MAX_CODE_SIZE)

/* Symbol Table Entry */
typedef struct {
    char name[FORGE_ASM_MAX_NAME_LEN];
    uint16_t value;       /* 16-bit address (for labels) or constant value (for .equ) */
    uint32_t line;        /* 1-based line number where defined */
    bool is_constant;     /* true if defined by .equ/.const, false if address label */
} forge_asm_symbol_t;

/* Source Line to Machine Address Mapping Entry */
typedef struct {
    uint16_t address;     /* Memory address of emitted instruction */
    uint32_t line;        /* 1-based source line number */
    uint16_t word0;       /* First 16-bit instruction word */
    uint16_t word1;       /* Second 16-bit instruction word (imm16) */
} forge_asm_line_map_t;

/* Structured Assembly Diagnostic Error */
typedef struct {
    uint32_t line;        /* 1-based line number */
    uint32_t column;      /* 1-based column number */
    char message[FORGE_ASM_MAX_ERROR_MSG];
} forge_asm_error_t;

/* Complete Result of an Assembly Invocation */
typedef struct {
    bool success;

    /* Load and entry addresses */
    uint16_t load_addr;
    uint16_t entry_pc;

    /* Raw machine-code / data payload */
    uint8_t code[FORGE_ASM_MAX_CODE_SIZE];
    uint32_t code_size;

    /* Complete .fbin executable image (16-byte header + payload) */
    uint8_t fbin[FORGE_ASM_MAX_FBIN_SIZE];
    uint32_t fbin_size;

    /* Symbol table */
    forge_asm_symbol_t symbols[FORGE_ASM_MAX_SYMBOLS];
    size_t symbol_count;

    /* Line map for debugging & disassembly */
    forge_asm_line_map_t line_map[FORGE_ASM_MAX_LINE_MAP];
    size_t line_map_count;

    /* Syntax / semantic errors */
    forge_asm_error_t errors[FORGE_ASM_MAX_ERRORS];
    size_t error_count;
} forge_asm_result_t;

/* Initialize/clear a forge_asm_result_t structure */
void forge_asm_result_init(forge_asm_result_t *res);

/* Assemble an in-memory assembly source string */
bool forge_assemble_source(const char *source, forge_asm_result_t *out_result);

/* Assemble an assembly file from disk */
bool forge_assemble_file(const char *filepath, forge_asm_result_t *out_result);

/* Lookup a symbol by name in an assembly result */
const forge_asm_symbol_t *forge_asm_find_symbol(
    const forge_asm_result_t *res,
    const char *name
);

/* Write the .fbin executable image from a successful result to a file */
bool forge_asm_write_fbin(const forge_asm_result_t *res, const char *filepath);

/* Write the raw machine-code payload from a successful result to a file */
bool forge_asm_write_raw(const forge_asm_result_t *res, const char *filepath);

/* Disassemble a binary buffer (.fbin or raw machine code) into a text buffer */
forge_error_t forge_disassemble_binary(
    const uint8_t *bytes,
    size_t len,
    uint16_t default_load_addr,
    char *out_buf,
    size_t out_buf_size
);

/* Internal lexer/parser helpers exposed across assembler translation units */
bool forge_asm_parse_reg(const char *token, uint8_t *out_reg);
bool forge_asm_parse_opcode(const char *token, forge_opcode_t *out_opcode);
bool forge_asm_parse_number(const char *token, int32_t *out_val);
bool forge_asm_is_valid_ident(const char *token);

void forge_asm_add_error(
    forge_asm_result_t *res,
    uint32_t line,
    uint32_t col,
    const char *fmt,
    ...
);

bool forge_asm_symtab_add(
    forge_asm_result_t *res,
    const char *name,
    uint16_t value,
    uint32_t line,
    bool is_constant
);

bool forge_asm_resolve_value(
    const forge_asm_result_t *res,
    const char *token,
    uint16_t *out_val
);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_ASM_H */

