#include "forge_types.h"

const char *forge_error_str(forge_error_t err) {
    switch (err) {
        case FORGE_OK:
            return "OK";
        case FORGE_ERR_INVALID_INSTRUCTION:
            return "Invalid instruction or addressing mode";
        case FORGE_ERR_MEMORY_OUT_OF_BOUNDS:
            return "Memory access out of bounds or protected";
        case FORGE_ERR_DIVISION_BY_ZERO:
            return "Division by zero";
        case FORGE_ERR_STACK_OVERFLOW:
            return "Stack overflow (SP below 0xE000)";
        case FORGE_ERR_STACK_UNDERFLOW:
            return "Stack underflow (SP at or above 0xF000)";
        case FORGE_ERR_EXECUTION_LIMIT:
            return "Execution step limit exceeded";
        case FORGE_ERR_INVALID_IO_PORT:
            return "Invalid I/O port address";
        case FORGE_ERR_LOAD_FAILURE:
            return "Program load failure";
        case FORGE_ERR_NULL_POINTER:
            return "Null pointer argument";
        case FORGE_ERR_BREAKPOINT:
            return "Breakpoint hit";
        default:
            return "Unknown error";
    }
}

const char *forge_reg_name(uint8_t reg) {
    static const char *const names[FORGE_NUM_REGS] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7"
    };
    if (reg >= FORGE_NUM_REGS) {
        return "R?";
    }
    return names[reg];
}

