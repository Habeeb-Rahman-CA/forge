#include "forge_os.h"
#include "forge_cpu.h"
#include <string.h>

void forge_os_init(forge_os_state_t *os) {
    if (os == NULL) {
        return;
    }
    os->heap_start    = FORGE_DYNAMIC_HEAP_START;
    os->heap_brk      = FORGE_DYNAMIC_HEAP_START;
    os->heap_limit    = FORGE_DYNAMIC_HEAP_LIMIT;
    os->alloc_count   = 0U;
    os->syscall_count = 0U;
    os->last_syscall  = 0U;
    os->exit_code     = 0U;
    os->rng_state     = 0xACE1U;
    os->fb_writes     = 0U;
}

uint16_t forge_os_next_random(forge_os_state_t *os) {
    if (os == NULL) {
        return 0U;
    }
    uint16_t x = os->rng_state;
    if (x == 0U) {
        x = 0xACE1U;
    }
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    os->rng_state = x;
    return x;
}

uint16_t forge_os_alloc(struct forge_cpu_s *cpu, uint16_t size_bytes) {
    if (cpu == NULL || size_bytes == 0U) {
        return 0U;
    }
    /* Round up to 2-byte word alignment */
    uint32_t aligned = ((uint32_t)size_bytes + 1U) & ~1U;
    uint32_t new_brk = (uint32_t)cpu->os.heap_brk + aligned;
    if (new_brk > (uint32_t)cpu->os.heap_limit) {
        return 0U; /* Out of heap memory */
    }
    uint16_t ptr = cpu->os.heap_brk;
    memset(&cpu->mem.data[ptr], 0, (size_t)aligned);
    cpu->os.heap_brk = (uint16_t)new_brk;
    cpu->os.alloc_count++;
    return ptr;
}

void forge_os_free_all(struct forge_cpu_s *cpu) {
    if (cpu == NULL) {
        return;
    }
    cpu->os.heap_brk    = cpu->os.heap_start;
    cpu->os.alloc_count = 0U;
}

void forge_os_clear_screen(struct forge_cpu_s *cpu, uint8_t color) {
    if (cpu == NULL) {
        return;
    }
    memset(&cpu->mem.data[FORGE_VRAM_START], (int)(color & 0x0FU), FORGE_FB_SIZE);
    cpu->os.fb_writes++;
}

void forge_os_draw_pixel(struct forge_cpu_s *cpu, uint16_t x, uint16_t y, uint8_t color) {
    if (cpu == NULL || x >= FORGE_FB_WIDTH || y >= FORGE_FB_HEIGHT) {
        return;
    }
    uint16_t offset = (uint16_t)(y * FORGE_FB_WIDTH + x);
    cpu->mem.data[FORGE_VRAM_START + offset] = (uint8_t)(color & 0x0FU);
    cpu->mem.last_write_addr = (uint32_t)(FORGE_VRAM_START + offset);
    cpu->mem.has_write = true;
    cpu->os.fb_writes++;
}

uint8_t forge_os_get_pixel(const struct forge_cpu_s *cpu, uint16_t x, uint16_t y) {
    if (cpu == NULL || x >= FORGE_FB_WIDTH || y >= FORGE_FB_HEIGHT) {
        return 0U;
    }
    uint16_t offset = (uint16_t)(y * FORGE_FB_WIDTH + x);
    return (uint8_t)(cpu->mem.data[FORGE_VRAM_START + offset] & 0x0FU);
}

forge_error_t forge_os_handle_syscall(struct forge_cpu_s *cpu, uint16_t syscall_id) {
    if (cpu == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }

    cpu->os.syscall_count++;
    cpu->os.last_syscall = syscall_id;

    uint16_t arg0 = cpu->r[FORGE_R1];
    uint16_t arg1 = cpu->r[FORGE_R2];
    uint16_t arg2 = cpu->r[FORGE_R3];

    switch ((forge_syscall_t)syscall_id) {
        case FORGE_SYS_EXIT:
            cpu->os.exit_code = arg0;
            cpu->r[FORGE_R0]  = arg0;
            cpu->halted       = true;
            return FORGE_OK;

        case FORGE_SYS_PRINT_INT:
            return forge_io_write_port(&cpu->io, FORGE_PORT_CONSOLE_INT, arg0);

        case FORGE_SYS_PRINT_CHAR:
            return forge_io_write_port(&cpu->io, FORGE_PORT_CONSOLE_CHAR, arg0);

        case FORGE_SYS_PRINT_STR: {
            uint16_t addr = arg0;
            size_t max_len = 1024U;
            for (size_t i = 0U; i < max_len; i++) {
                uint8_t ch = cpu->mem.data[addr];
                if (ch == 0U) {
                    break;
                }
                forge_io_write_port(&cpu->io, FORGE_PORT_CONSOLE_CHAR, (uint16_t)ch);
                if (addr == 0xFFFFU) {
                    break;
                }
                addr++;
            }
            return FORGE_OK;
        }

        case FORGE_SYS_READ_CHAR: {
            uint16_t ch = 0U;
            forge_error_t err = forge_io_read_port(&cpu->io, FORGE_PORT_CONSOLE_CHAR, &ch);
            cpu->r[FORGE_R0] = ch;
            return err;
        }

        case FORGE_SYS_KEY_AVAILABLE: {
            uint16_t avail = 0U;
            forge_error_t err = forge_io_read_port(&cpu->io, FORGE_PORT_KEYBOARD_STATUS, &avail);
            cpu->r[FORGE_R0] = avail;
            return err;
        }

        case FORGE_SYS_ALLOC:
            cpu->r[FORGE_R0] = forge_os_alloc(cpu, arg0);
            return FORGE_OK;

        case FORGE_SYS_FREE_ALL:
            forge_os_free_all(cpu);
            cpu->r[FORGE_R0] = 0U;
            return FORGE_OK;

        case FORGE_SYS_DRAW_PIXEL:
            forge_os_draw_pixel(cpu, arg0, arg1, (uint8_t)(arg2 & 0xFFU));
            return FORGE_OK;

        case FORGE_SYS_CLEAR_SCREEN:
            forge_os_clear_screen(cpu, (uint8_t)(arg0 & 0xFFU));
            return FORGE_OK;

        case FORGE_SYS_GET_PIXEL:
            cpu->r[FORGE_R0] = (uint16_t)forge_os_get_pixel(cpu, arg0, arg1);
            return FORGE_OK;

        case FORGE_SYS_CYCLES:
            cpu->r[FORGE_R0] = (uint16_t)(cpu->cycles & 0xFFFFU);
            return FORGE_OK;

        case FORGE_SYS_RANDOM:
            cpu->r[FORGE_R0] = forge_os_next_random(&cpu->os);
            return FORGE_OK;

        default:
            return FORGE_ERR_INVALID_IO_PORT;
    }
}

forge_error_t forge_os_run_tasks(
    struct forge_cpu_s *cpu,
    const forge_task_desc_t *tasks,
    forge_task_result_t *results,
    size_t task_count
) {
    if (cpu == NULL || (tasks == NULL && task_count > 0U) || (results == NULL && task_count > 0U)) {
        return FORGE_ERR_NULL_POINTER;
    }
    for (size_t i = 0U; i < task_count; i++) {
        memset(&results[i], 0, sizeof(results[i]));
        forge_cpu_reset(cpu, false);
        forge_error_t load_err = forge_cpu_load_binary(
            cpu,
            tasks[i].image,
            tasks[i].image_len,
            FORGE_DEFAULT_PC
        );
        if (load_err != FORGE_OK) {
            results[i].error = load_err;
            return load_err;
        }
        if (tasks[i].stdin_text != NULL) {
            forge_io_push_input_str(&cpu->io, tasks[i].stdin_text);
        }
        forge_error_t run_err = forge_cpu_run(cpu);
        results[i].error                 = run_err;
        results[i].halted                = cpu->halted;
        results[i].exit_code             = cpu->os.exit_code;
        results[i].instructions_executed = cpu->instructions_executed;
        results[i].cycles                = cpu->cycles;
        results[i].heap_used_bytes       = (uint16_t)(cpu->os.heap_brk - cpu->os.heap_start);
        results[i].syscall_count         = cpu->os.syscall_count;

        size_t copy_len = cpu->io.console_len;
        if (copy_len >= sizeof(results[i].console_snapshot)) {
            copy_len = sizeof(results[i].console_snapshot) - 1U;
        }
        memcpy(results[i].console_snapshot, cpu->io.console_buf, copy_len);
        results[i].console_snapshot[copy_len] = '\0';
    }
    return FORGE_OK;
}

