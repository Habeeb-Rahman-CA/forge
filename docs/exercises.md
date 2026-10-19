# Forge-16 Guided Exercises & Problem Sets

This workbook presents guided programming challenges for learners exploring computer architecture, assembly language, and compiler systems with **Forge-16**.

---

## Exercise 1: Population Count (Bit Counting)

- **Difficulty**: Beginner
- **Category**: ALU & Bitwise Operations
- **Language**: Forge-16 Assembly (`.asm`)

### Problem Statement
Write a Forge-16 assembly program to count the number of set bits (1s) in a 16-bit word stored in `R0`. Output the final count to Console Port 1.

### Example
For input `R0 = 0xBEEF` (binary `1011 1110 1110 1111`), the output should be `13`.

### Hints
1. Use `AND R3, 1` to isolate the least-significant bit of the current value.
2. Add the isolated bit directly to an accumulator register (`R1`).
3. Use `SHR R0, 1` to shift the word right by one bit on each iteration.
4. Decrement a loop counter (`R2 = 16`) until all 16 bits have been checked.

### Starter Template
```assembly
; Exercise 1: Population Count Starter
.entry main

main:
    MOV R0, 0xBEEF      ; Input test value
    MOV R1, 0           ; TODO: Set bit accumulator
    MOV R2, 16          ; TODO: Bit counter

count_loop:
    ; TODO: Implement loop
    HALT
```

### Complete Solution
```assembly
; Exercise 1 Solution: Population Count
.entry main

main:
    MOV R0, 0xBEEF      ; 13 set bits
    MOV R1, 0           ; Bit count accumulator
    MOV R2, 16          ; 16 bits to inspect

count_loop:
    CMP R2, 0
    JZ  done
    MOV R3, R0
    AND R3, 1           ; Test lowest bit
    ADD R1, R3          ; Add 1 if bit was set
    SHR R0, 1           ; Shift right by 1
    SUB R2, 1           ; bits_remaining--
    JMP count_loop

done:
    OUT R1              ; Outputs 13
    HALT
```

---

## Exercise 2: Array Sum and Maximum Value

- **Difficulty**: Intermediate
- **Category**: Memory Addressing & Pointers
- **Language**: Forge-16 Assembly (`.asm`)

### Problem Statement
An array of 6 integers is stored sequentially at RAM address `0x8000`. Write a program to:
1. Traverse the array in memory.
2. Compute the sum of all elements.
3. Find the maximum element.
4. Output the sum followed by the maximum value.

### Test Data
Array at `0x8000`: `[15, 42, 8, 99, 23, 56]`
- Expected Sum: `243`
- Expected Max: `99`

### Hints
1. Words in Forge-16 are 2 bytes wide. Increment your pointer register by 2 (`ADD R4, 2`) after each `LOAD`.
2. Initialize your maximum accumulator with the first array element, not zero, to support negative values.
3. Use `CMP R3, R1` and `JLE not_greater` to conditionally update the maximum.

### Starter Template
```assembly
; Exercise 2 Starter: Array Sum and Maximum
.entry main
.equ ARRAY_ADDR, 0x8000
.equ ARRAY_LEN, 6

main:
    ; Initialize test array in RAM
    MOV R0, ARRAY_ADDR
    MOV R1, 15
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 42
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 8
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 99
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 23
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 56
    STORE R1, [R0]

    ; TODO: Implement traversal to compute Sum and Max
    HALT
```

### Complete Solution
```assembly
; Exercise 2 Solution: Array Sum and Maximum
.entry main
.equ ARRAY_ADDR, 0x8000
.equ ARRAY_LEN, 6

main:
    ; Initialize array
    MOV R0, ARRAY_ADDR
    MOV R1, 15
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 42
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 8
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 99
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 23
    STORE R1, [R0]
    ADD R0, 2
    MOV R1, 56
    STORE R1, [R0]

    ; Algorithm
    MOV R4, ARRAY_ADDR  ; Pointer to current element
    LOAD R1, [R4]       ; Max = first element (15)
    MOV R2, 0           ; Sum = 0
    MOV R0, 0           ; Loop index

loop:
    CMP R0, ARRAY_LEN
    JGE done
    LOAD R3, [R4]       ; Read element
    ADD R2, R3          ; Sum += element
    CMP R3, R1
    JLE not_greater
    MOV R1, R3          ; Max = element

not_greater:
    ADD R4, 2           ; Next word
    ADD R0, 1           ; index++
    JMP loop

done:
    OUT R2              ; Outputs 243
    OUT R1              ; Outputs 99
    HALT
```

---

## Exercise 3: Palindrome String Checker

- **Difficulty**: Intermediate
- **Category**: Strings, Conditionals & Pointers
- **Language**: Forge-16 Assembly (`.asm`)

### Problem Statement
Given a null-terminated ASCII string stored in memory, check if it reads the same forward and backward. Output `1` if it is a palindrome, or `0` if not.

### Test Data
- `"racecar"` -> `1` (True)
- `"forge16"` -> `0` (False)

### Hints
1. Find the end of the string by loading bytes until encountering the null terminator `0x00`.
2. Back the right pointer up by 1 byte so it points to the final valid ASCII character.
3. Compare characters with `CMP R3, R4` and `JNZ not_palindrome`.
4. Advance the left pointer (`ADD R1, 1`) and decrement the right pointer (`SUB R2, 1`).

### Complete Solution
```assembly
; Exercise 3 Solution: Palindrome String Checker
.entry main

main:
    MOV R1, text        ; Left pointer
    MOV R2, text        ; Right pointer (will seek end)

find_end:
    LOAD R3, [R2]
    AND R3, 0x00FF
    JZ found_end
    ADD R2, 1
    JMP find_end

found_end:
    SUB R2, 1           ; Point to last char

check_loop:
    CMP R1, R2
    JGE is_palindrome   ; Left pointer crossed right: match!
    LOAD R3, [R1]
    AND R3, 0x00FF
    LOAD R4, [R2]
    AND R4, 0x00FF
    CMP R3, R4
    JNZ not_palindrome  ; Mismatch detected
    ADD R1, 1
    SUB R2, 1
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
```

---

## Exercise 4: Framebuffer Geometric Patterns

- **Difficulty**: Advanced
- **Category**: High-Level Graphics & ForgeOS
- **Language**: ForgeLang (`.fl`)

### Problem Statement
Using ForgeLang, write a program that initializes the 64×32 virtual framebuffer, draws a yellow outer border, and paints a centered crosshair in red and cyan.

### Coordinate Specifications
- Screen size: `64` columns (X: `0`..`63`) × `32` rows (Y: `0`..`31`).
- Top/Bottom borders: `y = 0` and `y = 31` (color 14: Yellow).
- Left/Right borders: `x = 0` and `x = 63` (color 14: Yellow).
- Horizontal crosshair: `y = 16` across `1 <= x < 63` (color 11: Bright Cyan).
- Vertical crosshair: `x = 32` across `1 <= y < 31` (color 12: Bright Red).

### Complete Solution
```forgelang
// Exercise 4 Solution: Framebuffer Geometric Patterns
clear_screen(0)

// 1. Draw outer border in Yellow (color 14)
for (let x = 0; x < 64; x = x + 1) {
    draw_pixel(x, 0, 14)
    draw_pixel(x, 31, 14)
}
for (let y = 0; y < 32; y = y + 1) {
    draw_pixel(0, y, 14)
    draw_pixel(63, y, 14)
}

// 2. Horizontal center line in Bright Cyan (color 11)
for (let cx = 1; cx < 63; cx = cx + 1) {
    draw_pixel(cx, 16, 11)
}

// 3. Vertical center line in Bright Red (color 12)
for (let cy = 1; cy < 31; cy = cy + 1) {
    draw_pixel(32, cy, 12)
}

print 1
exit(0)
```

