; ForgeOS System Call & Framebuffer Assembly Demo (os_syscall.asm)
; Demonstrates:
;   - SYS_ALLOC (6): Dynamic heap allocation at 0x9000+
;   - SYS_CLEAR_SCREEN (9) and SYS_DRAW_PIXEL (8): Framebuffer graphics
;   - SYS_PRINT_STR (3) and SYS_PRINT_INT (1): Console output
;   - SYS_EXIT (0): Clean OS termination with exit code

.equ SYS_EXIT,         0
.equ SYS_PRINT_INT,    1
.equ SYS_PRINT_CHAR,   2
.equ SYS_PRINT_STR,    3
.equ SYS_ALLOC,        6
.equ SYS_DRAW_PIXEL,   8
.equ SYS_CLEAR_SCREEN, 9

.entry main

main:
    ; 1. Clear framebuffer to color 0 (Black)
    MOV R1, 0
    MOV R0, SYS_CLEAR_SCREEN
    OUT R0, 3

    ; 2. Allocate 8 bytes on the dynamic heap -> R0 holds pointer (0x9000)
    MOV R1, 8
    MOV R0, SYS_ALLOC
    OUT R0, 3
    MOV R4, R0          ; Save heap pointer in R4

    ; 3. Store ASCII "OS!\0" into allocated heap block and print via SYS_PRINT_STR
    MOV R5, 0x534F      ; 'O' (0x4F), 'S' (0x53) in little-endian
    STORE R5, [R4]
    MOV R5, 0x0021      ; '!' (0x21), '\0' (0x00)
    STORE R5, [R4 + 2]

    MOV R1, R4
    MOV R0, SYS_PRINT_STR
    OUT R0, 3
    MOV R1, 10          ; '\n'
    MOV R0, SYS_PRINT_CHAR
    OUT R0, 3

    ; 4. Draw a diagonal line of 16 bright cyan pixels (color 11)
    MOV R6, 0
draw_loop:
    MOV R1, R6          ; x = R6
    MOV R2, R6          ; y = R6
    MOV R3, 11          ; color = 11 (Bright Cyan)
    MOV R0, SYS_DRAW_PIXEL
    OUT R0, 3
    ADD R6, 1
    CMP R6, 16
    JL draw_loop

    ; 5. Exit cleanly with status code 42
    MOV R1, 42
    MOV R0, SYS_EXIT
    OUT R0, 3

