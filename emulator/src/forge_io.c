#include "forge_io.h"
#include <stdio.h>
#include <string.h>

void forge_io_init(forge_io_t *io) {
    if (io == NULL) {
        return;
    }
    memset(io->ports, 0, sizeof(io->ports));
    memset(io->console_buf, 0, sizeof(io->console_buf));
    io->console_len = 0U;
    memset(io->int_output_log, 0, sizeof(io->int_output_log));
    io->int_output_count = 0U;
    memset(io->input_queue, 0, sizeof(io->input_queue));
    io->input_head = 0U;
    io->input_tail = 0U;
    io->input_count = 0U;
}

static void append_console_char(forge_io_t *io, char c) {
    if (io->console_len + 1U < FORGE_CONSOLE_BUF_SIZE) {
        io->console_buf[io->console_len] = c;
        io->console_len++;
        io->console_buf[io->console_len] = '\0';
    }
}

forge_error_t forge_io_read_port(forge_io_t *io, uint16_t port, uint16_t *out_val) {
    if (io == NULL || out_val == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (port >= FORGE_IO_PORT_COUNT) {
        return FORGE_ERR_INVALID_IO_PORT;
    }

    if (port == FORGE_PORT_KEYBOARD_STATUS) {
        *out_val = (io->input_count > 0U) ? 1U : 0U;
        return FORGE_OK;
    }

    if ((port == FORGE_PORT_CONSOLE_CHAR || port == FORGE_PORT_CONSOLE_INT) && io->input_count > 0U) {
        uint16_t val = io->input_queue[io->input_head];
        io->input_head = (io->input_head + 1U) % FORGE_IO_INPUT_QUEUE_MAX;
        io->input_count--;
        io->ports[port] = val;
        *out_val = val;
        return FORGE_OK;
    }

    *out_val = io->ports[port];
    return FORGE_OK;
}

forge_error_t forge_io_write_port(forge_io_t *io, uint16_t port, uint16_t val) {
    if (io == NULL) {
        return FORGE_ERR_NULL_POINTER;
    }
    if (port >= FORGE_IO_PORT_COUNT) {
        return FORGE_ERR_INVALID_IO_PORT;
    }

    io->ports[port] = val;

    if (port == FORGE_PORT_CONSOLE_CHAR) {
        append_console_char(io, (char)(val & 0xFFU));
    } else if (port == FORGE_PORT_CONSOLE_INT) {
        if (io->int_output_count < FORGE_IO_INT_LOG_MAX) {
            io->int_output_log[io->int_output_count] = val;
            io->int_output_count++;
        }
        char num_str[16];
        int written = snprintf(num_str, sizeof(num_str), "%u\n", (unsigned int)val);
        for (int i = 0; i < written && num_str[i] != '\0'; i++) {
            append_console_char(io, num_str[i]);
        }
    }

    return FORGE_OK;
}

bool forge_io_push_input(forge_io_t *io, uint16_t val) {
    if (io == NULL || io->input_count >= FORGE_IO_INPUT_QUEUE_MAX) {
        return false;
    }
    io->input_queue[io->input_tail] = val;
    io->input_tail = (io->input_tail + 1U) % FORGE_IO_INPUT_QUEUE_MAX;
    io->input_count++;
    return true;
}

size_t forge_io_push_input_str(forge_io_t *io, const char *str) {
    if (io == NULL || str == NULL) {
        return 0U;
    }
    size_t pushed = 0U;
    for (size_t i = 0U; str[i] != '\0'; i++) {
        if (!forge_io_push_input(io, (uint16_t)(uint8_t)str[i])) {
            break;
        }
        pushed++;
    }
    return pushed;
}

