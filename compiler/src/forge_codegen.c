#include "forge_compiler.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FORGE_MAX_LOOP_NESTING 16U

typedef struct {
    forge_compile_result_t *res;
    uint32_t current_src_line;
    uint32_t current_asm_line;
    uint32_t label_counter;
    uint32_t asm_to_src_line[FORGE_ASM_MAX_LINE_MAP];
    const char *current_fn;
    char break_labels[FORGE_MAX_LOOP_NESTING][32];
    char continue_labels[FORGE_MAX_LOOP_NESTING][32];
    size_t loop_depth;
} codegen_ctx_t;

static void emit_line(codegen_ctx_t *ctx, const char *fmt, ...) {
    if (ctx->res->generated_asm_len + 128U >= FORGE_COMP_MAX_ASM_LEN) {
        return;
    }
    ctx->current_asm_line++;
    if (ctx->current_asm_line < FORGE_ASM_MAX_LINE_MAP) {
        ctx->asm_to_src_line[ctx->current_asm_line] = ctx->current_src_line;
    }

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(
        ctx->res->generated_asm + ctx->res->generated_asm_len,
        FORGE_COMP_MAX_ASM_LEN - ctx->res->generated_asm_len,
        fmt,
        args
    );
    va_end(args);
    if (n > 0) {
        ctx->res->generated_asm_len += (size_t)n;
    }
    if (ctx->res->generated_asm_len + 2U < FORGE_COMP_MAX_ASM_LEN) {
        ctx->res->generated_asm[ctx->res->generated_asm_len++] = '\n';
        ctx->res->generated_asm[ctx->res->generated_asm_len] = '\0';
    }
}

static void gen_expr(codegen_ctx_t *ctx, const forge_ast_node_t *node);
static void gen_stmt(codegen_ctx_t *ctx, const forge_ast_node_t *node);

static void emit_var_load(codegen_ctx_t *ctx, const forge_comp_symbol_t *sym, const char *dest_reg) {
    if (sym->kind == FORGE_SYM_GLOBAL_VAR) {
        emit_line(ctx, "    LOAD %s, [0x%04X] ; global %s", dest_reg, sym->global_addr, sym->name);
    } else {
        if (sym->fp_offset >= 0) {
            emit_line(ctx, "    LOAD %s, [FP + %d] ; %s", dest_reg, (int)sym->fp_offset, sym->name);
        } else {
            emit_line(ctx, "    LOAD %s, [FP - %d] ; %s", dest_reg, -(int)sym->fp_offset, sym->name);
        }
    }
}

static void emit_var_store(codegen_ctx_t *ctx, const forge_comp_symbol_t *sym, const char *src_reg) {
    if (sym->kind == FORGE_SYM_GLOBAL_VAR) {
        emit_line(ctx, "    STORE %s, [0x%04X] ; global %s", src_reg, sym->global_addr, sym->name);
    } else {
        if (sym->fp_offset >= 0) {
            emit_line(ctx, "    STORE %s, [FP + %d] ; %s", src_reg, (int)sym->fp_offset, sym->name);
        } else {
            emit_line(ctx, "    STORE %s, [FP - %d] ; %s", src_reg, -(int)sym->fp_offset, sym->name);
        }
    }
}

static void gen_expr(codegen_ctx_t *ctx, const forge_ast_node_t *node) {
    if (node == NULL) {
        return;
    }
    ctx->current_src_line = node->line;

    switch (node->type) {
        case FORGE_AST_NUMBER_LIT:
            emit_line(ctx, "    MOV R0, %d", (int)node->int_val);
            break;

        case FORGE_AST_IN_EXPR:
            emit_line(ctx, "    IN R0, 1");
            break;

        case FORGE_AST_VAR_REF: {
            const forge_comp_symbol_t *sym = forge_comp_find_var(ctx->res, ctx->current_fn, node->name);
            if (sym != NULL) {
                emit_var_load(ctx, sym, "R0");
            }
            break;
        }

        case FORGE_AST_UNARY_OP:
            gen_expr(ctx, node->left);
            if (node->op == FORGE_TOK_MINUS) {
                emit_line(ctx, "    MOV R1, R0");
                emit_line(ctx, "    MOV R0, 0");
                emit_line(ctx, "    SUB R0, R1");
            } else if (node->op == FORGE_TOK_TILDE) {
                emit_line(ctx, "    NOT R0");
            } else if (node->op == FORGE_TOK_BANG) {
                uint32_t lbl = ++ctx->label_counter;
                emit_line(ctx, "    CMP R0, 0");
                emit_line(ctx, "    MOV R0, 1");
                emit_line(ctx, "    JZ _not_end_%u", lbl);
                emit_line(ctx, "    MOV R0, 0");
                emit_line(ctx, "_not_end_%u:", lbl);
            }
            break;

        case FORGE_AST_BINARY_OP: {
            /* Short-circuit logical && and || */
            if (node->op == FORGE_TOK_LAND) {
                uint32_t lbl = ++ctx->label_counter;
                gen_expr(ctx, node->left);
                emit_line(ctx, "    CMP R0, 0");
                emit_line(ctx, "    JZ _land_false_%u", lbl);
                gen_expr(ctx, node->right);
                emit_line(ctx, "    CMP R0, 0");
                emit_line(ctx, "    JZ _land_false_%u", lbl);
                emit_line(ctx, "    MOV R0, 1");
                emit_line(ctx, "    JMP _land_end_%u", lbl);
                emit_line(ctx, "_land_false_%u:", lbl);
                emit_line(ctx, "    MOV R0, 0");
                emit_line(ctx, "_land_end_%u:", lbl);
                break;
            }
            if (node->op == FORGE_TOK_LOR) {
                uint32_t lbl = ++ctx->label_counter;
                gen_expr(ctx, node->left);
                emit_line(ctx, "    CMP R0, 0");
                emit_line(ctx, "    JNZ _lor_true_%u", lbl);
                gen_expr(ctx, node->right);
                emit_line(ctx, "    CMP R0, 0");
                emit_line(ctx, "    JNZ _lor_true_%u", lbl);
                emit_line(ctx, "    MOV R0, 0");
                emit_line(ctx, "    JMP _lor_end_%u", lbl);
                emit_line(ctx, "_lor_true_%u:", lbl);
                emit_line(ctx, "    MOV R0, 1");
                emit_line(ctx, "_lor_end_%u:", lbl);
                break;
            }

            gen_expr(ctx, node->left);
            emit_line(ctx, "    PUSH R0");
            gen_expr(ctx, node->right);
            emit_line(ctx, "    MOV R1, R0");
            emit_line(ctx, "    POP R0");

            switch (node->op) {
                case FORGE_TOK_PLUS:  emit_line(ctx, "    ADD R0, R1"); break;
                case FORGE_TOK_MINUS: emit_line(ctx, "    SUB R0, R1"); break;
                case FORGE_TOK_STAR:  emit_line(ctx, "    MUL R0, R1"); break;
                case FORGE_TOK_SLASH: emit_line(ctx, "    DIV R0, R1"); break;
                case FORGE_TOK_MOD:
                    emit_line(ctx, "    MOV R2, R0");
                    emit_line(ctx, "    DIV R0, R1");
                    emit_line(ctx, "    MUL R0, R1");
                    emit_line(ctx, "    SUB R2, R0");
                    emit_line(ctx, "    MOV R0, R2");
                    break;
                case FORGE_TOK_AMP:   emit_line(ctx, "    AND R0, R1"); break;
                case FORGE_TOK_PIPE:  emit_line(ctx, "    OR  R0, R1"); break;
                case FORGE_TOK_CARET: emit_line(ctx, "    XOR R0, R1"); break;
                case FORGE_TOK_SHL:   emit_line(ctx, "    SHL R0, R1"); break;
                case FORGE_TOK_SHR:   emit_line(ctx, "    SHR R0, R1"); break;

                case FORGE_TOK_EQ:
                case FORGE_TOK_NEQ:
                case FORGE_TOK_LT:
                case FORGE_TOK_LTE:
                case FORGE_TOK_GT:
                case FORGE_TOK_GTE: {
                    uint32_t lbl = ++ctx->label_counter;
                    const char *jmp_op = "JZ";
                    switch (node->op) {
                        case FORGE_TOK_EQ:  jmp_op = "JZ";  break;
                        case FORGE_TOK_NEQ: jmp_op = "JNZ"; break;
                        case FORGE_TOK_LT:  jmp_op = "JL";  break;
                        case FORGE_TOK_LTE: jmp_op = "JLE"; break;
                        case FORGE_TOK_GT:  jmp_op = "JG";  break;
                        case FORGE_TOK_GTE: jmp_op = "JGE"; break;
                        default: break;
                    }
                    emit_line(ctx, "    CMP R0, R1");
                    emit_line(ctx, "    MOV R0, 1");
                    emit_line(ctx, "    %s _cmp_end_%u", jmp_op, lbl);
                    emit_line(ctx, "    MOV R0, 0");
                    emit_line(ctx, "_cmp_end_%u:", lbl);
                    break;
                }
                default:
                    break;
            }
            break;
        }

        case FORGE_AST_CALL: {
            const forge_comp_symbol_t *fn_sym = forge_comp_find_fn(ctx->res, node->name);
            if (fn_sym == NULL) {
                /* Built-in ForgeOS / Hardware Intrinsics */
                if (strcmp(node->name, "peek") == 0 && node->child_count == 1U) {
                    gen_expr(ctx, node->children[0]);
                    emit_line(ctx, "    LOAD R0, [R0]");
                } else if (strcmp(node->name, "poke") == 0 && node->child_count == 2U) {
                    gen_expr(ctx, node->children[0]);
                    emit_line(ctx, "    PUSH R0");
                    gen_expr(ctx, node->children[1]);
                    emit_line(ctx, "    POP R1");
                    emit_line(ctx, "    STORE R0, [R1]");
                } else if (strcmp(node->name, "free_all") == 0) {
                    emit_line(ctx, "    MOV R0, %u", (unsigned)FORGE_SYS_FREE_ALL);
                    emit_line(ctx, "    OUT R0, 3");
                } else if (strcmp(node->name, "key_ready") == 0) {
                    emit_line(ctx, "    MOV R0, %u", (unsigned)FORGE_SYS_KEY_AVAILABLE);
                    emit_line(ctx, "    OUT R0, 3");
                } else if (strcmp(node->name, "read_key") == 0) {
                    emit_line(ctx, "    MOV R0, %u", (unsigned)FORGE_SYS_READ_CHAR);
                    emit_line(ctx, "    OUT R0, 3");
                } else if (strcmp(node->name, "cycles") == 0) {
                    emit_line(ctx, "    MOV R0, %u", (unsigned)FORGE_SYS_CYCLES);
                    emit_line(ctx, "    OUT R0, 3");
                } else if (strcmp(node->name, "rand") == 0) {
                    emit_line(ctx, "    MOV R0, %u", (unsigned)FORGE_SYS_RANDOM);
                    emit_line(ctx, "    OUT R0, 3");
                } else if ((strcmp(node->name, "alloc") == 0 ||
                            strcmp(node->name, "clear_screen") == 0 ||
                            strcmp(node->name, "exit") == 0) &&
                           node->child_count == 1U) {
                    unsigned sys_id = (unsigned)FORGE_SYS_ALLOC;
                    if (strcmp(node->name, "clear_screen") == 0) {
                        sys_id = (unsigned)FORGE_SYS_CLEAR_SCREEN;
                    } else if (strcmp(node->name, "exit") == 0) {
                        sys_id = (unsigned)FORGE_SYS_EXIT;
                    }
                    gen_expr(ctx, node->children[0]);
                    emit_line(ctx, "    MOV R1, R0");
                    emit_line(ctx, "    MOV R0, %u", sys_id);
                    emit_line(ctx, "    OUT R0, 3");
                } else if (strcmp(node->name, "get_pixel") == 0 && node->child_count == 2U) {
                    gen_expr(ctx, node->children[0]);
                    emit_line(ctx, "    PUSH R0");
                    gen_expr(ctx, node->children[1]);
                    emit_line(ctx, "    MOV R2, R0");
                    emit_line(ctx, "    POP R1");
                    emit_line(ctx, "    MOV R0, %u", (unsigned)FORGE_SYS_GET_PIXEL);
                    emit_line(ctx, "    OUT R0, 3");
                } else if (strcmp(node->name, "draw_pixel") == 0 && node->child_count == 3U) {
                    gen_expr(ctx, node->children[0]);
                    emit_line(ctx, "    PUSH R0");
                    gen_expr(ctx, node->children[1]);
                    emit_line(ctx, "    PUSH R0");
                    gen_expr(ctx, node->children[2]);
                    emit_line(ctx, "    MOV R3, R0");
                    emit_line(ctx, "    POP R2");
                    emit_line(ctx, "    POP R1");
                    emit_line(ctx, "    MOV R0, %u", (unsigned)FORGE_SYS_DRAW_PIXEL);
                    emit_line(ctx, "    OUT R0, 3");
                }
                break;
            }
            /* 1. Evaluate all arguments in order and push onto hardware stack */
            for (size_t i = 0U; i < node->child_count; i++) {
                gen_expr(ctx, node->children[i]);
                emit_line(ctx, "    PUSH R0");
            }
            /* 2. Allocate callee activation frame on FP (R7) */
            uint16_t frame_words = (uint16_t)(fn_sym->param_count + fn_sym->local_words);
            if (frame_words == 0U) {
                frame_words = 1U;
            }
            uint16_t frame_bytes = (uint16_t)(frame_words * 2U);
            emit_line(ctx, "    SUB FP, %u", frame_bytes);

            /* 3. Pop evaluated arguments into callee parameter slots [FP + 2*i] */
            for (size_t i = node->child_count; i > 0U; i--) {
                size_t param_idx = i - 1U;
                emit_line(ctx, "    POP R0");
                emit_line(ctx, "    STORE R0, [FP + %zu]", param_idx * 2U);
            }

            /* 4. Call function and restore caller FP */
            emit_line(ctx, "    CALL fn_%s", node->name);
            emit_line(ctx, "    ADD FP, %u", frame_bytes);
            break;
        }

        default:
            break;
    }
}

static void gen_stmt(codegen_ctx_t *ctx, const forge_ast_node_t *node) {
    if (node == NULL) {
        return;
    }
    ctx->current_src_line = node->line;

    switch (node->type) {
        case FORGE_AST_BLOCK:
            for (size_t i = 0U; i < node->child_count; i++) {
                gen_stmt(ctx, node->children[i]);
            }
            break;

        case FORGE_AST_VAR_DECL:
        case FORGE_AST_ASSIGN: {
            gen_expr(ctx, node->left);
            ctx->current_src_line = node->line;
            const forge_comp_symbol_t *sym = forge_comp_find_var(ctx->res, ctx->current_fn, node->name);
            if (sym != NULL) {
                emit_var_store(ctx, sym, "R0");
            }
            break;
        }

        case FORGE_AST_PRINT:
            gen_expr(ctx, node->left);
            ctx->current_src_line = node->line;
            emit_line(ctx, "    OUT R0, 1");
            break;

        case FORGE_AST_PUTC:
            gen_expr(ctx, node->left);
            ctx->current_src_line = node->line;
            emit_line(ctx, "    OUT R0, 0");
            break;

        case FORGE_AST_IF: {
            uint32_t lbl = ++ctx->label_counter;
            gen_expr(ctx, node->cond);
            ctx->current_src_line = node->line;
            emit_line(ctx, "    CMP R0, 0");
            if (node->else_branch != NULL) {
                emit_line(ctx, "    JZ _else_%u", lbl);
                gen_stmt(ctx, node->then_branch);
                emit_line(ctx, "    JMP _endif_%u", lbl);
                emit_line(ctx, "_else_%u:", lbl);
                gen_stmt(ctx, node->else_branch);
                emit_line(ctx, "_endif_%u:", lbl);
            } else {
                emit_line(ctx, "    JZ _endif_%u", lbl);
                gen_stmt(ctx, node->then_branch);
                emit_line(ctx, "_endif_%u:", lbl);
            }
            break;
        }

        case FORGE_AST_WHILE: {
            uint32_t lbl = ++ctx->label_counter;
            size_t depth = ctx->loop_depth;
            if (depth < FORGE_MAX_LOOP_NESTING) {
                snprintf(ctx->continue_labels[depth], 32U, "_while_start_%u", lbl);
                snprintf(ctx->break_labels[depth], 32U, "_while_end_%u", lbl);
                ctx->loop_depth++;
            }
            emit_line(ctx, "_while_start_%u:", lbl);
            gen_expr(ctx, node->cond);
            ctx->current_src_line = node->line;
            emit_line(ctx, "    CMP R0, 0");
            emit_line(ctx, "    JZ _while_end_%u", lbl);
            gen_stmt(ctx, node->body);
            emit_line(ctx, "    JMP _while_start_%u", lbl);
            emit_line(ctx, "_while_end_%u:", lbl);
            if (depth < FORGE_MAX_LOOP_NESTING) {
                ctx->loop_depth = depth;
            }
            break;
        }

        case FORGE_AST_FOR: {
            uint32_t lbl = ++ctx->label_counter;
            gen_stmt(ctx, node->left); /* init_stmt */
            size_t depth = ctx->loop_depth;
            if (depth < FORGE_MAX_LOOP_NESTING) {
                snprintf(ctx->continue_labels[depth], 32U, "_for_step_%u", lbl);
                snprintf(ctx->break_labels[depth], 32U, "_for_end_%u", lbl);
                ctx->loop_depth++;
            }
            emit_line(ctx, "_for_cond_%u:", lbl);
            gen_expr(ctx, node->cond);
            ctx->current_src_line = node->line;
            emit_line(ctx, "    CMP R0, 0");
            emit_line(ctx, "    JZ _for_end_%u", lbl);
            gen_stmt(ctx, node->body);
            emit_line(ctx, "_for_step_%u:", lbl);
            gen_stmt(ctx, node->right); /* step_stmt */
            emit_line(ctx, "    JMP _for_cond_%u", lbl);
            emit_line(ctx, "_for_end_%u:", lbl);
            if (depth < FORGE_MAX_LOOP_NESTING) {
                ctx->loop_depth = depth;
            }
            break;
        }

        case FORGE_AST_BREAK:
            if (ctx->loop_depth > 0U) {
                emit_line(ctx, "    JMP %s", ctx->break_labels[ctx->loop_depth - 1U]);
            }
            break;

        case FORGE_AST_CONTINUE:
            if (ctx->loop_depth > 0U) {
                emit_line(ctx, "    JMP %s", ctx->continue_labels[ctx->loop_depth - 1U]);
            }
            break;

        case FORGE_AST_RETURN:
            gen_expr(ctx, node->left);
            ctx->current_src_line = node->line;
            emit_line(ctx, "    RET");
            break;

        case FORGE_AST_FN_DECL:
            /* Emitted in dedicated function section */
            break;

        default:
            /* Expression statement (e.g. function call) */
            gen_expr(ctx, node);
            break;
    }
}

bool forge_codegen_and_assemble(forge_compile_result_t *res) {
    if (res == NULL || res->ast.root == NULL || res->error_count > 0U) {
        return false;
    }

    codegen_ctx_t *ctx = (codegen_ctx_t *)calloc(1U, sizeof(codegen_ctx_t));
    if (ctx == NULL) {
        return false;
    }
    ctx->res = res;
    ctx->current_src_line = 1U;
    ctx->current_asm_line = 0U;
    ctx->label_counter = 0U;
    ctx->current_fn = "";

    res->generated_asm[0] = '\0';
    res->generated_asm_len = 0U;

    const forge_ast_node_t *root = res->ast.root;

    emit_line(ctx, "; ForgeLang Compiled Assembly Output");
    emit_line(ctx, ".entry _start");
    emit_line(ctx, "_start:");
    emit_line(ctx, "    MOV FP, 0xDF00 ; Initialize activation frame pointer");

    /* Emit all top-level statements (skipping FN_DECL nodes) */
    for (size_t i = 0U; i < root->child_count; i++) {
        const forge_ast_node_t *stmt = root->children[i];
        if (stmt->type != FORGE_AST_FN_DECL) {
            gen_stmt(ctx, stmt);
        }
    }

    ctx->current_src_line = (root->child_count > 0U)
                                ? root->children[root->child_count - 1U]->line
                                : 1U;
    emit_line(ctx, "    HALT");

    /* Emit all function bodies */
    for (size_t i = 0U; i < root->child_count; i++) {
        const forge_ast_node_t *stmt = root->children[i];
        if (stmt->type == FORGE_AST_FN_DECL) {
            ctx->current_fn = stmt->name;
            ctx->current_src_line = stmt->line;
            emit_line(ctx, "fn_%s:", stmt->name);
            gen_stmt(ctx, stmt->body);
            /* Safety fallback RET in case control reaches end of function without explicit return */
            ctx->current_src_line = stmt->line;
            emit_line(ctx, "    RET");
            ctx->current_fn = "";
        }
    }

    /* Assemble generated assembly into machine code via forge_assemble_source */
    if (!forge_assemble_source(res->generated_asm, &res->asm_result)) {
        for (size_t i = 0U; i < res->asm_result.error_count; i++) {
            forge_comp_add_error(
                res,
                "assembler",
                res->asm_result.errors[i].line,
                res->asm_result.errors[i].column,
                "%s",
                res->asm_result.errors[i].message
            );
        }
        free(ctx);
        return false;
    }

    /* Build High-Level Line <-> Assembly Line <-> Machine Address Source Map */
    res->source_map_count = 0U;
    for (size_t i = 0U; i < res->asm_result.line_map_count; i++) {
        const forge_asm_line_map_t *lm = &res->asm_result.line_map[i];
        if (res->source_map_count >= FORGE_COMP_MAX_SOURCE_MAP) {
            break;
        }
        forge_source_map_entry_t *sm = &res->source_map[res->source_map_count++];
        sm->asm_line    = lm->line;
        sm->address     = lm->address;
        sm->source_line = (lm->line < FORGE_ASM_MAX_LINE_MAP)
                              ? ctx->asm_to_src_line[lm->line]
                              : 1U;
    }

    free(ctx);
    res->success = (res->error_count == 0U);
    return res->success;
}

bool forge_compile_source(const char *source, forge_compile_result_t *out_res) {
    if (out_res == NULL) {
        return false;
    }
    forge_compile_result_init(out_res);
    if (!forge_lex_source(source, out_res)) {
        return false;
    }
    if (!forge_parse_tokens(out_res)) {
        return false;
    }
    if (!forge_semantic_analyze(out_res)) {
        return false;
    }
    if (!forge_codegen_and_assemble(out_res)) {
        return false;
    }
    return true;
}

bool forge_compile_file(const char *filepath, forge_compile_result_t *out_res) {
    if (out_res == NULL) {
        return false;
    }
    forge_compile_result_init(out_res);
    if (filepath == NULL) {
        forge_comp_add_error(out_res, "driver", 1U, 1U, "Filepath is NULL");
        return false;
    }

    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        forge_comp_add_error(out_res, "driver", 1U, 1U, "Could not open source file '%s'", filepath);
        return false;
    }
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }
    long fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        return false;
    }
    rewind(fp);

    char *buf = (char *)malloc((size_t)fsize + 1U);
    if (buf == NULL) {
        fclose(fp);
        return false;
    }
    size_t nread = fread(buf, 1U, (size_t)fsize, fp);
    fclose(fp);
    buf[nread] = '\0';

    bool ok = forge_compile_source(buf, out_res);
    free(buf);
    return ok;
}

