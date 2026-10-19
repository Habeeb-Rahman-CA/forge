#include "forge_cpu.h"
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

/* 1. Test CPU initialization and reset */
static void test_cpu_init_and_reset(void) {
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);

    ASSERT_EQ(cpu.pc, FORGE_DEFAULT_PC);
    ASSERT_EQ(cpu.sp, FORGE_DEFAULT_SP);
    ASSERT_EQ(cpu.flags, 0U);
    ASSERT_TRUE(!cpu.halted);
    ASSERT_EQ(cpu.error, FORGE_OK);

    for (uint8_t i = 0; i < FORGE_NUM_REGS; i++) {
        ASSERT_EQ(forge_cpu_get_reg(&cpu, i), 0U);
        ASSERT_EQ(forge_cpu_set_reg(&cpu, i, (uint16_t)(0x1000U + i)), FORGE_OK);
        ASSERT_EQ(forge_cpu_get_reg(&cpu, i), (uint16_t)(0x1000U + i));
    }

    ASSERT_EQ(forge_mem_write16(&cpu.mem, 0x2000U, 0xCAFEU), FORGE_OK);
    forge_cpu_reset(&cpu, true);
    ASSERT_EQ(forge_cpu_get_reg(&cpu, FORGE_R0), 0U);

    uint16_t preserved = 0U;
    ASSERT_EQ(forge_mem_read16(&cpu.mem, 0x2000U, &preserved), FORGE_OK);
    ASSERT_EQ(preserved, 0xCAFEU);

    forge_cpu_reset(&cpu, false);
    ASSERT_EQ(forge_mem_read16(&cpu.mem, 0x2000U, &preserved), FORGE_OK);
    ASSERT_EQ(preserved, 0x0000U);
}

/* 2. Test instruction encoding, decoding, and invalid encodings */
static void test_decoder_and_invalid_instructions(void) {
    uint16_t w0 = 0U, w1 = 0U;
    forge_encode_instruction(FORGE_OP_ADD, FORGE_MODE_IMM, FORGE_R3, FORGE_R0, 0x1234U, &w0, &w1);

    forge_instruction_t decoded;
    ASSERT_EQ(forge_decode_instruction(w0, w1, &decoded), FORGE_OK);
    ASSERT_EQ(decoded.opcode, FORGE_OP_ADD);
    ASSERT_EQ(decoded.mode, FORGE_MODE_IMM);
    ASSERT_EQ(decoded.rd, FORGE_R3);
    ASSERT_EQ(decoded.imm16, 0x1234U);

    char disasm[64];
    forge_disassemble_instruction(&decoded, disasm, sizeof(disasm));
    ASSERT_TRUE(strcmp(disasm, "ADD R3, 0x1234") == 0);

    /* Non-zero reserved bits must fail decoding */
    ASSERT_EQ(forge_decode_instruction((uint16_t)(w0 | 0x0001U), w1, &decoded), FORGE_ERR_INVALID_INSTRUCTION);

    /* Out-of-range opcode (0x3F) must fail decoding */
    uint16_t bad_op = (uint16_t)(0x3FU << 10);
    ASSERT_EQ(forge_decode_instruction(bad_op, 0U, &decoded), FORGE_ERR_INVALID_INSTRUCTION);

    /* Illegal addressing mode for LOAD (REG mode is illegal for LOAD) */
    uint16_t bad_load = forge_encode_word0(FORGE_OP_LOAD, FORGE_MODE_REG, FORGE_R0, FORGE_R1);
    ASSERT_EQ(forge_decode_instruction(bad_load, 0U, &decoded), FORGE_ERR_INVALID_INSTRUCTION);

    /* Verify CPU halts with FORGE_ERR_INVALID_INSTRUCTION when executing bad opcode */
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    ASSERT_EQ(forge_mem_write16(&cpu.mem, FORGE_DEFAULT_PC, bad_op), FORGE_OK);
    ASSERT_EQ(forge_cpu_step(&cpu), FORGE_ERR_INVALID_INSTRUCTION);
    ASSERT_TRUE(cpu.halted);
    ASSERT_EQ(cpu.fault_pc, FORGE_DEFAULT_PC);
}

/* 3. Test data movement: MOV, LOAD, STORE (Direct & Indexed) */
static void test_data_movement_instructions(void) {
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    uint16_t pc = FORGE_DEFAULT_PC;

    /* MOV R0, 0xABCD */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R0, 0, 0xABCDU);
    /* MOV R1, R0 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_REG, FORGE_R1, FORGE_R0, 0U);
    /* STORE R1, [0x8000] */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_STORE, FORGE_MODE_MEM_DIR, FORGE_R1, 0, 0x8000U);
    /* LOAD R2, [0x8000] */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_LOAD, FORGE_MODE_MEM_DIR, FORGE_R2, 0, 0x8000U);
    /* MOV R3, 0x8000 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R3, 0, 0x8000U);
    /* STORE R0, [R3 + 4] */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_STORE, FORGE_MODE_MEM_IDX, FORGE_R0, FORGE_R3, 4U);
    /* LOAD R4, [R3 + 4] */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_LOAD, FORGE_MODE_MEM_IDX, FORGE_R4, FORGE_R3, 4U);
    /* Negative offset: MOV R5, 0x8008; LOAD R6, [R5 - 4] -> [0x8004] */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R5, 0, 0x8008U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_LOAD, FORGE_MODE_MEM_IDX, FORGE_R6, FORGE_R5, (uint16_t)(-4));
    /* NOP & HALT */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_NOP, FORGE_MODE_REG, 0, 0, 0U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U);

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_TRUE(cpu.halted);
    ASSERT_EQ(cpu.r[FORGE_R0], 0xABCDU);
    ASSERT_EQ(cpu.r[FORGE_R1], 0xABCDU);
    ASSERT_EQ(cpu.r[FORGE_R2], 0xABCDU);
    ASSERT_EQ(cpu.r[FORGE_R4], 0xABCDU);
    ASSERT_EQ(cpu.r[FORGE_R6], 0xABCDU);
}

/* 4. Test arithmetic and flag behavior: ADD, SUB, MUL, DIV */
static void test_arithmetic_and_flags(void) {
    /* ADD: unsigned carry & zero */
    forge_alu_result_t r = forge_alu_add(0xFFFFU, 0x0001U);
    ASSERT_EQ(r.result, 0x0000U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_Z) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_C) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_N) == 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_V) == 0U);

    /* ADD: signed positive overflow (0x7FFF + 1 = 0x8000) */
    r = forge_alu_add(0x7FFFU, 0x0001U);
    ASSERT_EQ(r.result, 0x8000U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_N) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_V) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_C) == 0U);

    /* SUB: unsigned borrow (3 - 5 = -2 = 0xFFFE) */
    r = forge_alu_sub(3U, 5U);
    ASSERT_EQ(r.result, 0xFFFEU);
    ASSERT_TRUE((r.flags & FORGE_FLAG_N) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_C) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_V) == 0U);

    /* SUB: signed overflow (-32768 - 1 = +32767) */
    r = forge_alu_sub(0x8000U, 0x0001U);
    ASSERT_EQ(r.result, 0x7FFFU);
    ASSERT_TRUE((r.flags & FORGE_FLAG_V) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_N) == 0U);

    /* MUL: normal and overflow */
    r = forge_alu_mul(12U, 11U);
    ASSERT_EQ(r.result, 132U);
    ASSERT_EQ(r.flags, 0U);

    r = forge_alu_mul(0x1000U, 0x0010U); /* 0x10000 -> low16 = 0, C=1, V=1 */
    ASSERT_EQ(r.result, 0x0000U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_Z) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_C) != 0U);
    ASSERT_TRUE((r.flags & FORGE_FLAG_V) != 0U);

    /* DIV: normal and division by zero */
    r = forge_alu_div(100U, 7U);
    ASSERT_EQ(r.error, FORGE_OK);
    ASSERT_EQ(r.result, 14U);

    r = forge_alu_div(42U, 0U);
    ASSERT_EQ(r.error, FORGE_ERR_DIVISION_BY_ZERO);

    /* Execute arithmetic instructions in CPU including divide-by-zero fault */
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    uint16_t pc = FORGE_DEFAULT_PC;
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R0, 0, 20U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_ADD, FORGE_MODE_IMM, FORGE_R0, 0, 15U); /* 35 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_SUB, FORGE_MODE_IMM, FORGE_R0, 0, 5U);  /* 30 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MUL, FORGE_MODE_IMM, FORGE_R0, 0, 3U);  /* 90 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_DIV, FORGE_MODE_IMM, FORGE_R0, 0, 9U);  /* 10 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_DIV, FORGE_MODE_IMM, FORGE_R0, 0, 0U);  /* Fault! */

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_ERR_DIVISION_BY_ZERO);
    ASSERT_EQ(cpu.r[FORGE_R0], 10U);
}

/* 5. Test bitwise and shift instructions: AND, OR, XOR, NOT, SHL, SHR */
static void test_bitwise_and_shifts(void) {
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    uint16_t pc = FORGE_DEFAULT_PC;

    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R0, 0, 0xF0F0U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_AND, FORGE_MODE_IMM, FORGE_R0, 0, 0xFF00U); /* 0xF000 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_OR,  FORGE_MODE_IMM, FORGE_R0, 0, 0x00A5U); /* 0xF0A5 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_XOR, FORGE_MODE_IMM, FORGE_R0, 0, 0xFFFFU); /* 0x0F5A */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_NOT, FORGE_MODE_REG, FORGE_R0, 0, 0U);      /* 0xF0A5 */

    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R1, 0, 0x8001U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_SHL, FORGE_MODE_IMM, FORGE_R1, 0, 1U);      /* 0x0002, C=1 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_SHR, FORGE_MODE_IMM, FORGE_R1, 0, 2U);      /* 0x0000, C=1, Z=1 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U);

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R0], 0xF0A5U);
    ASSERT_EQ(cpu.r[FORGE_R1], 0x0000U);
    ASSERT_TRUE(forge_cpu_get_flag(&cpu, FORGE_FLAG_Z));
    ASSERT_TRUE(forge_cpu_get_flag(&cpu, FORGE_FLAG_C));
}

/* 6. Test comparisons and conditional/unconditional branches */
static void test_comparisons_and_branches(void) {
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    uint16_t pc = FORGE_DEFAULT_PC;

    /* Sum integers from 1 to 10 using a loop:
     * R0 = sum (0), R1 = counter (1)
     * loop (0x0408):
     *   ADD R0, R1
     *   ADD R1, 1
     *   CMP R1, 11
     *   JNZ loop
     *   HALT
     */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R0, 0, 0U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R1, 0, 1U);
    uint16_t loop_addr = pc;
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_ADD, FORGE_MODE_REG, FORGE_R0, FORGE_R1, 0U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_ADD, FORGE_MODE_IMM, FORGE_R1, 0, 1U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_CMP, FORGE_MODE_IMM, FORGE_R1, 0, 11U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_JNZ, FORGE_MODE_IMM, 0, 0, loop_addr);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U);

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R0], 55U);

    /* Test signed comparisons JL, JG, JLE, JGE with negative two's complement numbers */
    forge_cpu_init(&cpu);
    pc = FORGE_DEFAULT_PC;
    /* R0 = -5 (0xFFFB), R1 = 3 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R0, 0, (uint16_t)(-5));
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R1, 0, 3U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_CMP, FORGE_MODE_REG, FORGE_R0, FORGE_R1, 0U);
    /* -5 < 3 signed, so JL should jump over the next instruction (pc + 8) */
    uint16_t target_ok = (uint16_t)(pc + 8U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_JL,  FORGE_MODE_IMM, 0, 0, target_ok);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R2, 0, 0xDEADU);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R2, 0, 0xBEEFU);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U);

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R2], 0xBEEFU);
}

/* 7. Test stack operations and function CALL/RET (including overflow & underflow) */
static void test_stack_and_subroutines(void) {
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    uint16_t pc = FORGE_DEFAULT_PC;

    /* Main at 0x0400:
     *   PUSH 0x1111
     *   MOV R0, 7
     *   CALL double_r0 (at 0x0418)
     *   POP R2
     *   HALT
     * double_r0 at 0x0418:
     *   ADD R0, R0
     *   RET
     */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_PUSH, FORGE_MODE_IMM, 0, 0, 0x1111U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV,  FORGE_MODE_IMM, FORGE_R0, 0, 7U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_CALL, FORGE_MODE_IMM, 0, 0, 0x0414U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_POP,  FORGE_MODE_REG, FORGE_R2, 0, 0U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U);
    /* Subroutine at 0x0414 */
    ASSERT_EQ(pc, 0x0414U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_ADD,  FORGE_MODE_REG, FORGE_R0, FORGE_R0, 0U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_RET,  FORGE_MODE_REG, 0, 0, 0U);

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R0], 14U);
    ASSERT_EQ(cpu.r[FORGE_R2], 0x1111U);
    ASSERT_EQ(cpu.sp, FORGE_STACK_TOP);

    /* Test Stack Underflow */
    forge_cpu_init(&cpu);
    pc = FORGE_DEFAULT_PC;
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_POP, FORGE_MODE_REG, FORGE_R0, 0, 0U);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_ERR_STACK_UNDERFLOW);

    /* Test Stack Overflow */
    forge_cpu_init(&cpu);
    cpu.sp = FORGE_STACK_BOTTOM;
    pc = FORGE_DEFAULT_PC;
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_PUSH, FORGE_MODE_IMM, 0, 0, 0x1234U);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_ERR_STACK_OVERFLOW);
}

/* 8. Test memory bounds checking and I/O devices */
static void test_memory_bounds_and_io(void) {
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);

    /* 16-bit word access at 0xFFFF crosses 64 KB boundary -> FORGE_ERR_MEMORY_OUT_OF_BOUNDS */
    uint16_t dummy = 0U;
    ASSERT_EQ(forge_mem_read16(&cpu.mem, 0xFFFFU, &dummy), FORGE_ERR_MEMORY_OUT_OF_BOUNDS);
    ASSERT_EQ(forge_mem_write16(&cpu.mem, 0xFFFFU, 0x1234U), FORGE_ERR_MEMORY_OUT_OF_BOUNDS);

    /* System region write protection */
    cpu.mem.write_protect_system = true;
    ASSERT_EQ(forge_mem_write16(&cpu.mem, 0x0010U, 0x1234U), FORGE_ERR_MEMORY_OUT_OF_BOUNDS);
    cpu.mem.write_protect_system = false;

    /* I/O IN and OUT instructions */
    forge_cpu_init(&cpu);
    ASSERT_TRUE(forge_io_push_input(&cpu.io, 10U));
    ASSERT_TRUE(forge_io_push_input(&cpu.io, 5U));

    uint16_t pc = FORGE_DEFAULT_PC;
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_IN,   FORGE_MODE_IMM, FORGE_R0, 0, FORGE_PORT_CONSOLE_INT);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_IN,   FORGE_MODE_IMM, FORGE_R1, 0, FORGE_PORT_CONSOLE_INT);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_ADD,  FORGE_MODE_REG, FORGE_R0, FORGE_R1, 0U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_OUT,  FORGE_MODE_IMM, FORGE_R0, 0, FORGE_PORT_CONSOLE_INT);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U);

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.io.int_output_count, 1U);
    ASSERT_EQ(cpu.io.int_output_log[0], 15U);
    ASSERT_TRUE(strcmp(cpu.io.console_buf, "15\n") == 0);

    /* Invalid I/O port (> 255) faults with FORGE_ERR_INVALID_IO_PORT */
    forge_cpu_init(&cpu);
    pc = FORGE_DEFAULT_PC;
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_OUT, FORGE_MODE_IMM, FORGE_R0, 0, 300U);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_ERR_INVALID_IO_PORT);
}

/* 9. Test execution limits, breakpoints, tracing, and .fbin loading */
static void test_execution_limit_trace_and_loader(void) {
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);
    cpu.max_instructions = 25U;
    cpu.trace_enabled = true;

    /* Infinite loop: JMP 0x0400 */
    uint16_t pc = FORGE_DEFAULT_PC;
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_JMP, FORGE_MODE_IMM, 0, 0, FORGE_DEFAULT_PC);

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_ERR_EXECUTION_LIMIT);
    ASSERT_EQ(cpu.instructions_executed, 25ULL);
    ASSERT_EQ(cpu.trace_count, 25U);

    /* Test breakpoint stopping */
    forge_cpu_init(&cpu);
    pc = FORGE_DEFAULT_PC;
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV,  FORGE_MODE_IMM, FORGE_R0, 0, 1U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV,  FORGE_MODE_IMM, FORGE_R0, 0, 2U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U);

    ASSERT_TRUE(forge_cpu_add_breakpoint(&cpu, (uint16_t)(FORGE_DEFAULT_PC + 4U)));
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_ERR_BREAKPOINT);
    ASSERT_EQ(cpu.pc, (uint16_t)(FORGE_DEFAULT_PC + 4U));
    ASSERT_EQ(cpu.r[FORGE_R0], 1U);
    /* Resume from breakpoint */
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R0], 2U);
    ASSERT_TRUE(cpu.halted);

    /* Test .fbin header binary loader */
    forge_cpu_init(&cpu);
    uint8_t fbin[16 + 8];
    memset(fbin, 0, sizeof(fbin));
    fbin[0] = 'F'; fbin[1] = 'R'; fbin[2] = 'G'; fbin[3] = '1';
    fbin[4] = 1U;  fbin[5] = 0U; /* version = 1 */
    fbin[6] = 0x00U; fbin[7] = 0x08U; /* load_addr = 0x0800 */
    fbin[8] = 0x00U; fbin[9] = 0x08U; /* entry_pc  = 0x0800 */
    fbin[10] = 8U; fbin[11] = 0U; /* code_size = 8 bytes (2 instructions) */

    uint16_t w0 = 0U, w1 = 0U;
    forge_encode_instruction(FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R5, 0, 0x4242U, &w0, &w1);
    fbin[16] = (uint8_t)(w0 & 0xFFU); fbin[17] = (uint8_t)(w0 >> 8);
    fbin[18] = (uint8_t)(w1 & 0xFFU); fbin[19] = (uint8_t)(w1 >> 8);
    forge_encode_instruction(FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U, &w0, &w1);
    fbin[20] = (uint8_t)(w0 & 0xFFU); fbin[21] = (uint8_t)(w0 >> 8);
    fbin[22] = (uint8_t)(w1 & 0xFFU); fbin[23] = (uint8_t)(w1 >> 8);

    ASSERT_EQ(forge_cpu_load_binary(&cpu, fbin, sizeof(fbin), FORGE_DEFAULT_PC), FORGE_OK);
    ASSERT_EQ(cpu.pc, 0x0800U);
    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_EQ(cpu.r[FORGE_R5], 0x4242U);
}

/* 10. Test ForgeOS syscalls, dynamic heap allocator, MMIO, framebuffer, and multi-task batch execution */
static void test_forge_os_syscalls_framebuffer_and_multitask(void) {
    forge_cpu_t cpu;
    forge_cpu_init(&cpu);

    uint16_t pc = FORGE_DEFAULT_PC;
    /* Allocate 10 bytes on dynamic heap (rounds up to 10 bytes at 0x9000) */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R1, 0, 10U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R0, 0, FORGE_SYS_ALLOC);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_OUT, FORGE_MODE_IMM, FORGE_R0, 0, FORGE_PORT_SYSCALL);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_REG, FORGE_R4, FORGE_R0, 0U);

    /* Plot pixel at (x=12, y=7, color=13) via SYS_DRAW_PIXEL */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R1, 0, 12U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R2, 0, 7U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R3, 0, 13U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R0, 0, FORGE_SYS_DRAW_PIXEL);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_OUT, FORGE_MODE_IMM, FORGE_R0, 0, FORGE_PORT_SYSCALL);

    /* Read MMIO heap break register (0xFF06) into R5 */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_LOAD, FORGE_MODE_MEM_DIR, FORGE_R5, 0, FORGE_MMIO_HEAP_BRK);

    /* Exit with status code 77 via SYS_EXIT */
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R1, 0, 77U);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R0, 0, FORGE_SYS_EXIT);
    forge_cpu_emit_instr(&cpu, &pc, FORGE_OP_OUT, FORGE_MODE_IMM, FORGE_R0, 0, FORGE_PORT_SYSCALL);

    ASSERT_EQ(forge_cpu_run(&cpu), FORGE_OK);
    ASSERT_TRUE(cpu.halted);
    ASSERT_EQ(cpu.os.exit_code, 77U);
    ASSERT_EQ(cpu.r[FORGE_R4], FORGE_DYNAMIC_HEAP_START);
    ASSERT_EQ(cpu.r[FORGE_R5], (uint16_t)(FORGE_DYNAMIC_HEAP_START + 10U));
    ASSERT_EQ(forge_os_get_pixel(&cpu, 12U, 7U), 13U);

    /* Host-managed multi-program execution test */
    uint8_t prog_a[8], prog_b[8];
    uint16_t w0 = 0U, w1 = 0U;
    forge_encode_instruction(FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R1, 0, 100U, &w0, &w1);
    prog_a[0] = (uint8_t)(w0 & 0xFFU); prog_a[1] = (uint8_t)(w0 >> 8);
    prog_a[2] = (uint8_t)(w1 & 0xFFU); prog_a[3] = (uint8_t)(w1 >> 8);
    forge_encode_instruction(FORGE_OP_OUT, FORGE_MODE_IMM, FORGE_R1, 0, FORGE_PORT_CONSOLE_INT, &w0, &w1);
    prog_a[4] = (uint8_t)(w0 & 0xFFU); prog_a[5] = (uint8_t)(w0 >> 8);
    prog_a[6] = (uint8_t)(w1 & 0xFFU); prog_a[7] = (uint8_t)(w1 >> 8);
    /* Wait: append HALT to both programs */
    uint8_t task1_bin[12], task2_bin[12];
    memcpy(task1_bin, prog_a, 8U);
    forge_encode_instruction(FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U, &w0, &w1);
    task1_bin[8]  = (uint8_t)(w0 & 0xFFU); task1_bin[9]  = (uint8_t)(w0 >> 8);
    task1_bin[10] = (uint8_t)(w1 & 0xFFU); task1_bin[11] = (uint8_t)(w1 >> 8);

    forge_encode_instruction(FORGE_OP_MOV, FORGE_MODE_IMM, FORGE_R1, 0, 200U, &w0, &w1);
    prog_b[0] = (uint8_t)(w0 & 0xFFU); prog_b[1] = (uint8_t)(w0 >> 8);
    prog_b[2] = (uint8_t)(w1 & 0xFFU); prog_b[3] = (uint8_t)(w1 >> 8);
    forge_encode_instruction(FORGE_OP_OUT, FORGE_MODE_IMM, FORGE_R1, 0, FORGE_PORT_CONSOLE_INT, &w0, &w1);
    prog_b[4] = (uint8_t)(w0 & 0xFFU); prog_b[5] = (uint8_t)(w0 >> 8);
    prog_b[6] = (uint8_t)(w1 & 0xFFU); prog_b[7] = (uint8_t)(w1 >> 8);
    memcpy(task2_bin, prog_b, 8U);
    memcpy(task2_bin + 8U, task1_bin + 8U, 4U);

    forge_task_desc_t tasks[2] = {
        { "task1", task1_bin, sizeof(task1_bin), NULL },
        { "task2", task2_bin, sizeof(task2_bin), NULL }
    };
    forge_task_result_t results[2];
    ASSERT_EQ(forge_os_run_tasks(&cpu, tasks, results, 2U), FORGE_OK);
    ASSERT_TRUE(results[0].halted);
    ASSERT_TRUE(strcmp(results[0].console_snapshot, "100\n") == 0);
    ASSERT_TRUE(results[1].halted);
    ASSERT_TRUE(strcmp(results[1].console_snapshot, "200\n") == 0);
}

int main(void) {
    printf("Running Forge-16 CPU Emulator Test Suite...\n");

    RUN_TEST(test_cpu_init_and_reset);
    RUN_TEST(test_decoder_and_invalid_instructions);
    RUN_TEST(test_data_movement_instructions);
    RUN_TEST(test_arithmetic_and_flags);
    RUN_TEST(test_bitwise_and_shifts);
    RUN_TEST(test_comparisons_and_branches);
    RUN_TEST(test_stack_and_subroutines);
    RUN_TEST(test_memory_bounds_and_io);
    RUN_TEST(test_execution_limit_trace_and_loader);
    RUN_TEST(test_forge_os_syscalls_framebuffer_and_multitask);

    printf(
        "\nTest Summary: %d/%d passed (%d failed)\n",
        g_tests_run - g_tests_failed,
        g_tests_run,
        g_tests_failed
    );
    return (g_tests_failed == 0) ? 0 : 1;
}

