#include "forge_bridge.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void save_prev_snapshot(forge_bridge_session_t *s) {
    memcpy(s->prev_r, s->cpu.r, sizeof(s->prev_r));
    s->prev_pc    = s->cpu.pc;
    s->prev_sp    = s->cpu.sp;
    s->prev_flags = s->cpu.flags;
}

void forge_bridge_init(forge_bridge_session_t *session) {
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
    forge_cpu_init(&session->cpu);
    session->cpu.trace_enabled = true;
    session->cpu.mem.write_protect_system = true;
    forge_asm_result_init(&session->last_asm);
    session->last_comp = NULL;
    session->is_compiled = false;
    session->has_program = false;
    save_prev_snapshot(session);
}

void forge_bridge_destroy(forge_bridge_session_t *session) {
    if (session == NULL) {
        return;
    }
    if (session->last_comp != NULL) {
        free(session->last_comp);
        session->last_comp = NULL;
    }
}

bool forge_bridge_assemble(forge_bridge_session_t *session, const char *source) {
    if (session == NULL || source == NULL) {
        return false;
    }
    /* Preserve breakpoints across re-assembly */
    uint16_t saved_bps[FORGE_MAX_BREAKPOINTS];
    size_t saved_bp_count = session->cpu.breakpoint_count;
    memcpy(saved_bps, session->cpu.breakpoints, sizeof(saved_bps));

    session->is_compiled = false;
    bool ok = forge_assemble_source(source, &session->last_asm);
    if (!ok) {
        return false;
    }

    forge_cpu_reset(&session->cpu, false);
    session->cpu.trace_enabled = true;
    session->cpu.mem.write_protect_system = false;
    forge_error_t load_err = forge_cpu_load_fbin(
        &session->cpu,
        session->last_asm.fbin,
        session->last_asm.fbin_size
    );
    session->cpu.mem.write_protect_system = true;

    session->cpu.breakpoint_count = saved_bp_count;
    memcpy(session->cpu.breakpoints, saved_bps, sizeof(saved_bps));

    save_prev_snapshot(session);
    session->has_program = (load_err == FORGE_OK);
    return session->has_program;
}

bool forge_bridge_compile(forge_bridge_session_t *session, const char *source) {
    if (session == NULL || source == NULL) {
        return false;
    }
    if (session->last_comp == NULL) {
        session->last_comp = (forge_compile_result_t *)calloc(1U, sizeof(forge_compile_result_t));
        if (session->last_comp == NULL) {
            return false;
        }
    }

    uint16_t saved_bps[FORGE_MAX_BREAKPOINTS];
    size_t saved_bp_count = session->cpu.breakpoint_count;
    memcpy(saved_bps, session->cpu.breakpoints, sizeof(saved_bps));

    session->is_compiled = true;
    bool ok = forge_compile_source(source, session->last_comp);
    if (!ok) {
        return false;
    }

    session->last_asm = session->last_comp->asm_result;

    forge_cpu_reset(&session->cpu, false);
    session->cpu.trace_enabled = true;
    session->cpu.mem.write_protect_system = false;
    forge_error_t load_err = forge_cpu_load_fbin(
        &session->cpu,
        session->last_asm.fbin,
        session->last_asm.fbin_size
    );
    session->cpu.mem.write_protect_system = true;

    session->cpu.breakpoint_count = saved_bp_count;
    memcpy(session->cpu.breakpoints, saved_bps, sizeof(saved_bps));

    save_prev_snapshot(session);
    session->has_program = (load_err == FORGE_OK);
    return session->has_program;
}

uint32_t forge_bridge_source_line_for_pc(const forge_bridge_session_t *s, uint16_t pc) {
    if (s == NULL) {
        return 0U;
    }
    if (s->is_compiled && s->last_comp != NULL) {
        for (size_t i = 0U; i < s->last_comp->source_map_count; i++) {
            if (s->last_comp->source_map[i].address == pc) {
                return s->last_comp->source_map[i].source_line;
            }
        }
        return 0U;
    }
    for (size_t i = 0U; i < s->last_asm.line_map_count; i++) {
        if (s->last_asm.line_map[i].address == pc) {
            return s->last_asm.line_map[i].line;
        }
    }
    return 0U;
}

uint32_t forge_bridge_asm_line_for_pc(const forge_bridge_session_t *s, uint16_t pc) {
    if (s == NULL) {
        return 0U;
    }
    for (size_t i = 0U; i < s->last_asm.line_map_count; i++) {
        if (s->last_asm.line_map[i].address == pc) {
            return s->last_asm.line_map[i].line;
        }
    }
    return 0U;
}

forge_error_t forge_bridge_step(forge_bridge_session_t *session, uint32_t count) {
    if (session == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (count == 0U) {
        count = 1U;
    }
    save_prev_snapshot(session);
    forge_error_t err = FORGE_OK;
    for (uint32_t i = 0U; i < count; i++) {
        if (session->cpu.halted || session->cpu.error != FORGE_OK) {
            break;
        }
        err = forge_cpu_step(&session->cpu);
        if (err != FORGE_OK) {
            break;
        }
    }
    return err;
}

forge_error_t forge_bridge_step_line(forge_bridge_session_t *session) {
    if (session == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    save_prev_snapshot(session);
    uint32_t start_line = forge_bridge_source_line_for_pc(session, session->cpu.pc);
    forge_error_t err = FORGE_OK;
    for (uint32_t steps = 0U; steps < 4096U; steps++) {
        if (session->cpu.halted || session->cpu.error != FORGE_OK) {
            break;
        }
        if (steps > 0U && forge_cpu_has_breakpoint(&session->cpu, session->cpu.pc)) {
            return FORGE_ERR_BREAKPOINT;
        }
        err = forge_cpu_step(&session->cpu);
        if (err != FORGE_OK) {
            break;
        }
        uint32_t cur_line = forge_bridge_source_line_for_pc(session, session->cpu.pc);
        if (cur_line != 0U && cur_line != start_line) {
            break;
        }
    }
    return err;
}

forge_error_t forge_bridge_step_over(forge_bridge_session_t *session, bool line_mode) {
    if (session == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    save_prev_snapshot(session);
    size_t start_depth = session->cpu.call_depth;
    uint32_t start_line = forge_bridge_source_line_for_pc(session, session->cpu.pc);
    forge_error_t err = FORGE_OK;

    for (uint32_t steps = 0U; steps < 65536U; steps++) {
        if (session->cpu.halted || session->cpu.error != FORGE_OK) {
            break;
        }
        if (steps > 0U && forge_cpu_has_breakpoint(&session->cpu, session->cpu.pc)) {
            return FORGE_ERR_BREAKPOINT;
        }
        err = forge_cpu_step(&session->cpu);
        if (err != FORGE_OK) {
            break;
        }
        if (session->cpu.call_depth < start_depth) {
            break;
        }
        if (session->cpu.call_depth == start_depth) {
            if (!line_mode) {
                break;
            }
            uint32_t cur_line = forge_bridge_source_line_for_pc(session, session->cpu.pc);
            if (cur_line != 0U && cur_line != start_line) {
                break;
            }
        }
    }
    return err;
}

forge_error_t forge_bridge_step_out(forge_bridge_session_t *session) {
    if (session == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    save_prev_snapshot(session);
    size_t start_depth = session->cpu.call_depth;
    if (start_depth == 0U) {
        return forge_bridge_step_line(session);
    }
    size_t target_depth = start_depth - 1U;
    forge_error_t err = FORGE_OK;
    for (uint32_t steps = 0U; steps < 65536U; steps++) {
        if (session->cpu.halted || session->cpu.error != FORGE_OK) {
            break;
        }
        if (steps > 0U && forge_cpu_has_breakpoint(&session->cpu, session->cpu.pc)) {
            return FORGE_ERR_BREAKPOINT;
        }
        err = forge_cpu_step(&session->cpu);
        if (err != FORGE_OK) {
            break;
        }
        if (session->cpu.call_depth <= target_depth) {
            break;
        }
    }
    return err;
}

forge_error_t forge_bridge_run(forge_bridge_session_t *session, uint32_t max_steps) {
    if (session == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    save_prev_snapshot(session);
    uint64_t prev_limit = session->cpu.max_instructions;
    if (max_steps > 0U) {
        session->cpu.max_instructions = session->cpu.instructions_executed + (uint64_t)max_steps;
    }
    forge_error_t err = forge_cpu_run(&session->cpu);
    session->cpu.max_instructions = prev_limit;
    return err;
}

void forge_bridge_reset(forge_bridge_session_t *session) {
    if (session == NULL) {
        return;
    }
    uint16_t saved_bps[FORGE_MAX_BREAKPOINTS];
    size_t saved_bp_count = session->cpu.breakpoint_count;
    memcpy(saved_bps, session->cpu.breakpoints, sizeof(saved_bps));

    forge_cpu_reset(&session->cpu, false);
    session->cpu.trace_enabled = true;
    if (session->has_program && session->last_asm.success) {
        session->cpu.mem.write_protect_system = false;
        forge_cpu_load_fbin(&session->cpu, session->last_asm.fbin, session->last_asm.fbin_size);
        session->cpu.mem.write_protect_system = true;
    }
    session->cpu.breakpoint_count = saved_bp_count;
    memcpy(session->cpu.breakpoints, saved_bps, sizeof(saved_bps));
    save_prev_snapshot(session);
}

bool forge_bridge_add_watch(forge_bridge_session_t *session, uint16_t addr) {
    if (session == NULL) {
        return false;
    }
    for (size_t i = 0U; i < session->watch_count; i++) {
        if (session->watches[i] == addr) {
            return true;
        }
    }
    if (session->watch_count >= FORGE_MAX_WATCHES) {
        return false;
    }
    session->watches[session->watch_count++] = addr;
    return true;
}

bool forge_bridge_remove_watch(forge_bridge_session_t *session, uint16_t addr) {
    if (session == NULL) {
        return false;
    }
    for (size_t i = 0U; i < session->watch_count; i++) {
        if (session->watches[i] == addr) {
            for (size_t j = i; j + 1U < session->watch_count; j++) {
                session->watches[j] = session->watches[j + 1U];
            }
            session->watch_count--;
            return true;
        }
    }
    return false;
}

/* Safe buffer append helpers */
static void buf_appendf(char *buf, size_t cap, size_t *pos, const char *fmt, ...) {
    if (*pos + 1U >= cap) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *pos, cap - *pos, fmt, args);
    va_end(args);
    if (n > 0) {
        if ((size_t)n < cap - *pos) {
            *pos += (size_t)n;
        } else {
            *pos = cap - 1U;
        }
    }
}

static void buf_append_json_str(char *buf, size_t cap, size_t *pos, const char *s) {
    buf_appendf(buf, cap, pos, "\"");
    if (s != NULL) {
        for (size_t i = 0U; s[i] != '\0'; i++) {
            unsigned char c = (unsigned char)s[i];
            switch (c) {
                case '"':  buf_appendf(buf, cap, pos, "\\\""); break;
                case '\\': buf_appendf(buf, cap, pos, "\\\\"); break;
                case '\n': buf_appendf(buf, cap, pos, "\\n");  break;
                case '\r': buf_appendf(buf, cap, pos, "\\r");  break;
                case '\t': buf_appendf(buf, cap, pos, "\\t");  break;
                default:
                    if (c >= 32U && c <= 126U) {
                        buf_appendf(buf, cap, pos, "%c", (char)c);
                    } else {
                        buf_appendf(buf, cap, pos, "\\u%04X", (unsigned int)c);
                    }
                    break;
            }
        }
    }
    buf_appendf(buf, cap, pos, "\"");
}

static const char *resolve_symbol_for_addr(const forge_bridge_session_t *s, uint16_t addr) {
    for (size_t i = 0U; i < s->last_asm.symbol_count; i++) {
        const forge_asm_symbol_t *sym = &s->last_asm.symbols[i];
        if (!sym->is_constant && sym->value == addr) {
            if (strncmp(sym->name, "fn_", 3U) == 0) {
                return sym->name + 3;
            }
            return sym->name;
        }
    }
    return "subroutine";
}

size_t forge_bridge_state_json(
    const forge_bridge_session_t *s,
    uint16_t mem_start,
    uint16_t mem_len,
    char *out_buf,
    size_t out_buf_size
) {
    if (s == NULL || out_buf == NULL || out_buf_size == 0U) {
        return 0U;
    }
    if (mem_len == 0U || mem_len > 1024U) {
        mem_len = 256U;
    }

    size_t pos = 0U;
    out_buf[0] = '\0';

    const forge_cpu_t *cpu = &s->cpu;
    bool build_ok = s->is_compiled
                        ? (s->last_comp != NULL && s->last_comp->error_count == 0U)
                        : (s->last_asm.error_count == 0U);

    uint32_t active_src_line = forge_bridge_source_line_for_pc(s, cpu->pc);
    uint32_t active_asm_line = forge_bridge_asm_line_for_pc(s, cpu->pc);

    buf_appendf(out_buf, out_buf_size, &pos, "{");
    buf_appendf(out_buf, out_buf_size, &pos, "\"success\":%s,", build_ok ? "true" : "false");
    buf_appendf(out_buf, out_buf_size, &pos, "\"mode\":\"%s\",", s->is_compiled ? "forgelang" : "assembly");
    buf_appendf(out_buf, out_buf_size, &pos, "\"hasProgram\":%s,", s->has_program ? "true" : "false");
    buf_appendf(out_buf, out_buf_size, &pos, "\"activeSourceLine\":%u,", active_src_line);
    buf_appendf(out_buf, out_buf_size, &pos, "\"activeAsmLine\":%u,", active_asm_line);
    buf_appendf(out_buf, out_buf_size, &pos, "\"pc\":%u,", cpu->pc);
    buf_appendf(out_buf, out_buf_size, &pos, "\"prevPc\":%u,", s->prev_pc);
    buf_appendf(out_buf, out_buf_size, &pos, "\"sp\":%u,", cpu->sp);
    buf_appendf(out_buf, out_buf_size, &pos, "\"prevSp\":%u,", s->prev_sp);
    buf_appendf(out_buf, out_buf_size, &pos, "\"flags\":%u,", cpu->flags);
    buf_appendf(out_buf, out_buf_size, &pos, "\"prevFlags\":%u,", s->prev_flags);
    buf_appendf(out_buf, out_buf_size, &pos, "\"flagZ\":%s,", (cpu->flags & FORGE_FLAG_Z) ? "true" : "false");
    buf_appendf(out_buf, out_buf_size, &pos, "\"flagN\":%s,", (cpu->flags & FORGE_FLAG_N) ? "true" : "false");
    buf_appendf(out_buf, out_buf_size, &pos, "\"flagC\":%s,", (cpu->flags & FORGE_FLAG_C) ? "true" : "false");
    buf_appendf(out_buf, out_buf_size, &pos, "\"flagV\":%s,", (cpu->flags & FORGE_FLAG_V) ? "true" : "false");
    buf_appendf(out_buf, out_buf_size, &pos, "\"halted\":%s,", cpu->halted ? "true" : "false");
    buf_appendf(out_buf, out_buf_size, &pos, "\"errorCode\":%d,", (int)cpu->error);
    buf_appendf(out_buf, out_buf_size, &pos, "\"errorMessage\":");
    buf_append_json_str(out_buf, out_buf_size, &pos, forge_error_str(cpu->error));
    buf_appendf(out_buf, out_buf_size, &pos, ",\"faultPc\":%u,", cpu->fault_pc);
    buf_appendf(out_buf, out_buf_size, &pos, "\"cycles\":%" PRIu64 ",", cpu->cycles);
    buf_appendf(out_buf, out_buf_size, &pos, "\"instructionsExecuted\":%" PRIu64 ",", cpu->instructions_executed);

    /* Registers & Previous Registers */
    buf_appendf(out_buf, out_buf_size, &pos, "\"registers\":[");
    for (uint8_t i = 0U; i < FORGE_NUM_REGS; i++) {
        buf_appendf(out_buf, out_buf_size, &pos, "%s%u", (i > 0U ? "," : ""), cpu->r[i]);
    }
    buf_appendf(out_buf, out_buf_size, &pos, "],\"prevRegisters\":[");
    for (uint8_t i = 0U; i < FORGE_NUM_REGS; i++) {
        buf_appendf(out_buf, out_buf_size, &pos, "%s%u", (i > 0U ? "," : ""), s->prev_r[i]);
    }

    /* Breakpoints */
    buf_appendf(out_buf, out_buf_size, &pos, "],\"breakpoints\":[");
    for (size_t i = 0U; i < cpu->breakpoint_count; i++) {
        buf_appendf(out_buf, out_buf_size, &pos, "%s%u", (i > 0U ? "," : ""), cpu->breakpoints[i]);
    }

    /* Disassembly / Line Map (enriched with sourceLine) */
    buf_appendf(out_buf, out_buf_size, &pos, "],\"disassembly\":[");
    for (size_t i = 0U; i < s->last_asm.line_map_count; i++) {
        const forge_asm_line_map_t *lm = &s->last_asm.line_map[i];
        forge_instruction_t decoded;
        char mnem[64];
        if (forge_decode_instruction(lm->word0, lm->word1, &decoded) == FORGE_OK) {
            forge_disassemble_instruction(&decoded, mnem, sizeof(mnem));
        } else {
            snprintf(mnem, sizeof(mnem), "DW 0x%04X, 0x%04X", lm->word0, lm->word1);
        }
        uint32_t src_line = forge_bridge_source_line_for_pc(s, lm->address);
        buf_appendf(
            out_buf, out_buf_size, &pos,
            "%s{\"address\":%u,\"line\":%u,\"sourceLine\":%u,\"word0\":%u,\"word1\":%u,\"text\":",
            (i > 0U ? "," : ""),
            lm->address,
            lm->line,
            src_line,
            lm->word0,
            lm->word1
        );
        buf_append_json_str(out_buf, out_buf_size, &pos, mnem);
        buf_appendf(out_buf, out_buf_size, &pos, "}");
    }

    /* 3-Way Source Map, Generated ASM & AST */
    buf_appendf(out_buf, out_buf_size, &pos, "],\"sourceMap\":[");
    if (s->is_compiled && s->last_comp != NULL) {
        for (size_t i = 0U; i < s->last_comp->source_map_count; i++) {
            const forge_source_map_entry_t *sm = &s->last_comp->source_map[i];
            buf_appendf(
                out_buf, out_buf_size, &pos,
                "%s{\"sourceLine\":%u,\"asmLine\":%u,\"address\":%u}",
                (i > 0U ? "," : ""),
                sm->source_line,
                sm->asm_line,
                sm->address
            );
        }
    }
    buf_appendf(out_buf, out_buf_size, &pos, "],\"generatedAsm\":");
    if (s->is_compiled && s->last_comp != NULL) {
        buf_append_json_str(out_buf, out_buf_size, &pos, s->last_comp->generated_asm);
    } else {
        buf_append_json_str(out_buf, out_buf_size, &pos, "");
    }
    buf_appendf(out_buf, out_buf_size, &pos, ",\"astText\":");
    if (s->is_compiled && s->last_comp != NULL && s->last_comp->ast.root != NULL) {
        char ast_buf[8192];
        forge_format_ast(s->last_comp->ast.root, 0, ast_buf, sizeof(ast_buf));
        buf_append_json_str(out_buf, out_buf_size, &pos, ast_buf);
    } else {
        buf_append_json_str(out_buf, out_buf_size, &pos, "");
    }

    /* High-Level Global Variables (live values read from RAM) */
    buf_appendf(out_buf, out_buf_size, &pos, ",\"globals\":[");
    if (s->is_compiled && s->last_comp != NULL) {
        size_t g_idx = 0U;
        for (size_t i = 0U; i < s->last_comp->symbol_count; i++) {
            const forge_comp_symbol_t *sym = &s->last_comp->symbols[i];
            if (sym->kind == FORGE_SYM_GLOBAL_VAR) {
                uint16_t val16 = 0U;
                forge_mem_peek16(&cpu->mem, sym->global_addr, &val16);
                int16_t sval16 = (int16_t)val16;
                buf_appendf(out_buf, out_buf_size, &pos, "%s{\"name\":", (g_idx > 0U ? "," : ""));
                buf_append_json_str(out_buf, out_buf_size, &pos, sym->name);
                buf_appendf(
                    out_buf, out_buf_size, &pos,
                    ",\"address\":%u,\"value\":%u,\"signedValue\":%d,\"declLine\":%u}",
                    sym->global_addr,
                    val16,
                    (int)sval16,
                    sym->decl_line
                );
                g_idx++;
            }
        }
    }

    /* Call Stack & Activation Frames (with live parameters and local variables) */
    buf_appendf(out_buf, out_buf_size, &pos, "],\"callStack\":[");
    size_t max_frames = (cpu->call_depth < FORGE_MAX_CALL_DEPTH) ? cpu->call_depth : FORGE_MAX_CALL_DEPTH;
    for (size_t f = 0U; f < max_frames; f++) {
        const forge_call_frame_t *cf = &cpu->call_stack[f];
        const char *fn_name = resolve_symbol_for_addr(s, cf->callee_addr);
        uint32_t caller_line = forge_bridge_source_line_for_pc(s, cf->caller_pc);
        buf_appendf(out_buf, out_buf_size, &pos, "%s{\"depth\":%zu,\"function\":", (f > 0U ? "," : ""), f);
        buf_append_json_str(out_buf, out_buf_size, &pos, fn_name);
        buf_appendf(
            out_buf, out_buf_size, &pos,
            ",\"calleeAddr\":%u,\"callerPc\":%u,\"callerSourceLine\":%u,\"returnAddr\":%u,\"frameFp\":%u,\"frameSp\":%u,\"locals\":[",
            cf->callee_addr,
            cf->caller_pc,
            caller_line,
            cf->return_addr,
            cf->frame_fp,
            cf->frame_sp
        );
        if (s->is_compiled && s->last_comp != NULL) {
            size_t l_idx = 0U;
            for (size_t i = 0U; i < s->last_comp->symbol_count; i++) {
                const forge_comp_symbol_t *sym = &s->last_comp->symbols[i];
                if ((sym->kind == FORGE_SYM_PARAM_VAR || sym->kind == FORGE_SYM_LOCAL_VAR) &&
                    strcmp(sym->scope_fn, fn_name) == 0) {
                    uint16_t addr = (uint16_t)((int32_t)cf->frame_fp + (int32_t)sym->fp_offset);
                    uint16_t val16 = 0U;
                    forge_mem_peek16(&cpu->mem, addr, &val16);
                    int16_t sval16 = (int16_t)val16;
                    buf_appendf(out_buf, out_buf_size, &pos, "%s{\"name\":", (l_idx > 0U ? "," : ""));
                    buf_append_json_str(out_buf, out_buf_size, &pos, sym->name);
                    buf_appendf(
                        out_buf, out_buf_size, &pos,
                        ",\"kind\":\"%s\",\"offset\":%d,\"address\":%u,\"value\":%u,\"signedValue\":%d}",
                        (sym->kind == FORGE_SYM_PARAM_VAR) ? "param" : "local",
                        (int)sym->fp_offset,
                        addr,
                        val16,
                        (int)sval16
                    );
                    l_idx++;
                }
            }
        }
        buf_appendf(out_buf, out_buf_size, &pos, "]}");
    }

    /* Symbols */
    buf_appendf(out_buf, out_buf_size, &pos, "],\"symbols\":[");
    for (size_t i = 0U; i < s->last_asm.symbol_count; i++) {
        const forge_asm_symbol_t *sym = &s->last_asm.symbols[i];
        buf_appendf(out_buf, out_buf_size, &pos, "%s{\"name\":", (i > 0U ? "," : ""));
        buf_append_json_str(out_buf, out_buf_size, &pos, sym->name);
        buf_appendf(
            out_buf, out_buf_size, &pos,
            ",\"value\":%u,\"line\":%u,\"isConstant\":%s}",
            sym->value,
            sym->line,
            sym->is_constant ? "true" : "false"
        );
    }

    /* Memory Window */
    buf_appendf(
        out_buf, out_buf_size, &pos,
        "],\"memory\":{\"start\":%u,\"length\":%u,\"lastRead\":%d,\"lastWrite\":%d,\"bytes\":[",
        mem_start,
        mem_len,
        cpu->mem.has_read ? (int)cpu->mem.last_read_addr : -1,
        cpu->mem.has_write ? (int)cpu->mem.last_write_addr : -1
    );
    for (uint32_t i = 0U; i < mem_len; i++) {
        uint32_t addr = (uint32_t)mem_start + i;
        uint8_t b = (addr < FORGE_MEM_SIZE) ? cpu->mem.data[addr] : 0U;
        buf_appendf(out_buf, out_buf_size, &pos, "%s%u", (i > 0U ? "," : ""), b);
    }
    buf_appendf(out_buf, out_buf_size, &pos, "]}");

    /* Watched Addresses */
    buf_appendf(out_buf, out_buf_size, &pos, ",\"watches\":[");
    for (size_t i = 0U; i < s->watch_count; i++) {
        uint16_t waddr = s->watches[i];
        uint16_t w16 = 0U;
        forge_mem_peek16(&cpu->mem, waddr, &w16);
        uint8_t w8 = cpu->mem.data[waddr];
        buf_appendf(
            out_buf, out_buf_size, &pos,
            "%s{\"address\":%u,\"value16\":%u,\"value8\":%u}",
            (i > 0U ? "," : ""),
            waddr,
            w16,
            w8
        );
    }

    /* Stack Entries (from current SP up to FORGE_STACK_TOP, max 32 words) */
    buf_appendf(out_buf, out_buf_size, &pos, "],\"stack\":[");
    if (cpu->sp >= FORGE_STACK_BOTTOM && cpu->sp < FORGE_STACK_TOP) {
        size_t idx = 0U;
        for (uint32_t a = cpu->sp; a + 2U <= FORGE_STACK_TOP && idx < 32U; a += 2U, idx++) {
            uint16_t sval = 0U;
            forge_mem_peek16(&cpu->mem, (uint16_t)a, &sval);
            buf_appendf(
                out_buf, out_buf_size, &pos,
                "%s{\"address\":%u,\"value\":%u}",
                (idx > 0U ? "," : ""),
                (uint16_t)a,
                sval
            );
        }
    }

    /* Execution History / Trace */
    buf_appendf(out_buf, out_buf_size, &pos, "],\"trace\":[");
    size_t trace_start = (cpu->trace_count > 64U) ? (cpu->trace_count - 64U) : 0U;
    for (size_t i = trace_start; i < cpu->trace_count; i++) {
        const forge_trace_entry_t *te = forge_cpu_get_trace(cpu, i);
        if (te == NULL) continue;
        char mnem[64];
        forge_disassemble_instruction(&te->instr, mnem, sizeof(mnem));
        buf_appendf(
            out_buf, out_buf_size, &pos,
            "%s{\"step\":%" PRIu64 ",\"pcBefore\":%u,\"pcAfter\":%u,\"spAfter\":%u,\"flagsAfter\":%u,\"cycles\":%u,\"mnemonic\":",
            (i > trace_start ? "," : ""),
            te->step_number,
            te->pc_before,
            te->pc_after,
            te->sp_after,
            te->flags_after,
            te->cycles
        );
        buf_append_json_str(out_buf, out_buf_size, &pos, mnem);
        buf_appendf(out_buf, out_buf_size, &pos, "}");
    }

    /* Console & Numeric Output */
    buf_appendf(out_buf, out_buf_size, &pos, "],\"console\":");
    buf_append_json_str(out_buf, out_buf_size, &pos, cpu->io.console_buf);
    buf_appendf(out_buf, out_buf_size, &pos, ",\"numericOutputs\":[");
    for (size_t i = 0U; i < cpu->io.int_output_count; i++) {
        buf_appendf(out_buf, out_buf_size, &pos, "%s%u", (i > 0U ? "," : ""), cpu->io.int_output_log[i]);
    }

    /* Virtual Framebuffer (64x32 at 0xF000..0xF7FF, 1 hex char per pixel '0'..'F') */
    char fb_hex[FORGE_FB_SIZE + 1U];
    static const char hex_chars[] = "0123456789ABCDEF";
    for (size_t i = 0U; i < FORGE_FB_SIZE; i++) {
        uint8_t b = cpu->mem.data[FORGE_VRAM_START + i];
        uint8_t c = (uint8_t)(b & 0x0FU);
        if (c == 0U && b != 0U) {
            c = 15U;
        }
        fb_hex[i] = hex_chars[c];
    }
    fb_hex[FORGE_FB_SIZE] = '\0';
    buf_appendf(
        out_buf, out_buf_size, &pos,
        "],\"framebuffer\":{\"width\":%u,\"height\":%u,\"vramStart\":%u,\"fbWrites\":%u,\"pixelsHex\":\"%s\"}",
        FORGE_FB_WIDTH,
        FORGE_FB_HEIGHT,
        FORGE_VRAM_START,
        cpu->os.fb_writes,
        fb_hex
    );

    /* ForgeOS Supervisor & Dynamic Heap State */
    buf_appendf(
        out_buf, out_buf_size, &pos,
        ",\"os\":{\"heapStart\":%u,\"heapBrk\":%u,\"heapLimit\":%u,\"heapUsedBytes\":%u,\"allocCount\":%u,\"syscallCount\":%u,\"lastSyscall\":%u,\"exitCode\":%u,\"inputQueueCount\":%zu}",
        cpu->os.heap_start,
        cpu->os.heap_brk,
        cpu->os.heap_limit,
        (uint16_t)(cpu->os.heap_brk - cpu->os.heap_start),
        cpu->os.alloc_count,
        cpu->os.syscall_count,
        cpu->os.last_syscall,
        cpu->os.exit_code,
        cpu->io.input_count
    );

    /* Diagnostics */
    buf_appendf(out_buf, out_buf_size, &pos, ",\"diagnostics\":[");
    if (s->is_compiled && s->last_comp != NULL) {
        for (size_t i = 0U; i < s->last_comp->error_count; i++) {
            const forge_comp_error_t *err = &s->last_comp->errors[i];
            buf_appendf(
                out_buf, out_buf_size, &pos,
                "%s{\"line\":%u,\"column\":%u,\"stage\":",
                (i > 0U ? "," : ""),
                err->line,
                err->column
            );
            buf_append_json_str(out_buf, out_buf_size, &pos, err->stage);
            buf_appendf(out_buf, out_buf_size, &pos, ",\"message\":");
            buf_append_json_str(out_buf, out_buf_size, &pos, err->message);
            buf_appendf(out_buf, out_buf_size, &pos, "}");
        }
    } else {
        for (size_t i = 0U; i < s->last_asm.error_count; i++) {
            const forge_asm_error_t *err = &s->last_asm.errors[i];
            buf_appendf(
                out_buf, out_buf_size, &pos,
                "%s{\"line\":%u,\"column\":%u,\"stage\":\"assembler\",\"message\":",
                (i > 0U ? "," : ""),
                err->line,
                err->column
            );
            buf_append_json_str(out_buf, out_buf_size, &pos, err->message);
            buf_appendf(out_buf, out_buf_size, &pos, "}");
        }
    }

    buf_appendf(out_buf, out_buf_size, &pos, "]}");
    return pos;
}

/* Lightweight JSON field parsers for incoming POST payloads */
static bool json_get_int(const char *json, const char *key, int32_t *out_val) {
    if (json == NULL || key == NULL || out_val == NULL) {
        return false;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    p += strlen(pattern);
    while (*p != '\0' && (isspace((unsigned char)*p) || *p == ':')) {
        p++;
    }
    char *end = NULL;
    long v = strtol(p, &end, 0);
    if (end == p) {
        return false;
    }
    *out_val = (int32_t)v;
    return true;
}

static bool json_get_bool(const char *json, const char *key, bool *out_val) {
    if (json == NULL || key == NULL || out_val == NULL) {
        return false;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    p += strlen(pattern);
    while (*p != '\0' && (isspace((unsigned char)*p) || *p == ':')) {
        p++;
    }
    if (strncmp(p, "true", 4U) == 0) {
        *out_val = true;
        return true;
    }
    if (strncmp(p, "false", 5U) == 0) {
        *out_val = false;
        return true;
    }
    return false;
}

static bool json_get_string(const char *json, const char *key, char *out_str, size_t max_len) {
    if (json == NULL || key == NULL || out_str == NULL || max_len == 0U) {
        return false;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    p += strlen(pattern);
    while (*p != '\0' && isspace((unsigned char)*p)) p++;
    if (*p != ':') return false;
    p++;
    while (*p != '\0' && isspace((unsigned char)*p)) p++;
    if (*p != '"') return false;
    p++;

    size_t idx = 0U;
    while (*p != '\0' && *p != '"' && idx + 1U < max_len) {
        if (*p == '\\') {
            p++;
            if (*p == '\0') break;
            switch (*p) {
                case 'n':  out_str[idx++] = '\n'; break;
                case 'r':  out_str[idx++] = '\r'; break;
                case 't':  out_str[idx++] = '\t'; break;
                case '"':  out_str[idx++] = '"';  break;
                case '\\': out_str[idx++] = '\\'; break;
                default:   out_str[idx++] = *p;   break;
            }
            p++;
        } else {
            out_str[idx++] = *p++;
        }
    }
    out_str[idx] = '\0';
    return true;
}

static void parse_query_mem_params(const char *uri, const char *body, uint16_t *mem_start, uint16_t *mem_len) {
    *mem_start = FORGE_DEFAULT_PC;
    *mem_len   = 256U;

    int32_t ms = 0, ml = 0;
    if (body != NULL) {
        if (json_get_int(body, "memStart", &ms) && ms >= 0 && ms <= 0xFFFF) {
            *mem_start = (uint16_t)ms;
        }
        if (json_get_int(body, "memLen", &ml) && ml > 0 && ml <= 1024) {
            *mem_len = (uint16_t)ml;
        }
    }
    if (uri != NULL) {
        const char *q = strchr(uri, '?');
        if (q != NULL) {
            const char *p_ms = strstr(q, "memStart=");
            if (p_ms != NULL) {
                long v = strtol(p_ms + 9, NULL, 0);
                if (v >= 0 && v <= 0xFFFF) *mem_start = (uint16_t)v;
            }
            const char *p_ml = strstr(q, "memLen=");
            if (p_ml != NULL) {
                long v = strtol(p_ml + 7, NULL, 0);
                if (v > 0 && v <= 1024) *mem_len = (uint16_t)v;
            }
        }
    }
}

static bool resolve_source_line_to_addr(const forge_bridge_session_t *s, uint32_t src_line, uint16_t *out_addr) {
    if (s == NULL || out_addr == NULL || src_line == 0U) {
        return false;
    }
    if (s->is_compiled && s->last_comp != NULL) {
        for (size_t i = 0U; i < s->last_comp->source_map_count; i++) {
            if (s->last_comp->source_map[i].source_line == src_line) {
                *out_addr = s->last_comp->source_map[i].address;
                return true;
            }
        }
    } else {
        for (size_t i = 0U; i < s->last_asm.line_map_count; i++) {
            if (s->last_asm.line_map[i].line == src_line) {
                *out_addr = s->last_asm.line_map[i].address;
                return true;
            }
        }
    }
    return false;
}

int forge_bridge_handle_request(
    forge_bridge_session_t *session,
    const char *method,
    const char *uri,
    const char *body,
    char *out_json,
    size_t out_json_size
) {
    if (session == NULL || method == NULL || uri == NULL || out_json == NULL || out_json_size == 0U) {
        return 400;
    }

    uint16_t mem_start = FORGE_DEFAULT_PC;
    uint16_t mem_len   = 256U;
    parse_query_mem_params(uri, body, &mem_start, &mem_len);

    if (strncmp(uri, "/api/state", 10U) == 0) {
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/assemble", 13U) == 0) {
        char *src_buf = (char *)malloc(65536U);
        if (src_buf == NULL) {
            return 500;
        }
        if (json_get_string(body, "source", src_buf, 65536U)) {
            forge_bridge_assemble(session, src_buf);
        }
        free(src_buf);
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/compile", 12U) == 0) {
        char *src_buf = (char *)malloc(65536U);
        if (src_buf == NULL) {
            return 500;
        }
        if (json_get_string(body, "source", src_buf, 65536U)) {
            forge_bridge_compile(session, src_buf);
        }
        free(src_buf);
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/step", 9U) == 0) {
        char mode[32] = {0};
        if (json_get_string(body, "mode", mode, sizeof(mode))) {
            if (strcmp(mode, "line") == 0) {
                forge_bridge_step_line(session);
            } else if (strcmp(mode, "over") == 0) {
                forge_bridge_step_over(session, session->is_compiled);
            } else if (strcmp(mode, "out") == 0) {
                forge_bridge_step_out(session);
            } else {
                int32_t count = 1;
                json_get_int(body, "count", &count);
                if (count < 1) count = 1;
                forge_bridge_step(session, (uint32_t)count);
            }
        } else {
            int32_t count = 1;
            json_get_int(body, "count", &count);
            if (count < 1) count = 1;
            forge_bridge_step(session, (uint32_t)count);
        }
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/run", 8U) == 0) {
        int32_t max_steps = 100000;
        json_get_int(body, "maxSteps", &max_steps);
        if (max_steps < 1) max_steps = 100000;
        forge_bridge_run(session, (uint32_t)max_steps);
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/reset", 10U) == 0) {
        forge_bridge_reset(session);
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/breakpoint", 15U) == 0) {
        char action[32];
        if (json_get_string(body, "action", action, sizeof(action)) && strcmp(action, "clear") == 0) {
            forge_cpu_clear_breakpoints(&session->cpu);
        } else {
            int32_t addr = -1;
            int32_t src_line = -1;
            if (!json_get_int(body, "address", &addr)) {
                if (json_get_int(body, "sourceLine", &src_line) && src_line > 0) {
                    uint16_t resolved = 0U;
                    if (resolve_source_line_to_addr(session, (uint32_t)src_line, &resolved)) {
                        addr = (int32_t)resolved;
                    }
                }
            }
            bool enabled = true;
            if (addr >= 0 && addr <= 0xFFFF) {
                if (json_get_bool(body, "enabled", &enabled)) {
                    if (enabled) {
                        forge_cpu_add_breakpoint(&session->cpu, (uint16_t)addr);
                    } else {
                        forge_cpu_remove_breakpoint(&session->cpu, (uint16_t)addr);
                    }
                } else {
                    if (forge_cpu_has_breakpoint(&session->cpu, (uint16_t)addr)) {
                        forge_cpu_remove_breakpoint(&session->cpu, (uint16_t)addr);
                    } else {
                        forge_cpu_add_breakpoint(&session->cpu, (uint16_t)addr);
                    }
                }
            }
        }
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/watch", 10U) == 0) {
        int32_t addr = -1;
        bool enabled = true;
        if (json_get_int(body, "address", &addr) && addr >= 0 && addr <= 0xFFFF) {
            if (json_get_bool(body, "enabled", &enabled) && !enabled) {
                forge_bridge_remove_watch(session, (uint16_t)addr);
            } else {
                forge_bridge_add_watch(session, (uint16_t)addr);
            }
        }
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/memory/write", 17U) == 0) {
        int32_t addr = -1, val = 0, width = 8;
        if (json_get_int(body, "address", &addr) && json_get_int(body, "value", &val)) {
            json_get_int(body, "width", &width);
            if (addr >= 0 && addr <= 0xFFFF) {
                forge_error_t merr = (width == 16)
                                         ? forge_mem_write16(&session->cpu.mem, (uint16_t)addr, (uint16_t)(val & 0xFFFF))
                                         : forge_mem_write8(&session->cpu.mem, (uint16_t)addr, (uint8_t)(val & 0xFF));
                if (merr != FORGE_OK) {
                    session->cpu.error = merr;
                }
            }
        }
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/register/write", 19U) == 0) {
        int32_t reg = -1, val = 0;
        if (json_get_int(body, "reg", &reg) && json_get_int(body, "value", &val)) {
            if (reg >= 0 && reg < (int32_t)FORGE_NUM_REGS) {
                forge_cpu_set_reg(&session->cpu, (uint8_t)reg, (uint16_t)(val & 0xFFFF));
            }
        }
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/io/input", 13U) == 0) {
        char text_buf[256];
        int32_t num_val = 0;
        if (json_get_string(body, "text", text_buf, sizeof(text_buf)) && text_buf[0] != '\0') {
            forge_io_push_input_str(&session->cpu.io, text_buf);
        } else if (json_get_int(body, "value", &num_val)) {
            forge_io_push_input(&session->cpu.io, (uint16_t)(num_val & 0xFFFF));
        }
        forge_bridge_state_json(session, mem_start, mem_len, out_json, out_json_size);
        return 200;
    }

    snprintf(out_json, out_json_size, "{\"error\":\"Not found\"}");
    return 404;
}
