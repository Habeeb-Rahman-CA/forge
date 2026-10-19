; Project Forge — Population Count (Bit Counting) in Forge-16 Assembly
; Counts the number of set bits (1s) in a 16-bit word.
; Example test word: 0xBEEF (binary: 1011 1110 1110 1111 -> 13 set bits)
.entry main

main:
    MOV R0, 0xBEEF      ; test value
    MOV R1, 0           ; bit count accumulator
    MOV R2, 16          ; 16 bits to check

count_loop:
    CMP R2, 0
    JZ  done

    ; Test lowest bit: R3 = R0 & 1
    MOV R3, R0
    AND R3, 1
    ADD R1, R3          ; count += lowest_bit

    ; Shift right by 1 bit to inspect next bit
    SHR R0, 1
    SUB R2, 1           ; bits_remaining--
    JMP count_loop

done:
    ; Output the count of set bits (expected: 13)
    OUT R1
    HALT

