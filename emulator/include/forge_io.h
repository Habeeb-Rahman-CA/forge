#ifndef FORGE_IO_H
#define FORGE_IO_H

#include "forge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FORGE_IO_PORT_COUNT      256U
#define FORGE_CONSOLE_BUF_SIZE   4096U
#define FORGE_IO_INT_LOG_MAX     512U
#define FORGE_IO_INPUT_QUEUE_MAX 512U

/* Standard I/O Port Numbers */
#define FORGE_PORT_CONSOLE_CHAR    0U /* Write/Read ASCII character */
#define FORGE_PORT_CONSOLE_INT     1U /* Write 16-bit integer (default OUT port) */
#define FORGE_PORT_KEYBOARD_STATUS 2U /* Read: 1 if input queued, 0 if empty */
#define FORGE_PORT_SYSCALL         3U /* System call trigger port */

typedef struct {
    uint16_t ports[FORGE_IO_PORT_COUNT];

    /* Console text output buffer */
    char console_buf[FORGE_CONSOLE_BUF_SIZE];
    size_t console_len;

    /* Numeric output log (records values sent to FORGE_PORT_CONSOLE_INT) */
    uint16_t int_output_log[FORGE_IO_INT_LOG_MAX];
    size_t int_output_count;

    /* Input queue (for IN on FORGE_PORT_CONSOLE_CHAR / FORGE_PORT_CONSOLE_INT) */
    uint16_t input_queue[FORGE_IO_INPUT_QUEUE_MAX];
    size_t input_head;
    size_t input_tail;
    size_t input_count;
} forge_io_t;

void forge_io_init(forge_io_t *io);

/* Read 16-bit value from an I/O port */
forge_error_t forge_io_read_port(forge_io_t *io, uint16_t port, uint16_t *out_val);

/* Write 16-bit value to an I/O port */
forge_error_t forge_io_write_port(forge_io_t *io, uint16_t port, uint16_t val);

/* Enqueue a 16-bit value (or ASCII char) for subsequent IN instructions */
bool forge_io_push_input(forge_io_t *io, uint16_t val);

/* Enqueue a null-terminated string into the input queue */
size_t forge_io_push_input_str(forge_io_t *io, const char *str);

#ifdef __cplusplus
}
#endif

#endif /* FORGE_IO_H */

