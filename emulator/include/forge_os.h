#ifndef FORGE_OS_H
#define FORGE_OS_H

#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Virtual Framebuffer Geometry (mapped at FORGE_VRAM_START = 0xF000..0xF7FF) */
#define FORGE_FB_WIDTH  64U
#define FORGE_FB_HEIGHT 32U
#define FORGE_FB_SIZE   (FORGE_FB_WIDTH * FORGE_FB_HEIGHT) /* 2048 bytes */

/* Dynamic Heap Boundaries (0x8000..0x8FFF reserved for static globals) */
#define FORGE_DYNAMIC_HEAP_START 0x9000U
#define FORGE_DYNAMIC_HEAP_LIMIT 0xE000U

/* Memory-Mapped I/O Register Addresses (0xFF00..0xFFFF) */
#define FORGE_MMIO_TIMER_LO   0xFF00U
#define FORGE_MMIO_KEY_STATUS 0xFF02U
#define FORGE_MMIO_KEY_DATA   0xFF04U
#define FORGE_MMIO_HEAP_BRK   0xFF06U
#define FORGE_MMIO_RANDOM     0xFF08U

/* ForgeOS System Call Numbers (invoked via OUT Rd, 3 / FORGE_PORT_SYSCALL) */
typedef enum {
    FORGE_SYS_EXIT          = 0,  /* R1 = exit code; halts CPU cleanly */
    FORGE_SYS_PRINT_INT     = 1,  /* R1 = 16-bit integer to print */
    FORGE_SYS_PRINT_CHAR    = 2,  /* R1 = ASCII char to print */
    FORGE_SYS_PRINT_STR     = 3,  /* R1 = RAM address of null-terminated string */
    FORGE_SYS_READ_CHAR     = 4,  /* R0 <- next char from input queue (0 if empty) */
    FORGE_SYS_KEY_AVAILABLE = 5,  /* R0 <- 1 if input queue non-empty, else 0 */
    FORGE_SYS_ALLOC         = 6,  /* R1 = size in bytes; R0 <- heap addr (0 on OOM) */
    FORGE_SYS_FREE_ALL      = 7,  /* Resets dynamic heap break to 0x9000 */
    FORGE_SYS_DRAW_PIXEL    = 8,  /* R1 = x (0..63), R2 = y (0..31), R3 = color (0..15) */
    FORGE_SYS_CLEAR_SCREEN  = 9,  /* R1 = color (0..15); fills 64x32 framebuffer */
    FORGE_SYS_GET_PIXEL     = 10, /* R1 = x, R2 = y; R0 <- pixel color at (x, y) */
    FORGE_SYS_CYCLES        = 11, /* R0 <- low 16 bits of CPU cycle counter */
    FORGE_SYS_RANDOM        = 12  /* R0 <- next deterministic 16-bit pseudo-random number */
} forge_syscall_t;

/* ForgeOS Supervisor State */
typedef struct {
    uint16_t heap_start;
    uint16_t heap_brk;
    uint16_t heap_limit;
    uint32_t alloc_count;
    uint32_t syscall_count;
    uint16_t last_syscall;
    uint16_t exit_code;
    uint16_t rng_state;
    uint32_t fb_writes;
} forge_os_state_t;

/* Forward declaration */
struct forge_cpu_s;

void forge_os_init(forge_os_state_t *os);

/* Heap management */
uint16_t forge_os_alloc(struct forge_cpu_s *cpu, uint16_t size_bytes);
void forge_os_free_all(struct forge_cpu_s *cpu);

/* Framebuffer operations */
void forge_os_clear_screen(struct forge_cpu_s *cpu, uint8_t color);
void forge_os_draw_pixel(struct forge_cpu_s *cpu, uint16_t x, uint16_t y, uint8_t color);
uint8_t forge_os_get_pixel(const struct forge_cpu_s *cpu, uint16_t x, uint16_t y);

/* Syscall handler (triggered when writing syscall number to FORGE_PORT_SYSCALL) */
forge_error_t forge_os_handle_syscall(struct forge_cpu_s *cpu, uint16_t syscall_id);

/* Deterministic 16-bit PRNG step */
uint16_t forge_os_next_random(forge_os_state_t *os);

/* Host-managed multi-program batch execution */
typedef struct {
    const char *name;
    const uint8_t *image;
    size_t image_len;
    const char *stdin_text;
} forge_task_desc_t;

typedef struct {
    forge_error_t error;
    bool halted;
    uint16_t exit_code;
    uint64_t instructions_executed;
    uint64_t cycles;
    uint16_t heap_used_bytes;
    uint32_t syscall_count;
    char console_snapshot[256];
} forge_task_result_t;

forge_error_t forge_os_run_tasks(
    struct forge_cpu_s *cpu,
    const forge_task_desc_t *tasks,
    forge_task_result_t *results,
    size_t task_count
);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_OS_H */
