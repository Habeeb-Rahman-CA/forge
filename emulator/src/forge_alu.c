#include "forge_alu.h"

static uint16_t compute_zn_flags(uint16_t res) {
    uint16_t flags = 0U;
    if (res == 0U) {
        flags |= FORGE_FLAG_Z;
    }
    if ((res & 0x8000U) != 0U) {
        flags |= FORGE_FLAG_N;
    }
    return flags;
}

forge_alu_result_t forge_alu_add(uint16_t a, uint16_t b) {
    forge_alu_result_t out;
    uint32_t sum32 = (uint32_t)a + (uint32_t)b;
    uint16_t res16 = (uint16_t)(sum32 & 0xFFFFU);

    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    out.error  = FORGE_OK;

    if (sum32 > 0xFFFFU) {
        out.flags |= FORGE_FLAG_C;
    }
    /* Signed overflow: operands have same sign bit, result has opposite sign bit */
    if (((~(a ^ b)) & (a ^ res16) & 0x8000U) != 0U) {
        out.flags |= FORGE_FLAG_V;
    }
    return out;
}

forge_alu_result_t forge_alu_sub(uint16_t a, uint16_t b) {
    forge_alu_result_t out;
    uint32_t diff32 = (uint32_t)a - (uint32_t)b;
    uint16_t res16  = (uint16_t)(diff32 & 0xFFFFU);

    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    out.error  = FORGE_OK;

    /* Carry flag represents unsigned borrow (a < b) */
    if (a < b) {
        out.flags |= FORGE_FLAG_C;
    }
    /* Signed overflow: operands have different signs, result sign differs from 'a' */
    if (((a ^ b) & (a ^ res16) & 0x8000U) != 0U) {
        out.flags |= FORGE_FLAG_V;
    }
    return out;
}

forge_alu_result_t forge_alu_cmp(uint16_t a, uint16_t b) {
    return forge_alu_sub(a, b);
}

forge_alu_result_t forge_alu_mul(uint16_t a, uint16_t b) {
    forge_alu_result_t out;
    uint32_t prod_u32 = (uint32_t)a * (uint32_t)b;
    int32_t  sa       = (int32_t)(int16_t)a;
    int32_t  sb       = (int32_t)(int16_t)b;
    int32_t  prod_s32 = sa * sb;
    uint16_t res16    = (uint16_t)(prod_u32 & 0xFFFFU);

    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    out.error  = FORGE_OK;

    if ((prod_u32 >> 16) != 0U) {
        out.flags |= FORGE_FLAG_C;
    }
    if (prod_s32 < -32768 || prod_s32 > 32767) {
        out.flags |= FORGE_FLAG_V;
    }
    return out;
}

forge_alu_result_t forge_alu_div(uint16_t a, uint16_t b) {
    forge_alu_result_t out;
    if (b == 0U) {
        out.result = 0U;
        out.flags  = 0U;
        out.error  = FORGE_ERR_DIVISION_BY_ZERO;
        return out;
    }
    uint16_t res16 = (uint16_t)(a / b);
    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    out.error  = FORGE_OK;
    return out;
}

forge_alu_result_t forge_alu_and(uint16_t a, uint16_t b) {
    forge_alu_result_t out;
    uint16_t res16 = (uint16_t)(a & b);
    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    out.error  = FORGE_OK;
    return out;
}

forge_alu_result_t forge_alu_or(uint16_t a, uint16_t b) {
    forge_alu_result_t out;
    uint16_t res16 = (uint16_t)(a | b);
    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    out.error  = FORGE_OK;
    return out;
}

forge_alu_result_t forge_alu_xor(uint16_t a, uint16_t b) {
    forge_alu_result_t out;
    uint16_t res16 = (uint16_t)(a ^ b);
    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    out.error  = FORGE_OK;
    return out;
}

forge_alu_result_t forge_alu_not(uint16_t a) {
    forge_alu_result_t out;
    uint16_t res16 = (uint16_t)(~a);
    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    out.error  = FORGE_OK;
    return out;
}

forge_alu_result_t forge_alu_shl(uint16_t a, uint16_t shift) {
    forge_alu_result_t out;
    uint8_t cnt = (uint8_t)(shift & 0x1FU);
    uint16_t res16 = 0U;
    uint16_t carry = 0U;

    if (cnt == 0U) {
        res16 = a;
        carry = 0U;
    } else if (cnt < 16U) {
        carry = (uint16_t)(((uint32_t)a >> (16U - cnt)) & 1U);
        res16 = (uint16_t)(((uint32_t)a << cnt) & 0xFFFFU);
    } else if (cnt == 16U) {
        carry = (uint16_t)(a & 1U);
        res16 = 0U;
    } else {
        carry = 0U;
        res16 = 0U;
    }

    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    if (carry != 0U) {
        out.flags |= FORGE_FLAG_C;
    }
    out.error = FORGE_OK;
    return out;
}

forge_alu_result_t forge_alu_shr(uint16_t a, uint16_t shift) {
    forge_alu_result_t out;
    uint8_t cnt = (uint8_t)(shift & 0x1FU);
    uint16_t res16 = 0U;
    uint16_t carry = 0U;

    if (cnt == 0U) {
        res16 = a;
        carry = 0U;
    } else if (cnt < 16U) {
        carry = (uint16_t)(((uint32_t)a >> (cnt - 1U)) & 1U);
        res16 = (uint16_t)((uint32_t)a >> cnt);
    } else if (cnt == 16U) {
        carry = (uint16_t)((a >> 15U) & 1U);
        res16 = 0U;
    } else {
        carry = 0U;
        res16 = 0U;
    }

    out.result = res16;
    out.flags  = compute_zn_flags(res16);
    if (carry != 0U) {
        out.flags |= FORGE_FLAG_C;
    }
    out.error = FORGE_OK;
    return out;
}

