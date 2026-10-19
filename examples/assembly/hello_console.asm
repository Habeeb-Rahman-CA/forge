; Example 4: String Output to Console Port 0 using .asciz and Indexed LOAD
.equ PORT_CHAR, 0

main:
    MOV R1, message     ; R1 = pointer to null-terminated string

print_loop:
    LOAD R0, [R1]       ; Load 16-bit word at [R1]; low byte is current ASCII char
    AND  R0, 0x00FF     ; Isolate low byte
    JZ   done           ; If '\0', finish
    OUT  R0, PORT_CHAR  ; Emit character to console port 0
    ADD  R1, 1          ; Advance byte pointer
    JMP  print_loop

done:
    HALT

message:
    .asciz "Hello, Forge-16!\n"
    .byte 0             ; Extra padding byte so last char word read at [R1] has valid high byte

