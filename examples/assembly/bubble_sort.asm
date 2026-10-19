; Project Forge — Bubble Sort in Forge-16 Assembly
; Sorts an array of 8 integers stored at RAM address 0x8000 in ascending order.
.entry main
.equ ARRAY_ADDR, 0x8000
.equ ARRAY_LEN, 8

main:
    ; Initialize array in memory: [42, 17, 88, 3, 25, 99, 12, 64]
    MOV R0, ARRAY_ADDR
    MOV R1, 42
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 17
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 88
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 3
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 25
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 99
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 12
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 64
    STORE R1, [R0]

    ; Outer loop: R2 = outer counter (i = 0 to ARRAY_LEN - 1)
    MOV R2, 0

outer_loop:
    CMP R2, ARRAY_LEN
    JGE print_results
    MOV R3, 0           ; R3 = inner counter (j = 0)

inner_loop:
    MOV R4, ARRAY_LEN
    SUB R4, 1
    SUB R4, R2          ; limit = ARRAY_LEN - 1 - i
    CMP R3, R4
    JGE next_outer

    ; Calculate memory address of array[j] -> R5 = ARRAY_ADDR + j*2
    MOV R5, R3
    MUL R5, 2
    ADD R5, ARRAY_ADDR

    LOAD R6, [R5]       ; R6 = array[j]
    ADD R5, 2
    LOAD R7, [R5]       ; R7 = array[j+1]
    SUB R5, 2           ; restore R5 to array[j]

    CMP R6, R7
    JLE no_swap

    ; Swap array[j] and array[j+1]
    STORE R7, [R5]
    ADD R5, 2
    STORE R6, [R5]

no_swap:
    ADD R3, 1           ; j++
    JMP inner_loop

next_outer:
    ADD R2, 1           ; i++
    JMP outer_loop

print_results:
    ; Output sorted array to Console Port 1
    MOV R0, ARRAY_ADDR
    MOV R1, 0           ; index

print_loop:
    CMP R1, ARRAY_LEN
    JGE finished
    LOAD R2, [R0]
    OUT R2              ; emit number
    ADD R0, 2
    ADD R1, 1
    JMP print_loop

finished:
    HALT

