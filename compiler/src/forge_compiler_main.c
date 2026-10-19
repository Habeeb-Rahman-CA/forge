#include "forge_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("ForgeLang High-Level Compiler CLI\n");
    printf("Usage: %s <input.fl> [options]\n\n", prog);
    printf("Options:\n");
    printf("  -o <file.fbin>        Write compiled .fbin executable binary to <file.fbin>\n");
    printf("  --emit-asm <file.asm> Write generated Forge-16 assembly to <file.asm>\n");
    printf("  --print-ast           Print Abstract Syntax Tree (AST)\n");
    printf("  --print-asm           Print generated Forge-16 assembly\n");
    printf("  --sourcemap           Print high-level source line -> assembly -> address map\n");
    printf("  --run                 Execute the compiled program in the Forge-16 CPU emulator\n");
    printf("  --trace               Enable CPU instruction trace when running with --run\n");
    printf("  --help                Display this help message\n");
}

int main(int argc, char **argv) {
    const char *input_path = NULL;
    const char *output_fbin = NULL;
    const char *output_asm = NULL;
    bool print_ast = false;
    bool print_asm = false;
    bool print_smap = false;
    bool run_prog = false;
    bool enable_trace = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_fbin = argv[++i];
        } else if (strcmp(argv[i], "--emit-asm") == 0 && i + 1 < argc) {
            output_asm = argv[++i];
        } else if (strcmp(argv[i], "--print-ast") == 0) {
            print_ast = true;
        } else if (strcmp(argv[i], "--print-asm") == 0) {
            print_asm = true;
        } else if (strcmp(argv[i], "--sourcemap") == 0) {
            print_smap = true;
        } else if (strcmp(argv[i], "--run") == 0) {
            run_prog = true;
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

    if (input_path == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    forge_compile_result_t *res = (forge_compile_result_t *)malloc(sizeof(forge_compile_result_t));
    if (res == NULL) {
        fprintf(stderr, "Failed to allocate compiler result structure\n");
        return 1;
    }

    if (!forge_compile_file(input_path, res)) {
        fprintf(stderr, "Compilation failed for '%s' (%zu error(s)):\n", input_path, res->error_count);
        for (size_t i = 0U; i < res->error_count; i++) {
            fprintf(
                stderr,
                "  %s:%u:%u: [%s error] %s\n",
                input_path,
                res->errors[i].line,
                res->errors[i].column,
                res->errors[i].stage,
                res->errors[i].message
            );
        }
        free(res);
        return 1;
    }

    if (print_ast) {
        char *ast_buf = (char *)malloc(32768U);
        if (ast_buf != NULL) {
            forge_format_ast(res->ast.root, 0, ast_buf, 32768U);
            printf("=== Abstract Syntax Tree ===\n%s\n", ast_buf);
            free(ast_buf);
        }
    }

    if (print_asm) {
        printf("=== Generated Forge-16 Assembly ===\n%s\n", res->generated_asm);
    }

    if (print_smap) {
        printf("=== Source Map (%zu entries) ===\n", res->source_map_count);
        for (size_t i = 0U; i < res->source_map_count; i++) {
            printf(
                "  Source Line %3u  ->  ASM Line %3u  ->  Address 0x%04X\n",
                res->source_map[i].source_line,
                res->source_map[i].asm_line,
                res->source_map[i].address
            );
        }
    }

    if (output_asm != NULL) {
        FILE *fp = fopen(output_asm, "wb");
        if (fp == NULL) {
            fprintf(stderr, "Error writing assembly to '%s'\n", output_asm);
            free(res);
            return 1;
        }
        fwrite(res->generated_asm, 1U, res->generated_asm_len, fp);
        fclose(fp);
    }

    if (output_fbin != NULL) {
        if (!forge_asm_write_fbin(&res->asm_result, output_fbin)) {
            fprintf(stderr, "Error writing .fbin to '%s'\n", output_fbin);
            free(res);
            return 1;
        }
    }

    if (run_prog) {
        forge_cpu_t *cpu = (forge_cpu_t *)malloc(sizeof(forge_cpu_t));
        if (cpu == NULL) {
            free(res);
            return 1;
        }
        forge_cpu_init(cpu);
        cpu->trace_enabled = enable_trace;

        forge_error_t err = forge_cpu_load_fbin(cpu, res->asm_result.fbin, res->asm_result.fbin_size);
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

