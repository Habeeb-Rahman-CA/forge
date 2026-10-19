# Forge-16 Assembly Language Reference

## 1. Overview

Forge-16 assembly files (`.asm`) are human-readable text files assembled by `forge-asm` into `.fbin` executable binaries (or raw machine-code images) for the Forge-16 CPU emulator (`forge-emu`).

Each line in an assembly source file has the general structure:

```asm
[label:] [mnemonic_or_directive [operands...]] [; comment]
```

- **Case Sensitivity**: Instruction mnemonics (`MOV`, `mov`), register names (`R0`–`R7`, `r0`–`r7`, `FP`, `fp`), and directives (`.word`, `.WORD`) are case-insensitive. Symbol and label names are case-sensitive.
- **Comments**: Start with `;` (or `//`) and extend to the end of the line.

---

## 2. Registers

| Register Name | Encoding | Description |
| :--- | :--- | :--- |
| `R0`–`R6` | `0`–`6` | General-purpose 16-bit registers |
| `R7` / `FP` | `7` | General-purpose register 7 / Frame Pointer alias |

---

## 3. Literals and Symbols

- **Decimal**: `0`, `42`, `-10`, `65535` (signed `-32768`..`32767` or unsigned `0`..`65535`, encoded as 16-bit two's complement).
- **Hexadecimal**: Prefixed with `0x` or `0X`, e.g., `0x002A`, `0xFF00`, `-0x08`.
- **Binary**: Prefixed with `0b` or `0B`, e.g., `0b10101010`.
- **Character**: Single ASCII character in single quotes, e.g., `'A'` (`65`), `'\n'` (`10`), `'\t'` (`9`), `'\0'` (`0`).
- **Symbols**: Labels or `.equ` constants can be used anywhere an immediate value or direct memory address is expected, including forward references to labels defined later in the file.

---

## 4. Addressing Modes & Instruction Syntax

### 4.1 Zero-Operand Instructions
```asm
NOP
HALT
RET
```

### 4.2 Single-Operand Instructions
```asm
NOT R0              ; Bitwise invert R0
POP R1              ; Pop top of stack into R1
PUSH R0             ; Push register R0 onto stack (MODE = REG)
PUSH 0x1234         ; Push immediate 0x1234 onto stack (MODE = IMM)
```

### 4.3 Control-Flow Instructions (`JMP`, `JZ`, `JNZ`, `CALL`, `JC`, `JNC`, `JN`, `JL`, `JLE`, `JG`, `JGE`)
```asm
JMP loop_start      ; Unconditional jump to label (MODE = IMM)
JZ  done_label      ; Jump if Zero flag (Z=1) to label (MODE = IMM)
CALL compute_sum    ; Call subroutine at label (MODE = IMM)
JMP R3              ; Indirect jump to address in R3 (MODE = REG)
CALL R4             ; Indirect call to address in R4 (MODE = REG)
```

### 4.4 Two-Operand Register / Immediate Instructions (`MOV`, `ADD`, `SUB`, `MUL`, `DIV`, `AND`, `OR`, `XOR`, `SHL`, `SHR`, `CMP`)
```asm
MOV R0, 10          ; Immediate mode (MODE = IMM)
MOV R1, R0          ; Register mode  (MODE = REG)
ADD R0, R1          ; R0 = R0 + R1   (MODE = REG)
SUB R0, 0x0005      ; R0 = R0 - 5    (MODE = IMM)
CMP R0, MAX_COUNT   ; Compare R0 with constant MAX_COUNT
```

### 4.5 Memory Access Instructions (`LOAD`, `STORE`)
```asm
LOAD  R0, [0x8000]      ; Direct memory mode (MODE = MEM_DIR): R0 <- Mem16[0x8000]
LOAD  R0, [my_var]      ; Direct memory mode with label:       R0 <- Mem16[my_var]
STORE R0, [0x8000]      ; Direct memory mode:                  Mem16[0x8000] <- R0

LOAD  R1, [R2]          ; Indexed mode (offset 0):             R1 <- Mem16[R2]
LOAD  R1, [R7 + 4]      ; Indexed mode (positive offset):      R1 <- Mem16[R7 + 4]
LOAD  R1, [FP - 2]      ; Indexed mode (negative offset):      R1 <- Mem16[R7 - 2]
STORE R1, [R7 - 4]      ; Indexed mode store:                  Mem16[R7 - 4] <- R1
```

### 4.6 Input/Output Instructions (`IN`, `OUT`)
```asm
OUT R0              ; Shorthand for OUT R0, 1 (emit R0 as 16-bit integer to port 1)
OUT R0, 0           ; Emit low byte of R0 as ASCII char to port 0 (FORGE_PORT_CONSOLE_CHAR)
OUT R0, R2          ; Emit R0 to dynamic port number in R2 (MODE = REG)

IN  R0              ; Shorthand for IN R0, 1 (read from port 1)
IN  R0, 2           ; Read keyboard status from port 2
IN  R0, R2          ; Read from dynamic port number in R2
```

---

## 5. Assembler Directives

| Directive | Syntax | Description |
| :--- | :--- | :--- |
| `.org` | `.org 0x0400` | Sets the origin/current assembly address. |
| `.entry` | `.entry main` | Sets the initial `PC` entry address in the `.fbin` header. |
| `.equ` / `.const` | `.equ LIMIT, 100` | Defines a named constant in the symbol table. |
| `.word` / `.dw` | `.word 10, 0x20, label` | Emits one or more 16-bit little-endian words. |
| `.byte` / `.db` | `.byte 0x48, 'i', 0` | Emits one or more 8-bit bytes. |
| `.asciz` / `.string` | `.asciz "Hello\n"` | Emits a null-terminated ASCII string. |
| `.space` / `.resb` | `.space 16` | Reserves `N` zero-initialized bytes. |
