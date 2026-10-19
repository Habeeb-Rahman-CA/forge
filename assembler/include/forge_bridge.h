#ifndef FORGE_BRIDGE_H
#define FORGE_BRIDGE_H

#include "forge_asm.h"
#include "forge_compiler.h"
#include "forge_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FORGE_MAX_WATCHES 32U

typedef struct {
    forge_cpu_t cpu;
    forge_asm_result_t last_asm;
    forge_compile_result_t *last_comp; /* Allocated on demand for high-level compilation */
    bool is_compiled;                  /* true if loaded via /api/compile, false if /api/assemble */
    bool has_program;

    /* Previous step snapshot for delta highlighting */
    uint16_t prev_r[FORGE_NUM_REGS];
    uint16_t prev_pc;
    uint16_t prev_sp;
    uint16_t prev_flags;

    /* Watched memory addresses */
    uint16_t watches[FORGE_MAX_WATCHES];
    size_t watch_count;
} forge_bridge_session_t;

void forge_bridge_init(forge_bridge_session_t *session);
void forge_bridge_destroy(forge_bridge_session_t *session);

bool forge_bridge_assemble(forge_bridge_session_t *session, const char *source);
bool forge_bridge_compile(forge_bridge_session_t *session, const char *source);

forge_error_t forge_bridge_step(forge_bridge_session_t *session, uint32_t count);
forge_error_t forge_bridge_step_line(forge_bridge_session_t *session);
forge_error_t forge_bridge_step_over(forge_bridge_session_t *session, bool line_mode);
forge_error_t forge_bridge_step_out(forge_bridge_session_t *session);

forge_error_t forge_bridge_run(forge_bridge_session_t *session, uint32_t max_steps);

void forge_bridge_reset(forge_bridge_session_t *session);

uint32_t forge_bridge_source_line_for_pc(const forge_bridge_session_t *session, uint16_t pc);
uint32_t forge_bridge_asm_line_for_pc(const forge_bridge_session_t *session, uint16_t pc);

bool forge_bridge_add_watch(forge_bridge_session_t *session, uint16_t addr);
bool forge_bridge_remove_watch(forge_bridge_session_t *session, uint16_t addr);

/* Serialize full debugger & CPU state into a JSON buffer */
size_t forge_bridge_state_json(
    const forge_bridge_session_t *session,
    uint16_t mem_start,
    uint16_t mem_len,
    char *out_buf,
    size_t out_buf_size
);

/* Dispatch an HTTP API request ("GET"/"POST", "/api/...", request body) and write JSON response */
int forge_bridge_handle_request(
    forge_bridge_session_t *session,
    const char *method,
    const char *uri,
    const char *body,
    char *out_json,
    size_t out_json_size
);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_BRIDGE_H */
