#include "forge_asm.h"
#include "forge_bridge.h"
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

/* 1. Test specification example: MOV, ADD, OUT, HALT */
static void test_spec_example_assemble_and_run(void) {
    const char *src =
        "MOV R0, 10\n"
        "MOV R1, 5\n"
        "ADD R0, R1\n"
        "OUT R0\n"
        "HALT\n";

    forge_asm_result_t *res = (forge_asm_result_t *)malloc(sizeof(forge_asm_result_t));
    ASSERT_TRUE(res != NULL);
    ASSERT_TRUE(forge_assemble_source(src, res));
    ASSERT_EQ(res->error_count, 0U);
    ASSERT_EQ(res->code_size, 20U); /* 5 instructions * 4 bytes */
    ASSERT_EQ(res->line_map_count, 5U);

    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, res->fbin, res->fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_TRUE(cpu.halted);
    ASSERT_EQ(cpu.r[FORGE_R0], 15U);
    ASSERT_EQ(cpu.io.int_output_count, 1U);
    ASSERT_EQ(cpu.io.int_output_log[0], 15U);
    ASSERT_TRUE(strcmp(cpu.io.console_buf, "15\n") == 0);

    free(res);
}

/* 2. Test literals (decimal, negative, hex, binary, char) and FP alias */
static void test_literals_and_registers(void) {
    const char *src =
        "mov r0, 0x1234      ; hex literal\n"
        "mov r1, 0b00001111  ; binary literal (15)\n"
        "mov r2, -10         ; negative decimal (0xFFF6)\n"
        "mov r3, 'A'         ; ASCII 65\n"
        "mov fp, 0xEFFE      ; FP alias for R7\n"
        "halt\n";

    forge_asm_result_t *res = (forge_asm_result_t *)malloc(sizeof(forge_asm_result_t));
    ASSERT_TRUE(res != NULL);
    ASSERT_TRUE(forge_assemble_source(src, res));

    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, res->fbin, res->fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R0], 0x1234U);
    ASSERT_EQ(cpu.r[FORGE_R1], 15U);
    ASSERT_EQ(cpu.r[FORGE_R2], (uint16_t)(-10));
    ASSERT_EQ(cpu.r[FORGE_R3], 65U);
    ASSERT_EQ(cpu.r[FORGE_R7], 0xEFFEU);

    free(res);
}

/* 3. Test labels, forward references, .equ constants, and .word/.byte directives */
static void test_labels_forward_refs_and_directives(void) {
    const char *src =
        ".org 0x0500\n"
        ".equ MULTIPLIER, 6\n"
        ".entry start\n"
        "data_val:\n"
        "    .word 7\n"
        "start:\n"
        "    LOAD R0, [data_val]       ; R0 = 7\n"
        "    MOV  R1, MULTIPLIER       ; R1 = 6\n"
        "    JMP  do_multiply          ; Forward reference!\n"
        "    MOV  R0, 999              ; Skipped\n"
        "do_multiply:\n"
        "    MUL  R0, R1               ; R0 = 42\n"
        "    STORE R0, [result_slot]   ; Forward reference to data label\n"
        "    LOAD R2, [result_slot]\n"
        "    OUT  R2\n"
        "    HALT\n"
        "result_slot:\n"
        "    .word 0\n";

    forge_asm_result_t *res = (forge_asm_result_t *)malloc(sizeof(forge_asm_result_t));
    ASSERT_TRUE(res != NULL);
    ASSERT_TRUE(forge_assemble_source(src, res));
    ASSERT_EQ(res->load_addr, 0x0500U);
    ASSERT_EQ(res->entry_pc, 0x0502U);

    const forge_asm_symbol_t *sym = forge_asm_find_symbol(res, "MULTIPLIER");
    ASSERT_TRUE(sym != NULL && sym->is_constant && sym->value == 6U);

    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, res->fbin, res->fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R2], 42U);
    ASSERT_EQ(cpu.io.int_output_log[0], 42U);

    free(res);
}

/* 4. Test indexed memory operands [Rs], [Rs + offset], [FP - offset] */
static void test_indexed_memory_modes(void) {
    const char *src =
        "MOV FP, 0x8100\n"
        "MOV R0, 0x1122\n"
        "MOV R1, 0x3344\n"
        "STORE R0, [FP + 4]\n"
        "STORE R1, [FP - 2]\n"
        "LOAD  R2, [FP + 4]\n"
        "LOAD  R3, [FP - 2]\n"
        "HALT\n";

    forge_asm_result_t *res = (forge_asm_result_t *)malloc(sizeof(forge_asm_result_t));
    ASSERT_TRUE(res != NULL);
    ASSERT_TRUE(forge_assemble_source(src, res));

    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, res->fbin, res->fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R2], 0x1122U);
    ASSERT_EQ(cpu.r[FORGE_R3], 0x3344U);

    free(res);
}

/* 5. Test example assembly files from disk (Fibonacci, Recursive Factorial, Console String) */
static void test_example_files_end_to_end(void) {
    forge_asm_result_t *res = (forge_asm_result_t *)malloc(sizeof(forge_asm_result_t));
    ASSERT_TRUE(res != NULL);

    /* Test fibonacci.asm */
    ASSERT_TRUE(forge_assemble_file(FORGE_SOURCE_DIR "/examples/assembly/fibonacci.asm", res));
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, res->fbin, res->fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.io.int_output_count, 8U);
    uint16_t expected_fib[8] = {0, 1, 1, 2, 3, 5, 8, 13};
    for (size_t i = 0; i < 8U; i++) {
        ASSERT_EQ(cpu.io.int_output_log[i], expected_fib[i]);
    }

    /* Test factorial_rec.asm (5! = 120) */
    ASSERT_TRUE(forge_assemble_file(FORGE_SOURCE_DIR "/examples/assembly/factorial_rec.asm", res));
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, res->fbin, res->fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.io.int_output_count, 1U);
    ASSERT_EQ(cpu.io.int_output_log[0], 120U);

    /* Test hello_console.asm */
    ASSERT_TRUE(forge_assemble_file(FORGE_SOURCE_DIR "/examples/assembly/hello_console.asm", res));
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_cpu_load_fbin(&cpu, res->fbin, res->fbin_size), FORGE_OK);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_TRUE(strcmp(cpu.io.console_buf, "Hello, Forge-16!\n") == 0);

    free(res);
}

/* 6. Test binary disassembly output */
static void test_disassembly(void) {
    const char *src =
        "MOV R0, 10\n"
        "ADD R0, R1\n"
        "HALT\n";

    forge_asm_result_t *res = (forge_asm_result_t *)malloc(sizeof(forge_asm_result_t));
    ASSERT_TRUE(res != NULL);
    ASSERT_TRUE(forge_assemble_source(src, res));

    char dis_buf[2048];
    ASSERT_EQ(
        forge_disassemble_binary(res->fbin, res->fbin_size, FORGE_DEFAULT_PC, dis_buf, sizeof(dis_buf)),
        FORGE_OK
    );
    ASSERT_TRUE(strstr(dis_buf, "MOV R0, 0x000A") != NULL);
    ASSERT_TRUE(strstr(dis_buf, "ADD R0, R1") != NULL);
    ASSERT_TRUE(strstr(dis_buf, "HALT") != NULL);

    free(res);
}

/* 7. Test syntax error reporting with accurate line numbers */
static void test_syntax_errors_with_line_numbers(void) {
    forge_asm_result_t *res = (forge_asm_result_t *)malloc(sizeof(forge_asm_result_t));
    ASSERT_TRUE(res != NULL);

    /* Unknown mnemonic on line 3 */
    const char *bad_mnem =
        "MOV R0, 1\n"
        "MOV R1, 2\n"
        "FOOBAR R0, R1\n"
        "HALT\n";
    ASSERT_TRUE(!forge_assemble_source(bad_mnem, res));
    ASSERT_EQ(res->error_count, 1U);
    ASSERT_EQ(res->errors[0].line, 3U);

    /* Duplicate label on line 4 */
    const char *dup_label =
        "loop:\n"
        "  MOV R0, 1\n"
        "  ADD R0, 2\n"
        "loop:\n"
        "  HALT\n";
    ASSERT_TRUE(!forge_assemble_source(dup_label, res));
    ASSERT_EQ(res->error_count, 1U);
    ASSERT_EQ(res->errors[0].line, 4U);

    /* Undefined label on line 2 */
    const char *undef_label =
        "MOV R0, 10\n"
        "JMP missing_target\n"
        "HALT\n";
    ASSERT_TRUE(!forge_assemble_source(undef_label, res));
    ASSERT_EQ(res->error_count, 1U);
    ASSERT_EQ(res->errors[0].line, 2U);

    /* Invalid register on line 2 */
    const char *bad_reg =
        "MOV R0, 10\n"
        "ADD R9, R0\n";
    ASSERT_TRUE(!forge_assemble_source(bad_reg, res));
    ASSERT_EQ(res->error_count, 1U);
    ASSERT_EQ(res->errors[0].line, 2U);

    /* Malformed memory operand on line 1 */
    const char *bad_mem =
        "LOAD R0, 0x8000\n";
    ASSERT_TRUE(!forge_assemble_source(bad_mem, res));
    ASSERT_EQ(res->error_count, 1U);
    ASSERT_EQ(res->errors[0].line, 1U);

    free(res);
}

/* 8. Test Native C Bridge session and HTTP/JSON request handling */
static void test_bridge_json_api(void) {
    forge_bridge_session_t *session = (forge_bridge_session_t *)malloc(sizeof(forge_bridge_session_t));
    ASSERT_TRUE(session != NULL);
    forge_bridge_init(session);

    char *json_out = (char *)malloc(65536U);
    ASSERT_TRUE(json_out != NULL);

    int status = forge_bridge_handle_request(
        session,
        "POST",
        "/api/assemble",
        "{\"source\":\"MOV R0, 10\\nMOV R1, 20\\nADD R0, R1\\nOUT R0\\nHALT\\n\"}",
        json_out,
        65536U
    );
    ASSERT_EQ(status, 200);
    ASSERT_TRUE(strstr(json_out, "\"success\":true") != NULL);
    ASSERT_TRUE(strstr(json_out, "\"pc\":1024") != NULL);

    /* Step once: MOV R0, 10 -> R0 becomes 10, PC becomes 1028 */
    status = forge_bridge_handle_request(session, "POST", "/api/step", "{\"count\":1}", json_out, 65536U);
    ASSERT_EQ(status, 200);
    ASSERT_EQ(session->cpu.r[FORGE_R0], 10U);
    ASSERT_EQ(session->prev_r[FORGE_R0], 0U);
    ASSERT_EQ(session->cpu.pc, 1028U);

    /* Edit memory at 0x8000 via bridge */
    status = forge_bridge_handle_request(
        session,
        "POST",
        "/api/memory/write",
        "{\"address\":32768,\"value\":4660,\"width\":16}",
        json_out,
        65536U
    );
    ASSERT_EQ(status, 200);
    uint16_t mval = 0U;
    ASSERT_EQ(forge_mem_peek16(&session->cpu.mem, 0x8000U, &mval), FORGE_OK);
    ASSERT_EQ(mval, 4660U);

    /* Run to completion */
    status = forge_bridge_handle_request(session, "POST", "/api/run", "{\"maxSteps\":100}", json_out, 65536U);
    ASSERT_EQ(status, 200);
    ASSERT_TRUE(session->cpu.halted);
    ASSERT_EQ(session->cpu.r[FORGE_R0], 30U);
    ASSERT_TRUE(strstr(json_out, "\"console\":\"30\\n\"") != NULL);

    free(json_out);
    free(session);
}

int main(void) {
    printf("Running Forge-16 Assembler Test Suite...\n");

    RUN_TEST(test_spec_example_assemble_and_run);
    RUN_TEST(test_literals_and_registers);
    RUN_TEST(test_labels_forward_refs_and_directives);
    RUN_TEST(test_indexed_memory_modes);
    RUN_TEST(test_example_files_end_to_end);
    RUN_TEST(test_disassembly);
    RUN_TEST(test_syntax_errors_with_line_numbers);
    RUN_TEST(test_bridge_json_api);

    printf(
        "\nTest Summary: %d/%d passed (%d failed)\n",
        g_tests_run - g_tests_failed,
        g_tests_run,
        g_tests_failed
    );
    return (g_tests_failed == 0) ? 0 : 1;
}

