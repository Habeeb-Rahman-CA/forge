#include "forge_stack.h"

forge_error_t forge_stack_push16(forge_memory_t *mem, uint16_t *sp, uint16_t val) {
    if (mem == NULL || sp == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    /* Full-descending stack in 0xE000 .. 0xF000: SP must be in [0xE002, 0xF000] before decrement */
    if (*sp < (uint16_t)(FORGE_STACK_BOTTOM + 2U) || *sp > FORGE_STACK_TOP) {
        return FORGE_ERR_STACK_OVERFLOW;
    }
    uint16_t new_sp = (uint16_t)(*sp - 2U);
    forge_error_t err = forge_mem_write16(mem, new_sp, val);
    if (err != FORGE_OK) {
        return err;
    }
    *sp = new_sp;
    return FORGE_OK;
}

forge_error_t forge_stack_pop16(forge_memory_t *mem, uint16_t *sp, uint16_t *out_val) {
    if (mem == NULL || sp == NULL || out_val == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    /* To pop 2 bytes, SP must be in [0xE000, 0xEFFE] */
    if (*sp >= FORGE_STACK_TOP) {
        return FORGE_ERR_STACK_UNDERFLOW;
    }
    if (*sp < FORGE_STACK_BOTTOM || *sp > (uint16_t)(FORGE_STACK_TOP - 2U)) {
        return FORGE_ERR_STACK_OVERFLOW;
    }
    forge_error_t err = forge_mem_read16(mem, *sp, out_val);
    if (err != FORGE_OK) {
        return err;
    }
    *sp = (uint16_t)(*sp + 2U);
    return FORGE_OK;
}

forge_error_t forge_stack_peek16(const forge_memory_t *mem, uint16_t sp, uint16_t *out_val) {
    if (mem == NULL || out_val == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (sp >= FORGE_STACK_TOP) {
        return FORGE_ERR_STACK_UNDERFLOW;
    }
    if (sp < FORGE_STACK_BOTTOM || sp > (uint16_t)(FORGE_STACK_TOP - 2U)) {
        return FORGE_ERR_STACK_OVERFLOW;
    }
    return forge_mem_peek16(mem, sp, out_val);
}

