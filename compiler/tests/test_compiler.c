#include "forge_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define ASSERT_TRUE(cond)                                                          \
    do {                                                                           \
        if (!(cond)) {                                                             \
            fprintf(stderr, "  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            g_tests_failed++;                                                      \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_EQ(actual, expected)                                                \
    do {                                                                           \
        unsigned long long a_ = (unsigned long long)(actual);                      \
        unsigned long long e_ = (unsigned long long)(expected);                    \
        if (a_ != e_) {                                                            \
            fprintf(                                                               \
                stderr,                                                            \
                "  [FAIL] %s:%d: %s == 0x%llX (%llu), expected 0x%llX (%llu)\n",   \
                __FILE__, __LINE__, #actual, a_, a_, e_, e_                        \
            );                                                                     \
            g_tests_failed++;                                                      \
            return;                                                                \
        }                                                                          \
    } while (0)

#define RUN_TEST(fn)                                                               \
    do {                                                                           \
        int before_ = g_tests_failed;                                              \
        g_tests_run++;                                                             \
        fn();                                                                      \
        if (g_tests_failed == before_) {                                           \
            printf("  [PASS] %s\n", #fn);                                          \
        }                                                                          \
    } while (0)

static bool compile_and_execute(const char *src, forge_cpu_t *out_cpu, forge_compile_result_t *out_comp) {
    if (!forge_compile_source(src, out_comp)) {
        return false;
    }
    forge_cpu_init(out_cpu);
    if (forge_cpu_load_fbin(out_cpu, out_comp->asm_result.fbin, out_comp->asm_result.fbin_size) != FORGE_OK) {
        return false;
    }
    return forge_cpu_run(out_cpu) == FORGE_OK;
}

/* 1. Test Lexer and AST Formatter */
static void test_lexer_and_ast(void) {
    const char *src =
        "let x = 0x10 + 'A'\n"
        "if x >= 20 {\n"
        "    print x << 1\n"
        "}\n";

    forge_compile_result_t *res = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    ASSERT_TRUE(res != NULL);
    forge_compile_result_init(res);

    ASSERT_TRUE(forge_lex_source(src, res));
    ASSERT_TRUE(res->tokens.count > 10U);
    ASSERT_EQ(res->tokens.tokens[0].type, FORGE_TOK_LET);
    ASSERT_EQ(res->tokens.tokens[1].type, FORGE_TOK_IDENT);
    ASSERT_EQ(res->tokens.tokens[3].type, FORGE_TOK_NUMBER);
    ASSERT_EQ(res->tokens.tokens[3].int_val, 16);

    ASSERT_TRUE(forge_parse_tokens(res));
    ASSERT_TRUE(res->ast.root != NULL);

    char ast_buf[2048];
    forge_format_ast(res->ast.root, 0, ast_buf, sizeof(ast_buf));
    ASSERT_TRUE(strstr(ast_buf, "VarDecl(x)") != NULL);
    ASSERT_TRUE(strstr(ast_buf, "If") != NULL);

    free(res);
}

/* 2. Test Section 6 Specification Example */
static void test_spec_example_compile_and_run(void) {
    const char *src =
        "let a = 10\n"
        "let b = 5\n"
        "\n"
        "fn add(x, y) {\n"
        "    return x + y\n"
        "}\n"
        "\n"
        "let result = add(a, b)\n"
        "print result\n";

    forge_compile_result_t *comp = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    forge_cpu_t cpu;
    ASSERT_TRUE(comp != NULL);

    ASSERT_TRUE(compile_and_execute(src, &cpu, comp));
    ASSERT_EQ(cpu.io.int_output_count, 1U);
    ASSERT_EQ(cpu.io.int_output_log[0], 15U);
    ASSERT_TRUE(strcmp(cpu.io.console_buf, "15\n") == 0);
    ASSERT_TRUE(comp->source_map_count > 0U);

    free(comp);
}

/* 3. Test Operator Precedence, Bitwise, Shifts, and Comparisons */
static void test_expressions_and_precedence(void) {
    const char *src =
        "let p1 = 2 + 3 * 4\n"          /* 14 */
        "let p2 = (2 + 3) * 4\n"        /* 20 */
        "let p3 = 100 / 5 - 3\n"        /* 17 */
        "let p4 = (0x0F & 0x05) | 0x10\n" /* 0x15 = 21 */
        "let p5 = (3 << 3) >> 1\n"      /* 12 */
        "print p1\n"
        "print p2\n"
        "print p3\n"
        "print p4\n"
        "print p5\n"
        "print (p2 > p1)\n"             /* 1 */
        "print (p1 == 99)\n";           /* 0 */

    forge_compile_result_t *comp = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    forge_cpu_t cpu;
    ASSERT_TRUE(comp != NULL);

    ASSERT_TRUE(compile_and_execute(src, &cpu, comp));
    ASSERT_EQ(cpu.io.int_output_count, 7U);
    ASSERT_EQ(cpu.io.int_output_log[0], 14U);
    ASSERT_EQ(cpu.io.int_output_log[1], 20U);
    ASSERT_EQ(cpu.io.int_output_log[2], 17U);
    ASSERT_EQ(cpu.io.int_output_log[3], 21U);
    ASSERT_EQ(cpu.io.int_output_log[4], 12U);
    ASSERT_EQ(cpu.io.int_output_log[5], 1U);
    ASSERT_EQ(cpu.io.int_output_log[6], 0U);

    free(comp);
}

/* 4. Test Conditionals (if/else) and While Loops */
static void test_conditionals_and_loops(void) {
    const char *src =
        "let sum = 0\n"
        "let i = 1\n"
        "while i <= 10 {\n"
        "    if i == 5 {\n"
        "        sum = sum + 100\n"
        "    } else {\n"
        "        sum = sum + i\n"
        "    }\n"
        "    i = i + 1\n"
        "}\n"
        "print sum\n"
        "putc 'O'\n"
        "putc 'K'\n";

    forge_compile_result_t *comp = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    forge_cpu_t cpu;
    ASSERT_TRUE(comp != NULL);

    ASSERT_TRUE(compile_and_execute(src, &cpu, comp));
    /* Sum 1..10 is 55; replacing 5 with 100 gives 55 - 5 + 100 = 150 */
    ASSERT_EQ(cpu.io.int_output_count, 1U);
    ASSERT_EQ(cpu.io.int_output_log[0], 150U);
    ASSERT_TRUE(strcmp(cpu.io.console_buf, "150\nOK") == 0);

    free(comp);
}

/* 5. Test Nested Calls, Local Variables, and Recursive Functions */
static void test_functions_locals_and_recursion(void) {
    const char *src =
        "fn compute_linear(x, m, b) {\n"
        "    let prod = m * x\n"
        "    let res = prod + b\n"
        "    return res\n"
        "}\n"
        "\n"
        "fn fib(n) {\n"
        "    if n <= 1 {\n"
        "        return n\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "\n"
        "print compute_linear(4, 3, 2)\n"  /* 3*4 + 2 = 14 */
        "print fib(6)\n";                  /* fib(6) = 8 */

    forge_compile_result_t *comp = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    forge_cpu_t cpu;
    ASSERT_TRUE(comp != NULL);

    ASSERT_TRUE(compile_and_execute(src, &cpu, comp));
    ASSERT_EQ(cpu.io.int_output_count, 2U);
    ASSERT_EQ(cpu.io.int_output_log[0], 14U);
    ASSERT_EQ(cpu.io.int_output_log[1], 8U);

    free(comp);
}

/* 6. Test Example .fl Files on Disk */
static void test_example_fl_files(void) {
    forge_compile_result_t *comp = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    forge_cpu_t cpu;
    ASSERT_TRUE(comp != NULL);

    /* add_fn.fl */
    ASSERT_TRUE(forge_compile_file(FORGE_SOURCE_DIR "/examples/programs/add_fn.fl", comp));
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, comp->asm_result.fbin, comp->asm_result.fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.io.int_output_log[0], 15U);

    /* factorial_rec.fl (5! = 120, 6! = 720) */
    ASSERT_TRUE(forge_compile_file(FORGE_SOURCE_DIR "/examples/programs/factorial_rec.fl", comp));
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, comp->asm_result.fbin, comp->asm_result.fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.io.int_output_count, 2U);
    ASSERT_EQ(cpu.io.int_output_log[0], 120U);
    ASSERT_EQ(cpu.io.int_output_log[1], 720U);

    free(comp);
}

/* 7. Test Semantic and Syntax Error Reporting */
static void test_compiler_error_reporting(void) {
    forge_compile_result_t *comp = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    ASSERT_TRUE(comp != NULL);

    /* Undeclared variable on line 2 */
    ASSERT_TRUE(!forge_compile_source("let a = 1\nprint unknown_var\n", comp));
    ASSERT_EQ(comp->error_count, 1U);
    ASSERT_EQ(comp->errors[0].line, 2U);

    /* Wrong number of function arguments on line 5 */
    const char *bad_args =
        "fn add(x, y) {\n"
        "    return x + y\n"
        "}\n"
        "let r = add(10)\n";
    ASSERT_TRUE(!forge_compile_source(bad_args, comp));
    ASSERT_EQ(comp->error_count, 1U);
    ASSERT_EQ(comp->errors[0].line, 4U);

    /* Return outside of function on line 2 */
    ASSERT_TRUE(!forge_compile_source("let x = 5\nreturn x\n", comp));
    ASSERT_EQ(comp->error_count, 1U);
    ASSERT_EQ(comp->errors[0].line, 2U);

    /* Break outside of loop on line 2 */
    ASSERT_TRUE(!forge_compile_source("let x = 5\nbreak\n", comp));
    ASSERT_EQ(comp->error_count, 1U);
    ASSERT_EQ(comp->errors[0].line, 2U);

    free(comp);
}

/* 8. Test For-Loops, Break, Continue, Modulo (%), and Logical Operators (&&, ||, !) */
static void test_for_break_continue_and_logical_ops(void) {
    forge_compile_result_t *comp = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    forge_cpu_t cpu;
    ASSERT_TRUE(comp != NULL);

    ASSERT_TRUE(forge_compile_file(FORGE_SOURCE_DIR "/examples/programs/primes.fl", comp));
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, comp->asm_result.fbin, comp->asm_result.fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);

    /* First 6 primes: 2, 3, 5, 7, 11, 13 */
    ASSERT_EQ(cpu.io.int_output_count, 6U);
    ASSERT_EQ(cpu.io.int_output_log[0], 2U);
    ASSERT_EQ(cpu.io.int_output_log[1], 3U);
    ASSERT_EQ(cpu.io.int_output_log[2], 5U);
    ASSERT_EQ(cpu.io.int_output_log[3], 7U);
    ASSERT_EQ(cpu.io.int_output_log[4], 11U);
    ASSERT_EQ(cpu.io.int_output_log[5], 13U);

    /* Test short-circuit && and || */
    const char *logic_src =
        "let a = (1 && 1)\n"
        "let b = (1 && 0)\n"
        "let c = (0 || 5)\n"
        "let d = (0 || 0)\n"
        "print a\n"
        "print b\n"
        "print c\n"
        "print d\n";
    ASSERT_TRUE(compile_and_execute(logic_src, &cpu, comp));
    ASSERT_EQ(cpu.io.int_output_count, 4U);
    ASSERT_EQ(cpu.io.int_output_log[0], 1U);
    ASSERT_EQ(cpu.io.int_output_log[1], 0U);
    ASSERT_EQ(cpu.io.int_output_log[2], 1U);
    ASSERT_EQ(cpu.io.int_output_log[3], 0U);

    free(comp);
}

/* 9. Test Bridge /api/compile, Source-Level Stepping, Call Stack Frames & Live Variables */
#include "forge_bridge.h"
static void test_bridge_compile_sourcemap_and_stepping(void) {
    forge_bridge_session_t *session = (forge_bridge_session_t *)malloc(sizeof(forge_bridge_session_t));
    char *json_buf = (char *)malloc(131072U);
    ASSERT_TRUE(session != NULL && json_buf != NULL);
    forge_bridge_init(session);

    const char *body =
        "{\"source\":\"fn fact(n) {\\n  if n <= 1 {\\n    return 1\\n  }\\n  return n * fact(n - 1)\\n}\\nlet ans = fact(4)\\nprint ans\\n\"}";

    int status = forge_bridge_handle_request(session, "POST", "/api/compile", body, json_buf, 131072U);
    ASSERT_EQ(status, 200);
    ASSERT_TRUE(strstr(json_buf, "\"mode\":\"forgelang\"") != NULL);
    ASSERT_TRUE(strstr(json_buf, "\"sourceMap\":[") != NULL);
    ASSERT_TRUE(strstr(json_buf, "\"globals\":[{\"name\":\"ans\"") != NULL);

    /* Set a source-line breakpoint on line 3 (return 1 base case inside fact) */
    status = forge_bridge_handle_request(session, "POST", "/api/breakpoint", "{\"sourceLine\":3,\"enabled\":true}", json_buf, 131072U);
    ASSERT_EQ(status, 200);
    ASSERT_EQ(session->cpu.breakpoint_count, 1U);

    /* Run until base-case breakpoint inside recursive fact(4) -> fact(3) -> fact(2) -> fact(1) */
    status = forge_bridge_handle_request(session, "POST", "/api/run", "{\"maxSteps\":5000}", json_buf, 131072U);
    ASSERT_EQ(status, 200);
    ASSERT_EQ(session->cpu.call_depth, 4U);
    ASSERT_EQ(forge_bridge_source_line_for_pc(session, session->cpu.pc), 3U);
    ASSERT_TRUE(strstr(json_buf, "\"function\":\"fact\"") != NULL);
    ASSERT_TRUE(strstr(json_buf, "\"name\":\"n\",\"kind\":\"param\"") != NULL);

    /* Step out one call frame (from depth 4 back to depth 3) */
    status = forge_bridge_handle_request(session, "POST", "/api/step", "{\"mode\":\"out\"}", json_buf, 131072U);
    ASSERT_EQ(status, 200);
    ASSERT_EQ(session->cpu.call_depth, 3U);

    /* Clear breakpoints and run to completion */
    forge_bridge_handle_request(session, "POST", "/api/breakpoint", "{\"action\":\"clear\"}", json_buf, 131072U);
    forge_bridge_handle_request(session, "POST", "/api/run", "{\"maxSteps\":5000}", json_buf, 131072U);
    ASSERT_TRUE(session->cpu.halted);
    ASSERT_EQ(session->cpu.io.int_output_log[0], 24U);
    ASSERT_TRUE(strstr(json_buf, "\"name\":\"ans\",\"address\":32768,\"value\":24") != NULL);

    forge_bridge_destroy(session);
    free(json_buf);
    free(session);
}

/* 10. Test ForgeLang OS intrinsics (alloc, peek, poke, draw_pixel, get_pixel, clear_screen, key_ready, read_key, exit) */
static void test_forgelang_os_intrinsics_and_graphics(void) {
    const char *src =
        "clear_screen(1)\n"
        "let arr = alloc(6)\n"
        "poke(arr + 0, 10)\n"
        "poke(arr + 2, 20)\n"
        "poke(arr + 4, 30)\n"
        "let total = peek(arr + 0) + peek(arr + 2) + peek(arr + 4)\n"
        "print total\n"
        "draw_pixel(5, 4, 14)\n"
        "print get_pixel(5, 4)\n"
        "if key_ready() {\n"
        "    print read_key()\n"
        "}\n"
        "exit(9)\n";

    forge_compile_result_t *res = (forge_compile_result_t *)calloc(1U, sizeof(forge_compile_result_t));
    ASSERT_TRUE(res != NULL);
    ASSERT_TRUE(forge_compile_source(src, res));

    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, res->asm_result.fbin, res->asm_result.fbin_size), FORGE_OK);
    forge_io_push_input(&cpu.io, 65U); /* 'A' */
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);

    ASSERT_TRUE(cpu.halted);
    ASSERT_EQ(cpu.os.exit_code, 9U);
    ASSERT_EQ(cpu.os.alloc_count, 1U);
    ASSERT_EQ(cpu.os.heap_brk, (uint16_t)(FORGE_DYNAMIC_HEAP_START + 6U));
    ASSERT_EQ(forge_os_get_pixel(&cpu, 0U, 0U), 1U);
    ASSERT_EQ(forge_os_get_pixel(&cpu, 5U, 4U), 14U);
    ASSERT_EQ(cpu.io.int_output_count, 3U);
    ASSERT_EQ(cpu.io.int_output_log[0], 60U); /* 10 + 20 + 30 */
    ASSERT_EQ(cpu.io.int_output_log[1], 14U); /* get_pixel(5, 4) */
    ASSERT_EQ(cpu.io.int_output_log[2], 65U); /* read_key() == 'A' */
    free(res);

    /* Also compile and run examples/programs/graphics_os.fl */
    forge_compile_result_t *g_res = (forge_compile_result_t *)calloc(1U, sizeof(forge_compile_result_t));
    ASSERT_TRUE(g_res != NULL);
    ASSERT_TRUE(forge_compile_file(FORGE_SOURCE_DIR "/examples/programs/graphics_os.fl", g_res));
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, g_res->asm_result.fbin, g_res->asm_result.fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_TRUE(cpu.halted);
    ASSERT_EQ(cpu.os.exit_code, 0U);
    ASSERT_EQ(cpu.io.int_output_log[0], 86U); /* 8 + 14 + 20 + 26 + 18 */
    ASSERT_TRUE(cpu.os.fb_writes > 100U);
    free(g_res);
}

int main(void) {
    printf("Running ForgeLang Compiler Test Suite...\n");

    RUN_TEST(test_lexer_and_ast);
    RUN_TEST(test_spec_example_compile_and_run);
    RUN_TEST(test_expressions_and_precedence);
    RUN_TEST(test_conditionals_and_loops);
    RUN_TEST(test_functions_locals_and_recursion);
    RUN_TEST(test_example_fl_files);
    RUN_TEST(test_compiler_error_reporting);
    RUN_TEST(test_for_break_continue_and_logical_ops);
    RUN_TEST(test_bridge_compile_sourcemap_and_stepping);
    RUN_TEST(test_forgelang_os_intrinsics_and_graphics);

    printf(
        "\nTest Summary: %d/%d passed (%d failed)\n",
        g_tests_run - g_tests_failed,
        g_tests_run,
        g_tests_failed
    );
    return (g_tests_failed == 0) ? 0 : 1;
}


