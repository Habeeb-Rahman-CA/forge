#include "forge_asm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("Forge-16 Assembler & Disassembler CLI\n");
    printf("Usage:\n");
    printf("  %s <input.asm> [-o <output.fbin>] [options]\n", prog);
    printf("  %s --disasm-file <binary.fbin>\n\n", prog);
    printf("Options:\n");
    printf("  -o <file>             Write output binary to <file>\n");
    printf("  --raw                 Emit raw machine code without 16-byte .fbin header\n");
    printf("  --symbols             Print symbol table after assembly\n");
    printf("  --disasm              Print disassembly of assembled binary\n");
    printf("  --disasm-file <file>  Disassemble an existing .fbin or raw binary file\n");
    printf("  --run                 Execute the assembled program immediately in the CPU emulator\n");
    printf("  --trace               Enable instruction trace when running with --run\n");
    printf("  --help                Display this help message\n");
}

static int disassemble_file_cmd(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: Could not open binary file '%s'\n", filepath);
        return 1;
    }
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return 1;
    }
    long fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        return 1;
    }
    rewind(fp);

    uint8_t *buf = (uint8_t *)malloc((size_t)fsize > 0U ? (size_t)fsize : 1U);
    if (buf == NULL) {
        fclose(fp);
        return 1;
    }
    size_t nread = fread(buf, 1U, (size_t)fsize, fp);
    fclose(fp);

    size_t out_cap = 65536U;
    char *out_text = (char *)malloc(out_cap);
    if (out_text == NULL) {
        free(buf);
        return 1;
    }

    forge_error_t err = forge_disassemble_binary(buf, nread, FORGE_DEFAULT_PC, out_text, out_cap);
    free(buf);
    if (err != FORGE_OK) {
        fprintf(stderr, "Disassembly error: %s\n", forge_error_str(err));
        free(out_text);
        return 1;
    }

    printf("%s", out_text);
    free(out_text);
    return 0;
}

int main(int argc, char **argv) {
    const char *input_path = NULL;
    const char *output_path = NULL;
    const char *disasm_file_path = NULL;
    bool emit_raw = false;
    bool show_symbols = false;
    bool show_disasm = false;
    bool run_program = false;
    bool enable_trace = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (strcmp(argv[i], "--raw") == 0) {
            emit_raw = true;
        } else if (strcmp(argv[i], "--symbols") == 0) {
            show_symbols = true;
        } else if (strcmp(argv[i], "--disasm") == 0) {
            show_disasm = true;
        } else if (strcmp(argv[i], "--disasm-file") == 0 && i + 1 < argc) {
            disasm_file_path = argv[++i];
        } else if (strcmp(argv[i], "--run") == 0) {
            run_program = true;
        } else if (strcmp(argv[i], "--trace") == 0) {
            enable_trace = true;
        } else if (argv[i][0] != '-') {
            input_path = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (disasm_file_path != NULL) {
        return disassemble_file_cmd(disasm_file_path);
    }

    if (input_path == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    forge_asm_result_t *res = (forge_asm_result_t *)malloc(sizeof(forge_asm_result_t));
    if (res == NULL) {
        fprintf(stderr, "Failed to allocate assembler result structure\n");
        return 1;
    }

    if (!forge_assemble_file(input_path, res)) {
        fprintf(stderr, "Assembly failed for '%s' (%zu error(s)):\n", input_path, res->error_count);
        for (size_t i = 0U; i < res->error_count; i++) {
            fprintf(
                stderr,
                "  %s:%u:%u: error: %s\n",
                input_path,
                res->errors[i].line,
                res->errors[i].column,
                res->errors[i].message
            );
        }
        free(res);
        return 1;
    }

    if (output_path != NULL) {
        bool wrote = emit_raw
                         ? forge_asm_write_raw(res, output_path)
                         : forge_asm_write_fbin(res, output_path);
        if (!wrote) {
            fprintf(stderr, "Error: Failed to write output binary to '%s'\n", output_path);
            free(res);
            return 1;
        }
    }

    if (show_symbols) {
        printf("Symbol Table (%zu entries):\n", res->symbol_count);
        for (size_t i = 0U; i < res->symbol_count; i++) {
            printf(
                "  %-20s = 0x%04X (%5u) [%s, line %u]\n",
                res->symbols[i].name,
                res->symbols[i].value,
                res->symbols[i].value,
                res->symbols[i].is_constant ? "CONST" : "LABEL",
                res->symbols[i].line
            );
        }
    }

    if (show_disasm) {
        size_t cap = 65536U;
        char *dis_buf = (char *)malloc(cap);
        if (dis_buf != NULL) {
            if (forge_disassemble_binary(res->fbin, res->fbin_size, res->load_addr, dis_buf, cap) == FORGE_OK) {
                printf("%s", dis_buf);
            }
            free(dis_buf);
        }
    }

    if (run_program) {
        forge_cpu_t *cpu = (forge_cpu_t *)malloc(sizeof(forge_cpu_t));
        if (cpu == NULL) {
            free(res);
            return 1;
        }
        forge_cpu_init(cpu);
        cpu->trace_enabled = enable_trace;

        forge_error_t err = forge_cpu_load_fbin(cpu, res->fbin, res->fbin_size);
        if (err == FORGE_OK) {
            err = forge_cpu_run(cpu);
        }
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
        if (err != FORGE_OK) {
            fprintf(stderr, "Runtime fault at PC=0x%04X: %s\n", cpu->fault_pc, forge_error_str(err));
            free(cpu);
            free(res);
            return 1;
        }
        free(cpu);
    }

    free(res);
    return 0;
}

