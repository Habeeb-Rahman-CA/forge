#include "forge_cpu.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void forge_cpu_init(forge_cpu_t *cpu) {
    if (cpu == NULL) {
        return;
    }
    memset(cpu, 0, sizeof(*cpu));
    forge_mem_init(&cpu->mem);
    forge_io_init(&cpu->io);
    forge_os_init(&cpu->os);
    cpu->pc = FORGE_DEFAULT_PC;
    cpu->sp = FORGE_DEFAULT_SP;
    cpu->flags = 0U;
    cpu->halted = false;
    cpu->error = FORGE_OK;
    cpu->fault_pc = 0U;
    cpu->cycles = 0ULL;
    cpu->instructions_executed = 0ULL;
    cpu->max_instructions = FORGE_DEFAULT_MAX_STEPS;
    cpu->breakpoint_count = 0U;
    cpu->call_depth = 0U;
    cpu->trace_enabled = false;
    cpu->trace_head = 0U;
    cpu->trace_count = 0U;
}

void forge_cpu_reset(forge_cpu_t *cpu, bool preserve_memory) {
    if (cpu == NULL) {
        return;
    }
    memset(cpu->r, 0, sizeof(cpu->r));
    cpu->pc = FORGE_DEFAULT_PC;
    cpu->sp = FORGE_DEFAULT_SP;
    cpu->flags = 0U;
    cpu->halted = false;
    cpu->error = FORGE_OK;
    cpu->fault_pc = 0U;
    cpu->cycles = 0ULL;
    cpu->instructions_executed = 0ULL;

    if (!preserve_memory) {
        forge_mem_init(&cpu->mem);
    } else {
        cpu->mem.has_read = false;
        cpu->mem.has_write = false;
        memset(&cpu->mem.data[FORGE_VRAM_START], 0, FORGE_FB_SIZE);
    }
    forge_io_init(&cpu->io);
    forge_os_init(&cpu->os);
    cpu->call_depth = 0U;
    cpu->trace_head = 0U;
    cpu->trace_count = 0U;
}

uint16_t forge_cpu_get_reg(const forge_cpu_t *cpu, uint8_t reg) {
    if (cpu == NULL || reg >= FORGE_NUM_REGS) {
        return 0U;
    }
    return cpu->r[reg];
}

forge_error_t forge_cpu_set_reg(forge_cpu_t *cpu, uint8_t reg, uint16_t val) {
    if (cpu == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (reg >= FORGE_NUM_REGS) {
        return FORGE_ERR_INVALID_INSTRUCTION;
    }
    cpu->r[reg] = val;
    return FORGE_OK;
}

bool forge_cpu_get_flag(const forge_cpu_t *cpu, uint16_t flag_mask) {
    if (cpu == NULL) {
        return false;
    }
    return (cpu->flags & flag_mask) != 0U;
}

void forge_cpu_set_flag(forge_cpu_t *cpu, uint16_t flag_mask, bool enabled) {
    if (cpu == NULL) {
        return;
    }
    if (enabled) {
        cpu->flags = (uint16_t)((cpu->flags | flag_mask) & FORGE_FLAG_MASK);
    } else {
        cpu->flags = (uint16_t)((cpu->flags & ~flag_mask) & FORGE_FLAG_MASK);
    }
}

bool forge_cpu_add_breakpoint(forge_cpu_t *cpu, uint16_t addr) {
    if (cpu == NULL) {
        return false;
    }
    for (size_t i = 0U; i < cpu->breakpoint_count; i++) {
        if (cpu->breakpoints[i] == addr) {
            return true;
        }
    }
    if (cpu->breakpoint_count >= FORGE_MAX_BREAKPOINTS) {
        return false;
    }
    cpu->breakpoints[cpu->breakpoint_count++] = addr;
    return true;
}

bool forge_cpu_remove_breakpoint(forge_cpu_t *cpu, uint16_t addr) {
    if (cpu == NULL) {
        return false;
    }
    for (size_t i = 0U; i < cpu->breakpoint_count; i++) {
        if (cpu->breakpoints[i] == addr) {
            for (size_t j = i; j + 1U < cpu->breakpoint_count; j++) {
                cpu->breakpoints[j] = cpu->breakpoints[j + 1U];
            }
            cpu->breakpoint_count--;
            return true;
        }
    }
    return false;
}

void forge_cpu_clear_breakpoints(forge_cpu_t *cpu) {
    if (cpu == NULL) {
        return;
    }
    cpu->breakpoint_count = 0U;
}

bool forge_cpu_has_breakpoint(const forge_cpu_t *cpu, uint16_t addr) {
    if (cpu == NULL) {
        return false;
    }
    for (size_t i = 0U; i < cpu->breakpoint_count; i++) {
        if (cpu->breakpoints[i] == addr) {
            return true;
        }
    }
    return false;
}

const forge_trace_entry_t *forge_cpu_get_trace(const forge_cpu_t *cpu, size_t index) {
    if (cpu == NULL || index >= cpu->trace_count) {
        return NULL;
    }
    size_t start = (cpu->trace_count < FORGE_TRACE_CAPACITY)
                       ? 0U
                       : cpu->trace_head;
    size_t pos = (start + index) % FORGE_TRACE_CAPACITY;
    return &cpu->trace[pos];
}

void forge_cpu_format_trace_entry(const forge_trace_entry_t *entry, char *buf, size_t buf_size) {
    if (buf == NULL || buf_size == 0U) {
        return;
    }
    if (entry == NULL) {
        snprintf(buf, buf_size, "<null trace>");
        return;
    }
    char disasm[64];
    forge_disassemble_instruction(&entry->instr, disasm, sizeof(disasm));
    snprintf(
        buf,
        buf_size,
        "[#%04" PRIu64 "] PC:0x%04X->0x%04X | %-20s | SP:0x%04X FLAGS:%c%c%c%c | CYC:%u",
        entry->step_number,
        entry->pc_before,
        entry->pc_after,
        disasm,
        entry->sp_after,
        (entry->flags_after & FORGE_FLAG_Z) ? 'Z' : '-',
        (entry->flags_after & FORGE_FLAG_N) ? 'N' : '-',
        (entry->flags_after & FORGE_FLAG_C) ? 'C' : '-',
        (entry->flags_after & FORGE_FLAG_V) ? 'V' : '-',
        entry->cycles
    );
}

void forge_cpu_format_state(const forge_cpu_t *cpu, char *buf, size_t buf_size) {
    if (buf == NULL || buf_size == 0U || cpu == NULL) {
        return;
    }
    snprintf(
        buf,
        buf_size,
        "PC: 0x%04X  SP: 0x%04X  FLAGS: [%c%c%c%c] (0x%04X)  HALTED: %s  ERR: %s\n"
        "R0: 0x%04X (%5u)  R1: 0x%04X (%5u)  R2: 0x%04X (%5u)  R3: 0x%04X (%5u)\n"
        "R4: 0x%04X (%5u)  R5: 0x%04X (%5u)  R6: 0x%04X (%5u)  R7: 0x%04X (%5u)\n"
        "Instructions: %" PRIu64 "  Cycles: %" PRIu64,
        cpu->pc,
        cpu->sp,
        (cpu->flags & FORGE_FLAG_Z) ? 'Z' : '-',
        (cpu->flags & FORGE_FLAG_N) ? 'N' : '-',
        (cpu->flags & FORGE_FLAG_C) ? 'C' : '-',
        (cpu->flags & FORGE_FLAG_V) ? 'V' : '-',
        cpu->flags,
        cpu->halted ? "YES" : "NO",
        forge_error_str(cpu->error),
        cpu->r[0], cpu->r[0],
        cpu->r[1], cpu->r[1],
        cpu->r[2], cpu->r[2],
        cpu->r[3], cpu->r[3],
        cpu->r[4], cpu->r[4],
        cpu->r[5], cpu->r[5],
        cpu->r[6], cpu->r[6],
        cpu->r[7], cpu->r[7],
        cpu->instructions_executed,
        cpu->cycles
    );
}

