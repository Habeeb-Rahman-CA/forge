#include "forge_cpu.h"
#include <string.h>

static void record_trace(
    forge_cpu_t *cpu,
    uint16_t pc_before,
    uint16_t sp_before,
    uint16_t flags_before,
    const uint16_t regs_before[FORGE_NUM_REGS],
    const forge_instruction_t *instr,
    forge_error_t err
) {
    if (!cpu->trace_enabled) {
        return;
    }
    forge_trace_entry_t *entry = &cpu->trace[cpu->trace_head];
    entry->step_number  = cpu->instructions_executed;
    entry->pc_before    = pc_before;
    entry->pc_after     = cpu->pc;
    entry->sp_before    = sp_before;
    entry->sp_after     = cpu->sp;
    entry->flags_before = flags_before;
    entry->flags_after  = cpu->flags;
    memcpy(entry->regs_before, regs_before, sizeof(entry->regs_before));
    memcpy(entry->regs_after, cpu->r, sizeof(entry->regs_after));
    entry->instr  = *instr;
    entry->cycles = instr->cycles;
    entry->error  = err;

    cpu->trace_head = (cpu->trace_head + 1U) % FORGE_TRACE_CAPACITY;
    if (cpu->trace_count < FORGE_TRACE_CAPACITY) {
        cpu->trace_count++;
    }
}

static forge_error_t fault_cpu(forge_cpu_t *cpu, uint16_t fault_pc, forge_error_t err) {
    cpu->error    = err;
    cpu->fault_pc = fault_pc;
    cpu->halted   = true;
    return err;
}

static bool evaluate_branch_condition(forge_opcode_t op, uint16_t flags) {
    bool z = (flags & FORGE_FLAG_Z) != 0U;
    bool n = (flags & FORGE_FLAG_N) != 0U;
    bool c = (flags & FORGE_FLAG_C) != 0U;
    bool v = (flags & FORGE_FLAG_V) != 0U;

    switch (op) {
        case FORGE_OP_JMP: return true;
        case FORGE_OP_JZ:  return z;
        case FORGE_OP_JNZ: return !z;
        case FORGE_OP_JC:  return c;
        case FORGE_OP_JNC: return !c;
        case FORGE_OP_JN:  return n;
        case FORGE_OP_JL:  return (n != v);
        case FORGE_OP_JLE: return z || (n != v);
        case FORGE_OP_JG:  return !z && (n == v);
        case FORGE_OP_JGE: return (n == v);
        default:           return false;
    }
}

forge_error_t forge_cpu_step(forge_cpu_t *cpu) {
    if (cpu == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (cpu->error != FORGE_OK) {
        return cpu->error;
    }
    if (cpu->halted) {
        return FORGE_OK;
    }
    if (cpu->max_instructions > 0ULL && cpu->instructions_executed >= cpu->max_instructions) {
        cpu->error = FORGE_ERR_EXECUTION_LIMIT;
        return FORGE_ERR_EXECUTION_LIMIT;
    }

    uint16_t pc_before    = cpu->pc;
    uint16_t sp_before    = cpu->sp;
    uint16_t flags_before = cpu->flags;
    uint16_t regs_before[FORGE_NUM_REGS];
    memcpy(regs_before, cpu->r, sizeof(regs_before));

    /* Ensure 4-byte instruction does not wrap or cross 0xFFFF */
    if (pc_before > 0xFFFCU) {
        return fault_cpu(cpu, pc_before, FORGE_ERR_MEMORY_OUT_OF_BOUNDS);
    }

    uint16_t word0 = 0U;
    uint16_t word1 = 0U;
    forge_error_t err = forge_mem_read16(&cpu->mem, pc_before, &word0);
    if (err != FORGE_OK) {
        return fault_cpu(cpu, pc_before, err);
    }
    err = forge_mem_read16(&cpu->mem, (uint16_t)(pc_before + 2U), &word1);
    if (err != FORGE_OK) {
        return fault_cpu(cpu, pc_before, err);
    }

    forge_instruction_t instr;
    err = forge_decode_instruction(word0, word1, &instr);
    if (err != FORGE_OK) {
        record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
        return fault_cpu(cpu, pc_before, err);
    }

    uint16_t next_pc = (uint16_t)(pc_before + 4U);
    uint16_t src_val = (instr.mode == FORGE_MODE_IMM) ? instr.imm16 : cpu->r[instr.rs];
    uint16_t eff_addr = (instr.mode == FORGE_MODE_MEM_DIR)
                            ? instr.imm16
                            : (uint16_t)(cpu->r[instr.rs] + instr.imm16);

    forge_alu_result_t alu;

    switch (instr.opcode) {
        case FORGE_OP_NOP:
            break;

        case FORGE_OP_HALT:
            cpu->halted = true;
            break;

        case FORGE_OP_MOV:
            cpu->r[instr.rd] = src_val;
            break;

        case FORGE_OP_LOAD: {
            uint16_t loaded = 0U;
            if (eff_addr == FORGE_MMIO_TIMER_LO) {
                loaded = (uint16_t)(cpu->cycles & 0xFFFFU);
                cpu->mem.last_read_addr = eff_addr;
                cpu->mem.has_read = true;
            } else if (eff_addr == FORGE_MMIO_KEY_STATUS) {
                loaded = (cpu->io.input_count > 0U) ? 1U : 0U;
                cpu->mem.last_read_addr = eff_addr;
                cpu->mem.has_read = true;
            } else if (eff_addr == FORGE_MMIO_KEY_DATA) {
                err = forge_io_read_port(&cpu->io, FORGE_PORT_CONSOLE_CHAR, &loaded);
                if (err != FORGE_OK) {
                    record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                    return fault_cpu(cpu, pc_before, err);
                }
                cpu->mem.last_read_addr = eff_addr;
                cpu->mem.has_read = true;
            } else if (eff_addr == FORGE_MMIO_HEAP_BRK) {
                loaded = cpu->os.heap_brk;
                cpu->mem.last_read_addr = eff_addr;
                cpu->mem.has_read = true;
            } else if (eff_addr == FORGE_MMIO_RANDOM) {
                loaded = forge_os_next_random(&cpu->os);
                cpu->mem.last_read_addr = eff_addr;
                cpu->mem.has_read = true;
            } else {
                err = forge_mem_read16(&cpu->mem, eff_addr, &loaded);
                if (err != FORGE_OK) {
                    record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                    return fault_cpu(cpu, pc_before, err);
                }
            }
            cpu->r[instr.rd] = loaded;
            break;
        }

        case FORGE_OP_STORE: {
            err = forge_mem_write16(&cpu->mem, eff_addr, cpu->r[instr.rd]);
            if (err != FORGE_OK) {
                record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                return fault_cpu(cpu, pc_before, err);
            }
            if (eff_addr >= FORGE_VRAM_START && eff_addr <= FORGE_VRAM_END) {
                cpu->os.fb_writes++;
            }
            break;
        }

        case FORGE_OP_ADD:
            alu = forge_alu_add(cpu->r[instr.rd], src_val);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_SUB:
            alu = forge_alu_sub(cpu->r[instr.rd], src_val);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_MUL:
            alu = forge_alu_mul(cpu->r[instr.rd], src_val);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_DIV:
            alu = forge_alu_div(cpu->r[instr.rd], src_val);
            if (alu.error != FORGE_OK) {
                record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, alu.error);
                return fault_cpu(cpu, pc_before, alu.error);
            }
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_AND:
            alu = forge_alu_and(cpu->r[instr.rd], src_val);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_OR:
            alu = forge_alu_or(cpu->r[instr.rd], src_val);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_XOR:
            alu = forge_alu_xor(cpu->r[instr.rd], src_val);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_NOT:
            alu = forge_alu_not(cpu->r[instr.rd]);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_SHL:
            alu = forge_alu_shl(cpu->r[instr.rd], src_val);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_SHR:
            alu = forge_alu_shr(cpu->r[instr.rd], src_val);
            cpu->r[instr.rd] = alu.result;
            cpu->flags       = alu.flags;
            break;

        case FORGE_OP_CMP:
            alu = forge_alu_cmp(cpu->r[instr.rd], src_val);
            cpu->flags = alu.flags;
            break;

        case FORGE_OP_JMP:
        case FORGE_OP_JZ:
        case FORGE_OP_JNZ:
        case FORGE_OP_JC:
        case FORGE_OP_JNC:
        case FORGE_OP_JN:
        case FORGE_OP_JL:
        case FORGE_OP_JLE:
        case FORGE_OP_JG:
        case FORGE_OP_JGE:
            if (evaluate_branch_condition(instr.opcode, cpu->flags)) {
                next_pc = src_val;
            }
            break;

        case FORGE_OP_CALL:
            err = forge_stack_push16(&cpu->mem, &cpu->sp, next_pc);
            if (err != FORGE_OK) {
                record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                return fault_cpu(cpu, pc_before, err);
            }
            if (cpu->call_depth < FORGE_MAX_CALL_DEPTH) {
                forge_call_frame_t *cf = &cpu->call_stack[cpu->call_depth];
                cf->callee_addr = src_val;
                cf->caller_pc   = pc_before;
                cf->return_addr = next_pc;
                cf->frame_fp    = cpu->r[FORGE_FP];
                cf->frame_sp    = cpu->sp;
            }
            cpu->call_depth++;
            next_pc = src_val;
            break;

        case FORGE_OP_RET: {
            uint16_t ret_addr = 0U;
            err = forge_stack_pop16(&cpu->mem, &cpu->sp, &ret_addr);
            if (err != FORGE_OK) {
                record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                return fault_cpu(cpu, pc_before, err);
            }
            if (cpu->call_depth > 0U) {
                cpu->call_depth--;
            }
            next_pc = ret_addr;
            break;
        }

        case FORGE_OP_PUSH: {
            uint16_t push_val = (instr.mode == FORGE_MODE_IMM) ? instr.imm16 : cpu->r[instr.rd];
            err = forge_stack_push16(&cpu->mem, &cpu->sp, push_val);
            if (err != FORGE_OK) {
                record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                return fault_cpu(cpu, pc_before, err);
            }
            break;
        }

        case FORGE_OP_POP: {
            uint16_t pop_val = 0U;
            err = forge_stack_pop16(&cpu->mem, &cpu->sp, &pop_val);
            if (err != FORGE_OK) {
                record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                return fault_cpu(cpu, pc_before, err);
            }
            cpu->r[instr.rd] = pop_val;
            break;
        }

        case FORGE_OP_IN: {
            uint16_t in_val = 0U;
            err = forge_io_read_port(&cpu->io, src_val, &in_val);
            if (err != FORGE_OK) {
                record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                return fault_cpu(cpu, pc_before, err);
            }
            cpu->r[instr.rd] = in_val;
            break;
        }

        case FORGE_OP_OUT: {
            uint16_t out_val = cpu->r[instr.rd];
            err = forge_io_write_port(&cpu->io, src_val, out_val);
            if (err != FORGE_OK) {
                record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                return fault_cpu(cpu, pc_before, err);
            }
            if (src_val == FORGE_PORT_SYSCALL) {
                err = forge_os_handle_syscall(cpu, out_val);
                if (err != FORGE_OK) {
                    record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, err);
                    return fault_cpu(cpu, pc_before, err);
                }
            }
            break;
        }

        default:
            record_trace(
                cpu, pc_before, sp_before, flags_before, regs_before,
                &instr, FORGE_ERR_INVALID_INSTRUCTION
            );
            return fault_cpu(cpu, pc_before, FORGE_ERR_INVALID_INSTRUCTION);
    }

    cpu->pc = next_pc;
    cpu->instructions_executed++;
    cpu->cycles += instr.cycles;

    record_trace(cpu, pc_before, sp_before, flags_before, regs_before, &instr, FORGE_OK);
    return FORGE_OK;
}

forge_error_t forge_cpu_run(forge_cpu_t *cpu) {
    if (cpu == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    bool first_step = true;
    while (!cpu->halted && cpu->error == FORGE_OK) {
        if (!first_step && forge_cpu_has_breakpoint(cpu, cpu->pc)) {
            return FORGE_ERR_BREAKPOINT;
        }
        first_step = false;
        forge_error_t err = forge_cpu_step(cpu);
        if (err != FORGE_OK) {
            return err;
        }
    }
    return cpu->error;
}

