#ifndef FORGE_COMPILER_H
#define FORGE_COMPILER_H

#include "forge_asm.h"
#include "forge_cpu.h"
#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FORGE_COMP_MAX_TOKENS       4096U
#define FORGE_COMP_MAX_AST_NODES    2048U
#define FORGE_COMP_MAX_CHILDREN     64U
#define FORGE_COMP_MAX_PARAMS       8U
#define FORGE_COMP_MAX_SYMBOLS      256U
#define FORGE_COMP_MAX_SOURCE_MAP   2048U
#define FORGE_COMP_MAX_ERRORS       64U
#define FORGE_COMP_MAX_ASM_LEN      65536U

/* 1. Lexer Token Types */
typedef enum {
    FORGE_TOK_EOF = 0,
    FORGE_TOK_NUMBER,
    FORGE_TOK_IDENT,

    /* Keywords */
    FORGE_TOK_LET,
    FORGE_TOK_FN,
    FORGE_TOK_RETURN,
    FORGE_TOK_IF,
    FORGE_TOK_ELSE,
    FORGE_TOK_WHILE,
    FORGE_TOK_FOR,
    FORGE_TOK_BREAK,
    FORGE_TOK_CONTINUE,
    FORGE_TOK_PRINT,
    FORGE_TOK_PUTC,
    FORGE_TOK_IN,

    /* Operators */
    FORGE_TOK_PLUS,       /* +  */
    FORGE_TOK_MINUS,      /* -  */
    FORGE_TOK_STAR,       /* *  */
    FORGE_TOK_SLASH,      /* /  */
    FORGE_TOK_MOD,        /* %  */
    FORGE_TOK_AMP,        /* &  */
    FORGE_TOK_PIPE,       /* |  */
    FORGE_TOK_CARET,      /* ^  */
    FORGE_TOK_TILDE,      /* ~  */
    FORGE_TOK_BANG,       /* !  */
    FORGE_TOK_LAND,       /* && */
    FORGE_TOK_LOR,        /* || */
    FORGE_TOK_SHL,        /* << */
    FORGE_TOK_SHR,        /* >> */
    FORGE_TOK_ASSIGN,     /* =  */
    FORGE_TOK_EQ,         /* == */
    FORGE_TOK_NEQ,        /* != */
    FORGE_TOK_LT,         /* <  */
    FORGE_TOK_LTE,        /* <= */
    FORGE_TOK_GT,         /* >  */
    FORGE_TOK_GTE,        /* >= */

    /* Delimiters */
    FORGE_TOK_LPAREN,     /* (  */
    FORGE_TOK_RPAREN,     /* )  */
    FORGE_TOK_LBRACE,     /* {  */
    FORGE_TOK_RBRACE,     /* }  */
    FORGE_TOK_COMMA,      /* ,  */
    FORGE_TOK_SEMICOLON   /* ;  */
} forge_token_type_t;

typedef struct {
    forge_token_type_t type;
    char text[64];
    int32_t int_val;
    uint32_t line;
    uint32_t column;
} forge_token_t;

typedef struct {
    forge_token_t tokens[FORGE_COMP_MAX_TOKENS];
    size_t count;
} forge_token_list_t;

/* 2. Abstract Syntax Tree (AST) Node Types */
typedef enum {
    FORGE_AST_PROGRAM = 0,
    FORGE_AST_BLOCK,
    FORGE_AST_VAR_DECL,     /* let name = expr */
    FORGE_AST_ASSIGN,       /* name = expr */
    FORGE_AST_PRINT,        /* print expr */
    FORGE_AST_PUTC,         /* putc expr */
    FORGE_AST_IF,           /* if cond { then_branch } else { else_branch } */
    FORGE_AST_WHILE,        /* while cond { body } */
    FORGE_AST_FOR,          /* for (init; cond; step) { body } */
    FORGE_AST_BREAK,        /* break */
    FORGE_AST_CONTINUE,     /* continue */
    FORGE_AST_FN_DECL,      /* fn name(params...) { body } */
    FORGE_AST_RETURN,       /* return expr */
    FORGE_AST_BINARY_OP,    /* left op right */
    FORGE_AST_UNARY_OP,     /* op operand */
    FORGE_AST_CALL,         /* name(args...) */
    FORGE_AST_IN_EXPR,      /* in() */
    FORGE_AST_VAR_REF,      /* variable identifier */
    FORGE_AST_NUMBER_LIT    /* numeric literal */
} forge_ast_type_t;

typedef struct forge_ast_node {
    forge_ast_type_t type;
    uint32_t line;
    uint32_t column;

    char name[64];
    forge_token_type_t op;
    int32_t int_val;

    /* Function parameter names (for FORGE_AST_FN_DECL) */
    char params[FORGE_COMP_MAX_PARAMS][64];
    size_t param_count;

    /* Child pointers */
    struct forge_ast_node *left;        /* also init_stmt for FOR */
    struct forge_ast_node *right;       /* also step_stmt for FOR */
    struct forge_ast_node *cond;
    struct forge_ast_node *then_branch;
    struct forge_ast_node *else_branch;
    struct forge_ast_node *body;

    /* Statement / argument list (for PROGRAM, BLOCK, CALL) */
    struct forge_ast_node *children[FORGE_COMP_MAX_CHILDREN];
    size_t child_count;
} forge_ast_node_t;

typedef struct {
    forge_ast_node_t nodes[FORGE_COMP_MAX_AST_NODES];
    size_t used;
    forge_ast_node_t *root;
} forge_ast_arena_t;

/* 3. Compiler Symbol Table */
typedef enum {
    FORGE_SYM_GLOBAL_VAR = 0,
    FORGE_SYM_LOCAL_VAR,
    FORGE_SYM_PARAM_VAR,
    FORGE_SYM_FUNCTION
} forge_comp_sym_kind_t;

typedef struct {
    char name[64];
    char scope_fn[64]; /* "" for global scope, function name for local/param */
    forge_comp_sym_kind_t kind;
    uint16_t global_addr; /* Address in 0x8000.. for global vars */
    int16_t fp_offset;    /* Signed offset from FP (R7) for params (+4, +6..) and locals (-2, -4..) */
    size_t param_count;   /* Number of parameters if kind == FORGE_SYM_FUNCTION */
    uint16_t local_words; /* Total local variable words allocated by function */
    uint32_t decl_line;
} forge_comp_symbol_t;

/* 4. Source Map Entry (High-Level Line <-> Assembly Line <-> Machine Address) */
typedef struct {
    uint32_t source_line;
    uint32_t asm_line;
    uint16_t address;
} forge_source_map_entry_t;

/* 5. Compiler Error Diagnostic */
typedef struct {
    uint32_t line;
    uint32_t column;
    char stage[24]; /* "lexer", "parser", "semantic", "codegen", "assembler" */
    char message[160];
} forge_comp_error_t;

/* 6. Complete Compilation Output */
typedef struct {
    bool success;

    /* Token & AST artifacts */
    forge_token_list_t tokens;
    forge_ast_arena_t ast;

    /* Symbol Table */
    forge_comp_symbol_t symbols[FORGE_COMP_MAX_SYMBOLS];
    size_t symbol_count;

    /* Generated Forge-16 Assembly text */
    char generated_asm[FORGE_COMP_MAX_ASM_LEN];
    size_t generated_asm_len;

    /* Assembled Machine Code & .fbin executable */
    forge_asm_result_t asm_result;

    /* High-level to machine-code source map */
    forge_source_map_entry_t source_map[FORGE_COMP_MAX_SOURCE_MAP];
    size_t source_map_count;

    /* Compiler diagnostics */
    forge_comp_error_t errors[FORGE_COMP_MAX_ERRORS];
    size_t error_count;
} forge_compile_result_t;

/* Compiler Pipeline APIs */
void forge_compile_result_init(forge_compile_result_t *res);

void forge_comp_add_error(
    forge_compile_result_t *res,
    const char *stage,
    uint32_t line,
    uint32_t col,
    const char *fmt,
    ...
);

/* Stage 1: Lexer */
bool forge_lex_source(const char *source, forge_compile_result_t *res);

/* Stage 2: Parser & AST */
bool forge_parse_tokens(forge_compile_result_t *res);

/* Format AST into human-readable tree text */
void forge_format_ast(const forge_ast_node_t *node, int indent, char *buf, size_t buf_size);

/* Stage 3: Symbol Table, Type Checking & Semantic Analysis */
bool forge_semantic_analyze(forge_compile_result_t *res);

const forge_comp_symbol_t *forge_comp_find_var(
    const forge_compile_result_t *res,
    const char *scope_fn,
    const char *name
);

const forge_comp_symbol_t *forge_comp_find_fn(
    const forge_compile_result_t *res,
    const char *name
);

/* Stage 4: Code Generation & Assembler Integration */
bool forge_codegen_and_assemble(forge_compile_result_t *res);

/* Full End-to-End Compilation */
bool forge_compile_source(const char *source, forge_compile_result_t *out_res);
bool forge_compile_file(const char *filepath, forge_compile_result_t *out_res);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_COMPILER_H */

