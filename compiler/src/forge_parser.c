#include "forge_compiler.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    forge_compile_result_t *res;
    size_t pos;
} parser_state_t;

static const forge_token_t *peek_tok(const parser_state_t *ps) {
    if (ps->pos >= ps->res->tokens.count) {
        return &ps->res->tokens.tokens[ps->res->tokens.count - 1U];
    }
    return &ps->res->tokens.tokens[ps->pos];
}

static const forge_token_t *advance_tok(parser_state_t *ps) {
    const forge_token_t *tok = peek_tok(ps);
    if (tok->type != FORGE_TOK_EOF && ps->pos + 1U < ps->res->tokens.count) {
        ps->pos++;
    }
    return tok;
}

static bool match_tok(parser_state_t *ps, forge_token_type_t expected) {
    if (peek_tok(ps)->type == expected) {
        advance_tok(ps);
        return true;
    }
    return false;
}

static const forge_token_t *expect_tok(parser_state_t *ps, forge_token_type_t expected, const char *desc) {
    const forge_token_t *tok = peek_tok(ps);
    if (tok->type != expected) {
        forge_comp_add_error(
            ps->res,
            "parser",
            tok->line,
            tok->column,
            "Expected %s, but found '%s'",
            desc,
            tok->text
        );
        return NULL;
    }
    return advance_tok(ps);
}

static forge_ast_node_t *alloc_node(parser_state_t *ps, forge_ast_type_t type, uint32_t line, uint32_t col) {
    if (ps->res->ast.used >= FORGE_COMP_MAX_AST_NODES) {
        forge_comp_add_error(ps->res, "parser", line, col, "AST node capacity (%u) exceeded", FORGE_COMP_MAX_AST_NODES);
        return NULL;
    }
    forge_ast_node_t *node = &ps->res->ast.nodes[ps->res->ast.used++];
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->line = line;
    node->column = col;
    return node;
}

static forge_ast_node_t *parse_expr(parser_state_t *ps);
static forge_ast_node_t *parse_statement(parser_state_t *ps);
static forge_ast_node_t *parse_block(parser_state_t *ps);

static forge_ast_node_t *parse_primary(parser_state_t *ps) {
    const forge_token_t *tok = peek_tok(ps);

    if (tok->type == FORGE_TOK_NUMBER) {
        advance_tok(ps);
        forge_ast_node_t *node = alloc_node(ps, FORGE_AST_NUMBER_LIT, tok->line, tok->column);
        if (node != NULL) {
            node->int_val = tok->int_val;
        }
        return node;
    }

    if (tok->type == FORGE_TOK_IN) {
        advance_tok(ps);
        if (expect_tok(ps, FORGE_TOK_LPAREN, "'(' after 'in'") == NULL) return NULL;
        if (expect_tok(ps, FORGE_TOK_RPAREN, "')' after 'in('") == NULL) return NULL;
        return alloc_node(ps, FORGE_AST_IN_EXPR, tok->line, tok->column);
    }

    if (tok->type == FORGE_TOK_IDENT) {
        advance_tok(ps);
        /* Check if function call: ident(...) */
        if (match_tok(ps, FORGE_TOK_LPAREN)) {
            forge_ast_node_t *call = alloc_node(ps, FORGE_AST_CALL, tok->line, tok->column);
            if (call == NULL) return NULL;
            (void)snprintf(call->name, sizeof(call->name), "%s", tok->text);

            if (peek_tok(ps)->type != FORGE_TOK_RPAREN) {
                while (true) {
                    forge_ast_node_t *arg = parse_expr(ps);
                    if (arg == NULL) return NULL;
                    if (call->child_count >= FORGE_COMP_MAX_CHILDREN) {
                        forge_comp_add_error(ps->res, "parser", tok->line, tok->column, "Too many call arguments");
                        return NULL;
                    }
                    call->children[call->child_count++] = arg;
                    if (!match_tok(ps, FORGE_TOK_COMMA)) {
                        break;
                    }
                }
            }
            if (expect_tok(ps, FORGE_TOK_RPAREN, "')' after function arguments") == NULL) {
                return NULL;
            }
            return call;
        }

        forge_ast_node_t *vref = alloc_node(ps, FORGE_AST_VAR_REF, tok->line, tok->column);
        if (vref != NULL) {
            (void)snprintf(vref->name, sizeof(vref->name), "%s", tok->text);
        }
        return vref;
    }

    if (match_tok(ps, FORGE_TOK_LPAREN)) {
        forge_ast_node_t *inner = parse_expr(ps);
        if (inner == NULL) return NULL;
        if (expect_tok(ps, FORGE_TOK_RPAREN, "')' after parenthesized expression") == NULL) {
            return NULL;
        }
        return inner;
    }

    forge_comp_add_error(
        ps->res,
        "parser",
        tok->line,
        tok->column,
        "Unexpected token '%s' in expression",
        tok->text
    );
    return NULL;
}

static forge_ast_node_t *parse_unary(parser_state_t *ps) {
    const forge_token_t *tok = peek_tok(ps);
    if (tok->type == FORGE_TOK_MINUS || tok->type == FORGE_TOK_TILDE || tok->type == FORGE_TOK_BANG) {
        advance_tok(ps);
        forge_ast_node_t *operand = parse_unary(ps);
        if (operand == NULL) return NULL;
        forge_ast_node_t *u = alloc_node(ps, FORGE_AST_UNARY_OP, tok->line, tok->column);
        if (u != NULL) {
            u->op = tok->type;
            u->left = operand;
        }
        return u;
    }
    return parse_primary(ps);
}

static forge_ast_node_t *parse_multiplicative(parser_state_t *ps) {
    forge_ast_node_t *left = parse_unary(ps);
    while (left != NULL) {
        const forge_token_t *tok = peek_tok(ps);
        if (tok->type != FORGE_TOK_STAR && tok->type != FORGE_TOK_SLASH && tok->type != FORGE_TOK_MOD) {
            break;
        }
        advance_tok(ps);
        forge_ast_node_t *right = parse_unary(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_additive(parser_state_t *ps) {
    forge_ast_node_t *left = parse_multiplicative(ps);
    while (left != NULL) {
        const forge_token_t *tok = peek_tok(ps);
        if (tok->type != FORGE_TOK_PLUS && tok->type != FORGE_TOK_MINUS) {
            break;
        }
        advance_tok(ps);
        forge_ast_node_t *right = parse_multiplicative(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_shift(parser_state_t *ps) {
    forge_ast_node_t *left = parse_additive(ps);
    while (left != NULL) {
        const forge_token_t *tok = peek_tok(ps);
        if (tok->type != FORGE_TOK_SHL && tok->type != FORGE_TOK_SHR) {
            break;
        }
        advance_tok(ps);
        forge_ast_node_t *right = parse_additive(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_relational(parser_state_t *ps) {
    forge_ast_node_t *left = parse_shift(ps);
    while (left != NULL) {
        const forge_token_t *tok = peek_tok(ps);
        if (tok->type != FORGE_TOK_LT && tok->type != FORGE_TOK_LTE &&
            tok->type != FORGE_TOK_GT && tok->type != FORGE_TOK_GTE) {
            break;
        }
        advance_tok(ps);
        forge_ast_node_t *right = parse_shift(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_equality(parser_state_t *ps) {
    forge_ast_node_t *left = parse_relational(ps);
    while (left != NULL) {
        const forge_token_t *tok = peek_tok(ps);
        if (tok->type != FORGE_TOK_EQ && tok->type != FORGE_TOK_NEQ) {
            break;
        }
        advance_tok(ps);
        forge_ast_node_t *right = parse_relational(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_bit_and(parser_state_t *ps) {
    forge_ast_node_t *left = parse_equality(ps);
    while (left != NULL && peek_tok(ps)->type == FORGE_TOK_AMP) {
        const forge_token_t *tok = advance_tok(ps);
        forge_ast_node_t *right = parse_equality(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_bit_xor(parser_state_t *ps) {
    forge_ast_node_t *left = parse_bit_and(ps);
    while (left != NULL && peek_tok(ps)->type == FORGE_TOK_CARET) {
        const forge_token_t *tok = advance_tok(ps);
        forge_ast_node_t *right = parse_bit_and(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_bit_or(parser_state_t *ps) {
    forge_ast_node_t *left = parse_bit_xor(ps);
    while (left != NULL && peek_tok(ps)->type == FORGE_TOK_PIPE) {
        const forge_token_t *tok = advance_tok(ps);
        forge_ast_node_t *right = parse_bit_xor(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_logical_and(parser_state_t *ps) {
    forge_ast_node_t *left = parse_bit_or(ps);
    while (left != NULL && peek_tok(ps)->type == FORGE_TOK_LAND) {
        const forge_token_t *tok = advance_tok(ps);
        forge_ast_node_t *right = parse_bit_or(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_logical_or(parser_state_t *ps) {
    forge_ast_node_t *left = parse_logical_and(ps);
    while (left != NULL && peek_tok(ps)->type == FORGE_TOK_LOR) {
        const forge_token_t *tok = advance_tok(ps);
        forge_ast_node_t *right = parse_logical_and(ps);
        if (right == NULL) return NULL;
        forge_ast_node_t *bin = alloc_node(ps, FORGE_AST_BINARY_OP, tok->line, tok->column);
        if (bin == NULL) return NULL;
        bin->op = tok->type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static forge_ast_node_t *parse_expr(parser_state_t *ps) {
    return parse_logical_or(ps);
}

static forge_ast_node_t *parse_block(parser_state_t *ps) {
    const forge_token_t *lbrace = expect_tok(ps, FORGE_TOK_LBRACE, "'{' to start block");
    if (lbrace == NULL) return NULL;

    forge_ast_node_t *block = alloc_node(ps, FORGE_AST_BLOCK, lbrace->line, lbrace->column);
    if (block == NULL) return NULL;

    while (peek_tok(ps)->type != FORGE_TOK_RBRACE && peek_tok(ps)->type != FORGE_TOK_EOF) {
        while (match_tok(ps, FORGE_TOK_SEMICOLON)) {}
        if (peek_tok(ps)->type == FORGE_TOK_RBRACE) break;
        forge_ast_node_t *stmt = parse_statement(ps);
        if (stmt == NULL) return NULL;
        if (block->child_count >= FORGE_COMP_MAX_CHILDREN) {
            forge_comp_add_error(ps->res, "parser", stmt->line, stmt->column, "Block statement limit exceeded");
            return NULL;
        }
        block->children[block->child_count++] = stmt;
        while (match_tok(ps, FORGE_TOK_SEMICOLON)) {}
    }

    if (expect_tok(ps, FORGE_TOK_RBRACE, "'}' to close block") == NULL) {
        return NULL;
    }
    return block;
}

static forge_ast_node_t *parse_if_stmt(parser_state_t *ps) {
    const forge_token_t *if_tok = advance_tok(ps);
    forge_ast_node_t *cond = parse_expr(ps);
    if (cond == NULL) return NULL;
    forge_ast_node_t *then_br = parse_block(ps);
    if (then_br == NULL) return NULL;

    forge_ast_node_t *else_br = NULL;
    if (match_tok(ps, FORGE_TOK_ELSE)) {
        if (peek_tok(ps)->type == FORGE_TOK_IF) {
            else_br = parse_if_stmt(ps);
        } else {
            else_br = parse_block(ps);
        }
        if (else_br == NULL) return NULL;
    }

    forge_ast_node_t *node = alloc_node(ps, FORGE_AST_IF, if_tok->line, if_tok->column);
    if (node != NULL) {
        node->cond = cond;
        node->then_branch = then_br;
        node->else_branch = else_br;
    }
    return node;
}

static forge_ast_node_t *parse_statement(parser_state_t *ps) {
    const forge_token_t *tok = peek_tok(ps);

    if (tok->type == FORGE_TOK_LET) {
        advance_tok(ps);
        const forge_token_t *id = expect_tok(ps, FORGE_TOK_IDENT, "variable name after 'let'");
        if (id == NULL) return NULL;
        if (expect_tok(ps, FORGE_TOK_ASSIGN, "'=' in variable declaration") == NULL) return NULL;
        forge_ast_node_t *val = parse_expr(ps);
        if (val == NULL) return NULL;

        forge_ast_node_t *decl = alloc_node(ps, FORGE_AST_VAR_DECL, tok->line, tok->column);
        if (decl != NULL) {
            (void)snprintf(decl->name, sizeof(decl->name), "%s", id->text);
            decl->left = val;
        }
        return decl;
    }

    if (tok->type == FORGE_TOK_PRINT || tok->type == FORGE_TOK_PUTC) {
        advance_tok(ps);
        forge_ast_node_t *val = parse_expr(ps);
        if (val == NULL) return NULL;
        forge_ast_node_t *p = alloc_node(
            ps,
            tok->type == FORGE_TOK_PRINT ? FORGE_AST_PRINT : FORGE_AST_PUTC,
            tok->line,
            tok->column
        );
        if (p != NULL) {
            p->left = val;
        }
        return p;
    }

    if (tok->type == FORGE_TOK_IF) {
        return parse_if_stmt(ps);
    }

    if (tok->type == FORGE_TOK_WHILE) {
        advance_tok(ps);
        forge_ast_node_t *cond = parse_expr(ps);
        if (cond == NULL) return NULL;
        forge_ast_node_t *body = parse_block(ps);
        if (body == NULL) return NULL;
        forge_ast_node_t *w = alloc_node(ps, FORGE_AST_WHILE, tok->line, tok->column);
        if (w != NULL) {
            w->cond = cond;
            w->body = body;
        }
        return w;
    }

    if (tok->type == FORGE_TOK_FOR) {
        advance_tok(ps);
        bool has_paren = match_tok(ps, FORGE_TOK_LPAREN);
        forge_ast_node_t *init_stmt = parse_statement(ps);
        if (init_stmt == NULL) return NULL;
        if (expect_tok(ps, FORGE_TOK_SEMICOLON, "';' after for-loop initializer") == NULL) return NULL;
        forge_ast_node_t *cond = parse_expr(ps);
        if (cond == NULL) return NULL;
        if (expect_tok(ps, FORGE_TOK_SEMICOLON, "';' after for-loop condition") == NULL) return NULL;
        forge_ast_node_t *step_stmt = parse_statement(ps);
        if (step_stmt == NULL) return NULL;
        if (has_paren && expect_tok(ps, FORGE_TOK_RPAREN, "')' after for-loop header") == NULL) {
            return NULL;
        }
        forge_ast_node_t *body = parse_block(ps);
        if (body == NULL) return NULL;
        forge_ast_node_t *f = alloc_node(ps, FORGE_AST_FOR, tok->line, tok->column);
        if (f != NULL) {
            f->left  = init_stmt;
            f->cond  = cond;
            f->right = step_stmt;
            f->body  = body;
        }
        return f;
    }

    if (tok->type == FORGE_TOK_BREAK) {
        advance_tok(ps);
        return alloc_node(ps, FORGE_AST_BREAK, tok->line, tok->column);
    }

    if (tok->type == FORGE_TOK_CONTINUE) {
        advance_tok(ps);
        return alloc_node(ps, FORGE_AST_CONTINUE, tok->line, tok->column);
    }

    if (tok->type == FORGE_TOK_FN) {
        advance_tok(ps);
        const forge_token_t *fn_id = expect_tok(ps, FORGE_TOK_IDENT, "function name after 'fn'");
        if (fn_id == NULL) return NULL;
        if (expect_tok(ps, FORGE_TOK_LPAREN, "'(' after function name") == NULL) return NULL;

        forge_ast_node_t *fn_node = alloc_node(ps, FORGE_AST_FN_DECL, tok->line, tok->column);
        if (fn_node == NULL) return NULL;
        (void)snprintf(fn_node->name, sizeof(fn_node->name), "%s", fn_id->text);

        if (peek_tok(ps)->type != FORGE_TOK_RPAREN) {
            while (true) {
                const forge_token_t *param = expect_tok(ps, FORGE_TOK_IDENT, "parameter name");
                if (param == NULL) return NULL;
                if (fn_node->param_count >= FORGE_COMP_MAX_PARAMS) {
                    forge_comp_add_error(ps->res, "parser", param->line, param->column, "Maximum 8 function parameters");
                    return NULL;
                }
                (void)snprintf(fn_node->params[fn_node->param_count++], sizeof(fn_node->params[0]), "%s", param->text);
                if (!match_tok(ps, FORGE_TOK_COMMA)) {
                    break;
                }
            }
        }
        if (expect_tok(ps, FORGE_TOK_RPAREN, "')' after parameter list") == NULL) return NULL;
        forge_ast_node_t *body = parse_block(ps);
        if (body == NULL) return NULL;
        fn_node->body = body;
        return fn_node;
    }

    if (tok->type == FORGE_TOK_RETURN) {
        advance_tok(ps);
        forge_ast_node_t *val = parse_expr(ps);
        if (val == NULL) return NULL;
        forge_ast_node_t *ret = alloc_node(ps, FORGE_AST_RETURN, tok->line, tok->column);
        if (ret != NULL) {
            ret->left = val;
        }
        return ret;
    }

    /* Assignment "ident = expr" or expression statement "fn_call(args)" */
    if (tok->type == FORGE_TOK_IDENT &&
        ps->pos + 1U < ps->res->tokens.count &&
        ps->res->tokens.tokens[ps->pos + 1U].type == FORGE_TOK_ASSIGN) {
        advance_tok(ps); /* consume ident */
        advance_tok(ps); /* consume '=' */
        forge_ast_node_t *val = parse_expr(ps);
        if (val == NULL) return NULL;
        forge_ast_node_t *asgn = alloc_node(ps, FORGE_AST_ASSIGN, tok->line, tok->column);
        if (asgn != NULL) {
            (void)snprintf(asgn->name, sizeof(asgn->name), "%s", tok->text);
            asgn->left = val;
        }
        return asgn;
    }

    return parse_expr(ps);
}

bool forge_parse_tokens(forge_compile_result_t *res) {
    if (res == NULL || res->tokens.count == 0U) {
        return false;
    }
    res->ast.used = 0U;
    res->ast.root = NULL;

    parser_state_t ps;
    ps.res = res;
    ps.pos = 0U;

    forge_ast_node_t *prog = alloc_node(&ps, FORGE_AST_PROGRAM, 1U, 1U);
    if (prog == NULL) return false;

    while (peek_tok(&ps)->type != FORGE_TOK_EOF) {
        while (match_tok(&ps, FORGE_TOK_SEMICOLON)) {}
        if (peek_tok(&ps)->type == FORGE_TOK_EOF) break;

        forge_ast_node_t *stmt = parse_statement(&ps);
        if (stmt == NULL) {
            return false;
        }
        if (prog->child_count >= FORGE_COMP_MAX_CHILDREN) {
            forge_comp_add_error(res, "parser", stmt->line, stmt->column, "Top-level statement limit exceeded");
            return false;
        }
        prog->children[prog->child_count++] = stmt;
        while (match_tok(&ps, FORGE_TOK_SEMICOLON)) {}
    }

    res->ast.root = prog;
    return res->error_count == 0U;
}

static void append_indent(char *buf, size_t cap, size_t *pos, int indent) {
    for (int i = 0; i < indent && *pos + 2U < cap; i++) {
        buf[(*pos)++] = ' ';
        buf[(*pos)++] = ' ';
    }
    buf[*pos] = '\0';
}

static void format_ast_rec(const forge_ast_node_t *node, int indent, char *buf, size_t cap, size_t *pos) {
    if (node == NULL || *pos + 1U >= cap) return;
    append_indent(buf, cap, pos, indent);

    int n = 0;
    switch (node->type) {
        case FORGE_AST_PROGRAM:
            n = snprintf(buf + *pos, cap - *pos, "Program (%zu stmts)\n", node->child_count);
            if (n > 0) *pos += (size_t)n;
            for (size_t i = 0U; i < node->child_count; i++) {
                format_ast_rec(node->children[i], indent + 1, buf, cap, pos);
            }
            break;
        case FORGE_AST_BLOCK:
            n = snprintf(buf + *pos, cap - *pos, "Block (%zu stmts)\n", node->child_count);
            if (n > 0) *pos += (size_t)n;
            for (size_t i = 0U; i < node->child_count; i++) {
                format_ast_rec(node->children[i], indent + 1, buf, cap, pos);
            }
            break;
        case FORGE_AST_VAR_DECL:
            n = snprintf(buf + *pos, cap - *pos, "VarDecl(%s) [line %u]\n", node->name, node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->left, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_ASSIGN:
            n = snprintf(buf + *pos, cap - *pos, "Assign(%s) [line %u]\n", node->name, node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->left, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_PRINT:
            n = snprintf(buf + *pos, cap - *pos, "Print [line %u]\n", node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->left, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_PUTC:
            n = snprintf(buf + *pos, cap - *pos, "Putc [line %u]\n", node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->left, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_IF:
            n = snprintf(buf + *pos, cap - *pos, "If [line %u]\n", node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->cond, indent + 1, buf, cap, pos);
            format_ast_rec(node->then_branch, indent + 1, buf, cap, pos);
            if (node->else_branch != NULL) {
                format_ast_rec(node->else_branch, indent + 1, buf, cap, pos);
            }
            break;
        case FORGE_AST_WHILE:
            n = snprintf(buf + *pos, cap - *pos, "While [line %u]\n", node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->cond, indent + 1, buf, cap, pos);
            format_ast_rec(node->body, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_FOR:
            n = snprintf(buf + *pos, cap - *pos, "For [line %u]\n", node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->left, indent + 1, buf, cap, pos);
            format_ast_rec(node->cond, indent + 1, buf, cap, pos);
            format_ast_rec(node->right, indent + 1, buf, cap, pos);
            format_ast_rec(node->body, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_BREAK:
            n = snprintf(buf + *pos, cap - *pos, "Break [line %u]\n", node->line);
            if (n > 0) *pos += (size_t)n;
            break;
        case FORGE_AST_CONTINUE:
            n = snprintf(buf + *pos, cap - *pos, "Continue [line %u]\n", node->line);
            if (n > 0) *pos += (size_t)n;
            break;
        case FORGE_AST_FN_DECL:
            n = snprintf(buf + *pos, cap - *pos, "FnDecl(%s, %zu params) [line %u]\n", node->name, node->param_count, node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->body, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_RETURN:
            n = snprintf(buf + *pos, cap - *pos, "Return [line %u]\n", node->line);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->left, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_BINARY_OP:
            n = snprintf(buf + *pos, cap - *pos, "BinaryOp(%d)\n", (int)node->op);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->left, indent + 1, buf, cap, pos);
            format_ast_rec(node->right, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_UNARY_OP:
            n = snprintf(buf + *pos, cap - *pos, "UnaryOp(%d)\n", (int)node->op);
            if (n > 0) *pos += (size_t)n;
            format_ast_rec(node->left, indent + 1, buf, cap, pos);
            break;
        case FORGE_AST_CALL:
            n = snprintf(buf + *pos, cap - *pos, "Call(%s, %zu args)\n", node->name, node->child_count);
            if (n > 0) *pos += (size_t)n;
            for (size_t i = 0U; i < node->child_count; i++) {
                format_ast_rec(node->children[i], indent + 1, buf, cap, pos);
            }
            break;
        case FORGE_AST_IN_EXPR:
            n = snprintf(buf + *pos, cap - *pos, "In()\n");
            if (n > 0) *pos += (size_t)n;
            break;
        case FORGE_AST_VAR_REF:
            n = snprintf(buf + *pos, cap - *pos, "VarRef(%s)\n", node->name);
            if (n > 0) *pos += (size_t)n;
            break;
        case FORGE_AST_NUMBER_LIT:
            n = snprintf(buf + *pos, cap - *pos, "Number(%d)\n", (int)node->int_val);
            if (n > 0) *pos += (size_t)n;
            break;
    }
}

void forge_format_ast(const forge_ast_node_t *node, int indent, char *buf, size_t buf_size) {
    if (buf == NULL || buf_size == 0U) return;
    buf[0] = '\0';
    size_t pos = 0U;
    format_ast_rec(node, indent, buf, buf_size, &pos);
}

