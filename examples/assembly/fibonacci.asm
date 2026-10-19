; Example 2: Iterative Fibonacci Sequence (first 8 numbers: 0, 1, 1, 2, 3, 5, 8, 13)
.equ COUNT, 8

main:
    MOV R0, 0           ; a = 0
    MOV R1, 1           ; b = 1
    MOV R2, COUNT       ; remaining terms

loop:
    CMP R2, 0
    JZ  done
    OUT R0              ; emit current Fibonacci number
    MOV R3, R0          ; temp = a
    ADD R3, R1          ; temp = a + b
    MOV R0, R1          ; a = b
    MOV R1, R3          ; b = temp
    SUB R2, 1           ; remaining--
    JMP loop

done:
    HALT

