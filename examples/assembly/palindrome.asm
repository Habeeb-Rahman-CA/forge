; Project Forge — Palindrome String Checker in Forge-16 Assembly
; Checks if the ASCII string stored at 'text' is a palindrome.
; Outputs 1 if palindrome, 0 if not.
.entry main

main:
    ; Step 1: Find the length and end pointer of the string
    MOV R1, text        ; left pointer (starts at beginning)
    MOV R2, text        ; right pointer (will seek to end)

find_end:
    LOAD R3, [R2]
    AND R3, 0x00FF      ; isolate byte
    JZ found_end
    ADD R2, 1
    JMP find_end

found_end:
    SUB R2, 1           ; point to the last character (before null terminator)

check_loop:
    CMP R1, R2
    JGE is_palindrome   ; left >= right, all characters matched

    LOAD R3, [R1]
    AND R3, 0x00FF      ; left char

    LOAD R4, [R2]
    AND R4, 0x00FF      ; right char

    CMP R3, R4
    JNZ not_palindrome  ; mismatch!

    ADD R1, 1           ; left++
    SUB R2, 1           ; right--
    JMP check_loop

is_palindrome:
    MOV R0, 1
    OUT R0
    HALT

not_palindrome:
    MOV R0, 0
    OUT R0
    HALT

text:
    .asciz "racecar"
    .byte 0, 0
