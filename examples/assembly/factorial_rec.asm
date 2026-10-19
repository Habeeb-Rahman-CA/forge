; Example 3: Recursive Factorial (computes 5! = 120 using CALL, RET, PUSH, POP)
.entry main

main:
    MOV R1, 5           ; n = 5
    CALL factorial      ; R0 = factorial(5)
    OUT R0              ; Output 120
    HALT

; Subroutine: factorial(R1) -> R0
factorial:
    CMP R1, 1
    JLE base_case
    PUSH R1             ; Save current n on stack
    SUB R1, 1           ; n - 1
    CALL factorial      ; R0 = factorial(n - 1)
    POP R1              ; Restore n
    MUL R0, R1          ; R0 = R0 * n
    RET

base_case:
    MOV R0, 1
    RET

