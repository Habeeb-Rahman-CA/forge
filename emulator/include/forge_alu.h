#ifndef FORGE_ALU_H
#define FORGE_ALU_H

#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t result;
    uint16_t flags;
    forge_error_t error;
} forge_alu_result_t;

forge_alu_result_t forge_alu_add(uint16_t a, uint16_t b);
forge_alu_result_t forge_alu_sub(uint16_t a, uint16_t b);
forge_alu_result_t forge_alu_mul(uint16_t a, uint16_t b);
forge_alu_result_t forge_alu_div(uint16_t a, uint16_t b);
forge_alu_result_t forge_alu_and(uint16_t a, uint16_t b);
forge_alu_result_t forge_alu_or(uint16_t a, uint16_t b);
forge_alu_result_t forge_alu_xor(uint16_t a, uint16_t b);
forge_alu_result_t forge_alu_not(uint16_t a);
forge_alu_result_t forge_alu_shl(uint16_t a, uint16_t shift);
forge_alu_result_t forge_alu_shr(uint16_t a, uint16_t shift);
forge_alu_result_t forge_alu_cmp(uint16_t a, uint16_t b);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_ALU_H */

