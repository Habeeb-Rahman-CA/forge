#ifndef FORGE_CPU_H
#define FORGE_CPU_H

#include "forge_alu.h"
#include "forge_io.h"
#include "forge_isa.h"
#include "forge_memory.h"
#include "forge_os.h"
#include "forge_stack.h"
#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FORGE_TRACE_CAPACITY  256U
#define FORGE_MAX_BREAKPOINTS 64U
#define FORGE_MAX_CALL_DEPTH  64U

/* Binary executable header (.fbin) constants */
#define FORGE_FBIN_MAGIC_0 'F'
#define FORGE_FBIN_MAGIC_1 'R'
#define FORGE_FBIN_MAGIC_2 'G'
#define FORGE_FBIN_MAGIC_3 '1'
#define FORGE_FBIN_HEADER_SIZE 16U
#define FORGE_FBIN_VERSION     1U

typedef struct {
    uint16_t callee_addr;
    uint16_t caller_pc;
    uint16_t return_addr;
    uint16_t frame_fp;
    uint16_t frame_sp;
} forge_call_frame_t;

typedef struct {
    uint64_t step_number;
    uint16_t pc_before;
    uint16_t pc_after;
    uint16_t sp_before;
    uint16_t sp_after;
    uint16_t flags_before;
    uint16_t flags_after;
    uint16_t regs_before[FORGE_NUM_REGS];
    uint16_t regs_after[FORGE_NUM_REGS];
    forge_instruction_t instr;
    uint32_t cycles;
    forge_error_t error;
} forge_trace_entry_t;

typedef struct forge_cpu_s {
    /* Architectural Registers */
    uint16_t r[FORGE_NUM_REGS];
    uint16_t pc;
    uint16_t sp;
    uint16_t flags;

    /* Execution State */
    bool halted;
    forge_error_t error;
    uint16_t fault_pc;
    uint64_t cycles;
    uint64_t instructions_executed;
    uint64_t max_instructions;

    /* Subsystems */
    forge_memory_t mem;
    forge_io_t io;
    forge_os_state_t os;

    /* Breakpoints */
    uint16_t breakpoints[FORGE_MAX_BREAKPOINTS];
    size_t breakpoint_count;

    /* Call Stack Tracking (CALL / RET) */
    forge_call_frame_t call_stack[FORGE_MAX_CALL_DEPTH];
    size_t call_depth;

    /* Instruction Tracing Ring Buffer */
    bool trace_enabled;
    forge_trace_entry_t trace[FORGE_TRACE_CAPACITY];
    size_t trace_head;
    size_t trace_count;
} forge_cpu_t;

/* CPU Lifecycle & State Management */
void forge_cpu_init(forge_cpu_t *cpu);
void forge_cpu_reset(forge_cpu_t *cpu, bool preserve_memory);

/* Register & Flag Inspection/Manipulation */
uint16_t forge_cpu_get_reg(const forge_cpu_t *cpu, uint8_t reg);
forge_error_t forge_cpu_set_reg(forge_cpu_t *cpu, uint8_t reg, uint16_t val);
bool forge_cpu_get_flag(const forge_cpu_t *cpu, uint16_t flag_mask);
void forge_cpu_set_flag(forge_cpu_t *cpu, uint16_t flag_mask, bool enabled);

/* Program Loader */
forge_error_t forge_cpu_load_raw(
    forge_cpu_t *cpu,
    const uint8_t *bytes,
    size_t len,
    uint16_t load_addr,
    uint16_t entry_pc
);

forge_error_t forge_cpu_load_fbin(
    forge_cpu_t *cpu,
    const uint8_t *bytes,
    size_t len
);

forge_error_t forge_cpu_load_binary(
    forge_cpu_t *cpu,
    const uint8_t *bytes,
    size_t len,
    uint16_t default_load_addr
);

forge_error_t forge_cpu_load_file(
    forge_cpu_t *cpu,
    const char *filepath,
    uint16_t default_load_addr
);

/* Helper to emit an encoded instruction directly into memory at addr */
forge_error_t forge_cpu_emit_instr(
    forge_cpu_t *cpu,
    uint16_t *inout_addr,
    forge_opcode_t opcode,
    forge_addr_mode_t mode,
    uint8_t rd,
    uint8_t rs,
    uint16_t imm16
);

/* Breakpoints */
bool forge_cpu_add_breakpoint(forge_cpu_t *cpu, uint16_t addr);
bool forge_cpu_remove_breakpoint(forge_cpu_t *cpu, uint16_t addr);
void forge_cpu_clear_breakpoints(forge_cpu_t *cpu);
bool forge_cpu_has_breakpoint(const forge_cpu_t *cpu, uint16_t addr);

/* Execution Engine */
forge_error_t forge_cpu_step(forge_cpu_t *cpu);
forge_error_t forge_cpu_run(forge_cpu_t *cpu);

/* Trace Inspection & Formatting */
const forge_trace_entry_t *forge_cpu_get_trace(const forge_cpu_t *cpu, size_t index);
void forge_cpu_format_trace_entry(const forge_trace_entry_t *entry, char *buf, size_t buf_size);
void forge_cpu_format_state(const forge_cpu_t *cpu, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_CPU_H */

