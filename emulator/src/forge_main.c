#include "forge_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("Forge-16 CPU Emulator CLI\n");
    printf("Usage: %s [options] [binary_file]\n\n", prog);
    printf("Options:\n");
    printf("  --demo                   Load and execute built-in demonstration program\n");
    printf("  --trace                  Enable and print instruction execution trace\n");
    printf("  --dump-regs              Print CPU register and flag state after execution\n");
    printf("  --dump-mem <addr> <len>  Print hex dump of memory region after execution\n");
    printf("  --max-steps <N>          Set maximum instruction execution limit (default: 100000)\n");
    printf("  --help                   Show this help message\n");
}

static void dump_memory_hex(const forge_cpu_t *cpu, uint16_t start_addr, uint16_t length) {
    printf("\nMemory Dump [0x%04X .. 0x%04X]:\n", start_addr, (uint16_t)(start_addr + length - 1U));
    for (uint32_t offset = 0U; offset < length; offset += 16U) {
        uint32_t row_addr = (uint32_t)start_addr + offset;
        if (row_addr >= FORGE_MEM_SIZE) {
            break;
        }
        printf("  0x%04X: ", (uint16_t)row_addr);
        for (uint32_t i = 0U; i < 16U; i++) {
            if (offset + i < length && (row_addr + i) < FORGE_MEM_SIZE) {
                printf("%02X ", cpu->mem.data[row_addr + i]);
            } else {
                printf("   ");
            }
        }
        printf(" |");
        for (uint32_t i = 0U; i < 16U; i++) {
            if (offset + i < length && (row_addr + i) < FORGE_MEM_SIZE) {
                uint8_t b = cpu->mem.data[row_addr + i];
                printf("%c", (b >= 32U && b <= 126U) ? (char)b : '.');
            }
        }
        printf("|\n");
    }
}

static void load_demo_program(forge_cpu_t *cpu) {
    uint16_t pc = FORGE_DEFAULT_PC;
    forge_cpu_emit_instr(cpu, &pc, FORGE_OP_MOV,  FORGE_MODE_IMM, FORGE_R0, 0, 10U);
    forge_cpu_emit_instr(cpu, &pc, FORGE_OP_MOV,  FORGE_MODE_IMM, FORGE_R1, 0, 5U);
    forge_cpu_emit_instr(cpu, &pc, FORGE_OP_ADD,  FORGE_MODE_REG, FORGE_R0, FORGE_R1, 0U);
    forge_cpu_emit_instr(cpu, &pc, FORGE_OP_OUT,  FORGE_MODE_IMM, FORGE_R0, 0, FORGE_PORT_CONSOLE_INT);
    forge_cpu_emit_instr(cpu, &pc, FORGE_OP_HALT, FORGE_MODE_REG, 0, 0, 0U);
    cpu->pc = FORGE_DEFAULT_PC;
}

int main(int argc, char **argv) {
    bool use_demo = false;
    bool enable_trace = false;
    bool dump_regs = false;
    bool dump_mem = false;
    uint16_t dump_addr = FORGE_DEFAULT_PC;
    uint16_t dump_len = 64U;
    uint64_t max_steps = FORGE_DEFAULT_MAX_STEPS;
    const char *filepath = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--demo") == 0) {
            use_demo = true;
        } else if (strcmp(argv[i], "--trace") == 0) {
            enable_trace = true;
        } else if (strcmp(argv[i], "--dump-regs") == 0) {
            dump_regs = true;
        } else if (strcmp(argv[i], "--dump-mem") == 0 && i + 2 < argc) {
            dump_mem = true;
            dump_addr = (uint16_t)strtoul(argv[++i], NULL, 0);
            dump_len  = (uint16_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--max-steps") == 0 && i + 1 < argc) {
            max_steps = (uint64_t)strtoull(argv[++i], NULL, 0);
        } else if (argv[i][0] != '-') {
            filepath = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!use_demo && filepath == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    forge_cpu_t *cpu = (forge_cpu_t *)malloc(sizeof(forge_cpu_t));
    if (cpu == NULL) {
        fprintf(stderr, "Failed to allocate CPU state\n");
        return 1;
    }

    forge_cpu_init(cpu);
    cpu->trace_enabled = enable_trace;
    cpu->max_instructions = max_steps;

    if (use_demo) {
        load_demo_program(cpu);
    } else {
        forge_error_t load_err = forge_cpu_load_file(cpu, filepath, FORGE_DEFAULT_PC);
        if (load_err != FORGE_OK) {
            fprintf(stderr, "Error loading '%s': %s\n", filepath, forge_error_str(load_err));
            free(cpu);
            return 1;
        }
    }

    forge_error_t exec_err = forge_cpu_run(cpu);

    if (cpu->io.console_len > 0U) {
        printf("%s", cpu->io.console_buf);
    }

    if (enable_trace) {
        printf("\nExecution Trace (%zu entries):\n", cpu->trace_count);
        for (size_t i = 0U; i < cpu->trace_count; i++) {
            const forge_trace_entry_t *entry = forge_cpu_get_trace(cpu, i);
            char line[256];
            forge_cpu_format_trace_entry(entry, line, sizeof(line));
            printf("  %s\n", line);
        }
    }

    if (dump_regs) {
        char state_buf[512];
        forge_cpu_format_state(cpu, state_buf, sizeof(state_buf));
        printf("\nCPU State:\n%s\n", state_buf);
    }

    if (dump_mem) {
        dump_memory_hex(cpu, dump_addr, dump_len);
    }

    if (exec_err != FORGE_OK && exec_err != FORGE_ERR_BREAKPOINT) {
        fprintf(
            stderr,
            "Runtime fault at PC=0x%04X: %s\n",
            cpu->fault_pc,
            forge_error_str(exec_err)
        );
        free(cpu);
        return 1;
    }

    free(cpu);
    return 0;
}

