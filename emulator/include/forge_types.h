#ifndef FORGE_TYPES_H
#define FORGE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Architectural Constants */
#define FORGE_NUM_REGS          8U
#define FORGE_MEM_SIZE          65536U
#define FORGE_INSTRUCTION_BYTES 4U

/* Memory Map Boundaries */
#define FORGE_SYSTEM_START      0x0000U
#define FORGE_SYSTEM_END        0x03FFU
#define FORGE_TEXT_START        0x0400U
#define FORGE_TEXT_END          0x7FFFU
#define FORGE_HEAP_START        0x8000U
#define FORGE_HEAP_END          0xDFFFU
#define FORGE_STACK_BOTTOM      0xE000U
#define FORGE_STACK_TOP         0xF000U
#define FORGE_VRAM_START        0xF000U
#define FORGE_VRAM_END          0xFEFFU
#define FORGE_MMIO_START        0xFF00U
#define FORGE_MMIO_END          0xFFFFU

/* Default Reset Register Values */
#define FORGE_DEFAULT_PC        FORGE_TEXT_START
#define FORGE_DEFAULT_SP        FORGE_STACK_TOP
#define FORGE_DEFAULT_MAX_STEPS 100000U

/* Register Indices */
#define FORGE_R0 0U
#define FORGE_R1 1U
#define FORGE_R2 2U
#define FORGE_R3 3U
#define FORGE_R4 4U
#define FORGE_R5 5U
#define FORGE_R6 6U
#define FORGE_R7 7U
#define FORGE_FP FORGE_R7

/* Status Flag Bitmasks */
#define FORGE_FLAG_Z (1U << 0) /* Zero Flag     (0x0001) */
#define FORGE_FLAG_N (1U << 1) /* Negative Flag (0x0002) */
#define FORGE_FLAG_C (1U << 2) /* Carry Flag    (0x0004) */
#define FORGE_FLAG_V (1U << 3) /* Overflow Flag (0x0008) */
#define FORGE_FLAG_MASK (FORGE_FLAG_Z | FORGE_FLAG_N | FORGE_FLAG_C | FORGE_FLAG_V)

/* Error and Status Codes */
typedef enum {
    FORGE_OK = 0,
    FORGE_ERR_INVALID_INSTRUCTION = 1,
    FORGE_ERR_MEMORY_OUT_OF_BOUNDS = 2,
    FORGE_ERR_DIVISION_BY_ZERO = 3,
    FORGE_ERR_STACK_OVERFLOW = 4,
    FORGE_ERR_STACK_UNDERFLOW = 5,
    FORGE_ERR_EXECUTION_LIMIT = 6,
    FORGE_ERR_INVALID_IO_PORT = 7,
    FORGE_ERR_LOAD_FAILURE = 8,
    FORGE_ERR_NULL_POINTER = 9,
    FORGE_ERR_BREAKPOINT = 10
} forge_error_t;

/* Convert error code to human-readable string */
const char *forge_error_str(forge_error_t err);

/* Convert register index (0..7) to name ("R0".."R7") */
const char *forge_reg_name(uint8_t reg);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_TYPES_H */

