#include "forge_compiler.h"
#include <stdio.h>
#include <string.h>

const forge_comp_symbol_t *forge_comp_find_var(
    const forge_compile_result_t *res,
    const char *scope_fn,
    const char *name
) {
    if (res == NULL || name == NULL) {
        return NULL;
    }
    /* First search local / parameter scope if inside a function */
    if (scope_fn != NULL && scope_fn[0] != '\0') {
        for (size_t i = 0U; i < res->symbol_count; i++) {
            const forge_comp_symbol_t *sym = &res->symbols[i];
            if ((sym->kind == FORGE_SYM_LOCAL_VAR || sym->kind == FORGE_SYM_PARAM_VAR) &&
                strcmp(sym->scope_fn, scope_fn) == 0 &&
                strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    /* Fallback to global variables */
    for (size_t i = 0U; i < res->symbol_count; i++) {
        const forge_comp_symbol_t *sym = &res->symbols[i];
        if (sym->kind == FORGE_SYM_GLOBAL_VAR && strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

const forge_comp_symbol_t *forge_comp_find_fn(
    const forge_compile_result_t *res,
    const char *name
) {
    if (res == NULL || name == NULL) {
        return NULL;
    }
    for (size_t i = 0U; i < res->symbol_count; i++) {
        const forge_comp_symbol_t *sym = &res->symbols[i];
        if (sym->kind == FORGE_SYM_FUNCTION && strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

static forge_comp_symbol_t *find_fn_mut(forge_compile_result_t *res, const char *name) {
    for (size_t i = 0U; i < res->symbol_count; i++) {
        forge_comp_symbol_t *sym = &res->symbols[i];
        if (sym->kind == FORGE_SYM_FUNCTION && strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

static bool has_var_in_exact_scope(
    const forge_compile_result_t *res,
    const char *scope_fn,
    const char *name
) {
    for (size_t i = 0U; i < res->symbol_count; i++) {
        const forge_comp_symbol_t *sym = &res->symbols[i];
        if (sym->kind != FORGE_SYM_FUNCTION &&
            strcmp(sym->scope_fn, scope_fn) == 0 &&
            strcmp(sym->name, name) == 0) {
            return true;
        }
    }
    return false;
}

static forge_comp_symbol_t *add_symbol(
    forge_compile_result_t *res,
    const char *name,
    const char *scope_fn,
    forge_comp_sym_kind_t kind,
    uint32_t line
) {
    if (res->symbol_count >= FORGE_COMP_MAX_SYMBOLS) {
        forge_comp_add_error(res, "semantic", line, 1U, "Compiler symbol table capacity exceeded");
        return NULL;
    }
    forge_comp_symbol_t *sym = &res->symbols[res->symbol_count++];
    memset(sym, 0, sizeof(*sym));
    (void)snprintf(sym->name, sizeof(sym->name), "%s", name);
    (void)snprintf(sym->scope_fn, sizeof(sym->scope_fn), "%s", scope_fn != NULL ? scope_fn : "");
    sym->kind = kind;
    sym->decl_line = line;
    return sym;
}

static void check_node(
    forge_compile_result_t *res,
    const forge_ast_node_t *node,
    const char *current_fn,
    int loop_depth,
    uint16_t *next_global_addr
) {
    if (node == NULL) {
        return;
    }

    switch (node->type) {
        case FORGE_AST_PROGRAM:
        case FORGE_AST_BLOCK:
            for (size_t i = 0U; i < node->child_count; i++) {
                check_node(res, node->children[i], current_fn, loop_depth, next_global_addr);
            }
            break;

        case FORGE_AST_VAR_DECL: {
            check_node(res, node->left, current_fn, loop_depth, next_global_addr);
            if (has_var_in_exact_scope(res, current_fn, node->name)) {
                forge_comp_add_error(
                    res,
                    "semantic",
                    node->line,
                    node->column,
                    "Duplicate variable declaration '%s'",
                    node->name
                );
                return;
            }
            if (current_fn[0] == '\0') {
                forge_comp_symbol_t *sym = add_symbol(res, node->name, "", FORGE_SYM_GLOBAL_VAR, node->line);
                if (sym != NULL) {
                    sym->global_addr = *next_global_addr;
                    *next_global_addr = (uint16_t)(*next_global_addr + 2U);
                }
            } else {
                forge_comp_symbol_t *fn_sym = find_fn_mut(res, current_fn);
                forge_comp_symbol_t *sym = add_symbol(res, node->name, current_fn, FORGE_SYM_LOCAL_VAR, node->line);
                if (sym != NULL && fn_sym != NULL) {
                    sym->fp_offset = (int16_t)(2 * (int)(fn_sym->param_count + fn_sym->local_words));
                    fn_sym->local_words++;
                }
            }
            break;
        }

        case FORGE_AST_ASSIGN: {
            check_node(res, node->left, current_fn, loop_depth, next_global_addr);
            const forge_comp_symbol_t *v = forge_comp_find_var(res, current_fn, node->name);
            if (v == NULL) {
                forge_comp_add_error(
                    res,
                    "semantic",
                    node->line,
                    node->column,
                    "Assignment to undeclared variable '%s'",
                    node->name
                );
            }
            break;
        }

        case FORGE_AST_PRINT:
        case FORGE_AST_PUTC:
            check_node(res, node->left, current_fn, loop_depth, next_global_addr);
            break;

        case FORGE_AST_IF:
            check_node(res, node->cond, current_fn, loop_depth, next_global_addr);
            check_node(res, node->then_branch, current_fn, loop_depth, next_global_addr);
            if (node->else_branch != NULL) {
                check_node(res, node->else_branch, current_fn, loop_depth, next_global_addr);
            }
            break;

        case FORGE_AST_WHILE:
            check_node(res, node->cond, current_fn, loop_depth, next_global_addr);
            check_node(res, node->body, current_fn, loop_depth + 1, next_global_addr);
            break;

        case FORGE_AST_FOR:
            check_node(res, node->left, current_fn, loop_depth, next_global_addr);
            check_node(res, node->cond, current_fn, loop_depth, next_global_addr);
            check_node(res, node->right, current_fn, loop_depth, next_global_addr);
            check_node(res, node->body, current_fn, loop_depth + 1, next_global_addr);
            break;

        case FORGE_AST_BREAK:
            if (loop_depth <= 0) {
                forge_comp_add_error(
                    res,
                    "semantic",
                    node->line,
                    node->column,
                    "'break' statement is only valid inside a loop"
                );
            }
            break;

        case FORGE_AST_CONTINUE:
            if (loop_depth <= 0) {
                forge_comp_add_error(
                    res,
                    "semantic",
                    node->line,
                    node->column,
                    "'continue' statement is only valid inside a loop"
                );
            }
            break;

        case FORGE_AST_FN_DECL:
            if (current_fn[0] != '\0') {
                forge_comp_add_error(
                    res,
                    "semantic",
                    node->line,
                    node->column,
                    "Nested function declarations are not allowed ('%s')",
                    node->name
                );
                return;
            }
            check_node(res, node->body, node->name, 0, next_global_addr);
            break;

        case FORGE_AST_RETURN:
            if (current_fn[0] == '\0') {
                forge_comp_add_error(
                    res,
                    "semantic",
                    node->line,
                    node->column,
                    "'return' statement is only valid inside a function"
                );
            }
            check_node(res, node->left, current_fn, loop_depth, next_global_addr);
            break;

        case FORGE_AST_BINARY_OP:
            check_node(res, node->left, current_fn, loop_depth, next_global_addr);
            check_node(res, node->right, current_fn, loop_depth, next_global_addr);
            break;

        case FORGE_AST_UNARY_OP:
            check_node(res, node->left, current_fn, loop_depth, next_global_addr);
            break;

        case FORGE_AST_CALL: {
            const forge_comp_symbol_t *fn = forge_comp_find_fn(res, node->name);
            if (fn != NULL) {
                if (node->child_count != fn->param_count) {
                    forge_comp_add_error(
                        res,
                        "semantic",
                        node->line,
                        node->column,
                        "Function '%s' expects %zu argument(s), but %zu provided",
                        node->name,
                        fn->param_count,
                        node->child_count
                    );
                }
            } else {
                /* Check built-in ForgeOS / hardware intrinsics */
                int builtin_arity = -1;
                if (strcmp(node->name, "free_all") == 0 ||
                    strcmp(node->name, "key_ready") == 0 ||
                    strcmp(node->name, "read_key") == 0 ||
                    strcmp(node->name, "cycles") == 0 ||
                    strcmp(node->name, "rand") == 0) {
                    builtin_arity = 0;
                } else if (strcmp(node->name, "alloc") == 0 ||
                           strcmp(node->name, "peek") == 0 ||
                           strcmp(node->name, "clear_screen") == 0 ||
                           strcmp(node->name, "exit") == 0) {
                    builtin_arity = 1;
                } else if (strcmp(node->name, "poke") == 0 ||
                           strcmp(node->name, "get_pixel") == 0) {
                    builtin_arity = 2;
                } else if (strcmp(node->name, "draw_pixel") == 0) {
                    builtin_arity = 3;
                }

                if (builtin_arity < 0) {
                    forge_comp_add_error(
                        res,
                        "semantic",
                        node->line,
                        node->column,
                        "Call to undefined function '%s'",
                        node->name
                    );
                } else if ((int)node->child_count != builtin_arity) {
                    forge_comp_add_error(
                        res,
                        "semantic",
                        node->line,
                        node->column,
                        "Built-in '%s' expects %d argument(s), but %zu provided",
                        node->name,
                        builtin_arity,
                        node->child_count
                    );
                }
            }
            for (size_t i = 0U; i < node->child_count; i++) {
                check_node(res, node->children[i], current_fn, loop_depth, next_global_addr);
            }
            break;
        }

        case FORGE_AST_VAR_REF: {
            const forge_comp_symbol_t *v = forge_comp_find_var(res, current_fn, node->name);
            if (v == NULL) {
                forge_comp_add_error(
                    res,
                    "semantic",
                    node->line,
                    node->column,
                    "Use of undeclared variable '%s'",
                    node->name
                );
            }
            break;
        }

        case FORGE_AST_IN_EXPR:
        case FORGE_AST_NUMBER_LIT:
            break;
    }
}

bool forge_semantic_analyze(forge_compile_result_t *res) {
    if (res == NULL || res->ast.root == NULL) {
        return false;
    }
    res->symbol_count = 0U;
    const forge_ast_node_t *root = res->ast.root;

    /* Pass 1: Register all top-level function signatures and parameters */
    for (size_t i = 0U; i < root->child_count; i++) {
        const forge_ast_node_t *stmt = root->children[i];
        if (stmt->type == FORGE_AST_FN_DECL) {
            if (forge_comp_find_fn(res, stmt->name) != NULL) {
                forge_comp_add_error(
                    res,
                    "semantic",
                    stmt->line,
                    stmt->column,
                    "Duplicate function definition '%s'",
                    stmt->name
                );
                continue;
            }
            forge_comp_symbol_t *fn_sym = add_symbol(res, stmt->name, "", FORGE_SYM_FUNCTION, stmt->line);
            if (fn_sym == NULL) return false;
            fn_sym->param_count = stmt->param_count;
            fn_sym->local_words = 0U;

            for (size_t p = 0U; p < stmt->param_count; p++) {
                if (has_var_in_exact_scope(res, stmt->name, stmt->params[p])) {
                    forge_comp_add_error(
                        res,
                        "semantic",
                        stmt->line,
                        stmt->column,
                        "Duplicate parameter '%s' in function '%s'",
                        stmt->params[p],
                        stmt->name
                    );
                    continue;
                }
                forge_comp_symbol_t *psym = add_symbol(
                    res,
                    stmt->params[p],
                    stmt->name,
                    FORGE_SYM_PARAM_VAR,
                    stmt->line
                );
                if (psym != NULL) {
                    psym->fp_offset = (int16_t)(2 * (int)p);
                }
            }
        }
    }

    /* Pass 2: Check all statements, declarations, expressions, and types */
    uint16_t next_global_addr = FORGE_HEAP_START; /* 0x8000 */
    check_node(res, root, "", 0, &next_global_addr);

    return res->error_count == 0U;
}

