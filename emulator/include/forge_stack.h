#ifndef FORGE_STACK_H
#define FORGE_STACK_H

#include "forge_memory.h"
#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Push a 16-bit value onto the full-descending stack (0xE000..0xEFFF) */
forge_error_t forge_stack_push16(forge_memory_t *mem, uint16_t *sp, uint16_t val);

/* Pop a 16-bit value from the full-descending stack (0xE000..0xEFFF) */
forge_error_t forge_stack_pop16(forge_memory_t *mem, uint16_t *sp, uint16_t *out_val);

/* Peek at the 16-bit value on top of the stack without modifying SP */
forge_error_t forge_stack_peek16(const forge_memory_t *mem, uint16_t sp, uint16_t *out_val);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_STACK_H */

