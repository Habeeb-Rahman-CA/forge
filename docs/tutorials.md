# Forge-16 Educational Computer Systems Tutorials

This curriculum provides a structured, hands-on path from raw processor hardware to high-level language compilation and virtual operating system concepts using the **Forge-16** platform.

---

## Curriculum Overview

1. **Lesson 1: CPU Architecture, Registers, & the ALU** — Register files, two's complement arithmetic, flags (`Z`, `N`, `C`, `V`), and deterministic instruction execution.
2. **Lesson 2: Memory Hierarchy, Little-Endian Storage, & Pointers** — 64 KB address space, word alignment, little-endian byte ordering, and RAM tables.
3. **Lesson 3: The Call Stack, Subroutines, & Recursion** — Stack Pointer (`SP`), activation frames, `PUSH`/`POP`, `CALL`/`RET`, and Frame Pointer (`FP`).
4. **Lesson 4: High-Level Language Compilation & Control Flow** — Lexical analysis, recursive-descent parsing, AST construction, code generation, and bi-directional source mapping.
5. **Lesson 5: Mini Operating System, Hardware Devices, & Graphics** — Supervisor system calls (`OUT Rd, 3`), dynamic heap allocation (`sbrk`), MMIO registers, and 64×32 virtual framebuffer graphics.

---

## Lesson 1: CPU Architecture, Registers, & the ALU

### Objective
Understand the execution model of a 16-bit processor, observe how general-purpose registers (`R0`–`R7`) store state, and trace status flag transitions in the Arithmetic Logic Unit.

### Theoretical Foundation
The **Forge-16** CPU uses a 16-bit word size and a 32-bit fixed instruction encoding.
State is maintained in:
- Eight 16-bit General Purpose Registers: `R0` through `R7` (`R7` also acts as Frame Pointer `FP`).
- 16-bit Program Counter (`PC`): Address of the currently executing instruction.
- 16-bit Stack Pointer (`SP`): Starts at `0xEFFE` and grows downward.
- Status Flags:
  - **Z (Zero)**: Set to 1 if ALU result equals `0x0000`.
  - **N (Negative)**: Set to 1 if the most significant bit (bit 15) is 1.
  - **C (Carry / Borrow)**: Set on unsigned addition overflow or subtraction borrow.
  - **V (Overflow)**: Set on signed two's complement overflow.

### Guided Walkthrough
Step through the following assembly program in the Visual Debugger:

```assembly
; Lesson 1: Basic Register Operations & Flags
MOV R0, 10          ; Load immediate decimal 10 into R0
MOV R1, 5           ; Load immediate decimal 5 into R1
ADD R0, R1          ; R0 = R0 + R1 (15, Z=0, N=0, C=0, V=0)
OUT R0              ; Emit result (15) to Console Port 1
SUB R0, 15          ; R0 = 15 - 15 = 0 (Z=1, N=0, C=0, V=0)
HALT
```

1. Click **Assemble**. The debugger loads the program at `PC=0x0400`.
2. Click **↷ Step**. Notice `R0` changes to `0x000A` (10).
3. Click **↷ Step**. `R1` changes to `0x0005` (5).
4. Click **↷ Step**. Observe the **ALU Operation Breakdown**:
   - `Operand A`: `R0` = `0x000A` (10)
   - `Operand B`: `R1` = `0x0005` (5)
   - `Result`: `0x000F` (15)
   - `Flags`: `Z=0`, `N=0`, `C=0`, `V=0`.
5. Step past `SUB R0, 15`. The Zero Flag (`Z`) lights up green in the CPU panel!

---

## Lesson 2: Memory Hierarchy, Little-Endian Storage, & Pointers

### Objective
Learn how data is addressed and stored in physical RAM, examine little-endian byte representation, and build an in-memory data table.

### Theoretical Foundation
The Forge-16 memory layout spans `0x0000` to `0xFFFF` (64 KB):
- `0x0000`–`0x7FFF` (32 KB): Code Segment (program binaries and read-only text constants).
- `0x8000`–`0x8FFF` (4 KB): Static User RAM & Global Tables.
- `0x9000`–`0xDFFF` (20 KB): Dynamic ForgeOS Heap.
- `0xE000`–`0xEFFF` (4 KB): Call Stack.
- `0xF000`–`0xF7FF` (2 KB): Video Framebuffer.
- `0xFF00`–`0xFF0F` (16 B): Hardware MMIO Registers.

#### Little-Endian Word Encoding
When storing 16-bit word `0x1234` at address `0x8000`:
- `Mem[0x8000] = 0x34` (Least Significant Byte).
- `Mem[0x8001] = 0x12` (Most Significant Byte).

### Guided Walkthrough

```assembly
; Lesson 2: Little-Endian Word Storage & RAM Tables
.equ TABLE_ADDR, 0x8000

main:
    MOV R0, TABLE_ADDR
    MOV R1, 0x1234
    STORE R1, [R0]      ; Stores 0x34 at 0x8000, 0x12 at 0x8001

    ADD R0, 2           ; Move to next word boundary
    MOV R1, 0xCAFE
    STORE R1, [R0]      ; Stores 0xFE at 0x8002, 0xCA at 0x8003

    ; Read back word from 0x8000
    MOV R4, TABLE_ADDR
    LOAD R2, [R4]       ; R2 = 0x1234
    OUT R2
    HALT
```

1. Enter `0x8000` in the Memory Inspector address box.
2. Step through `STORE R1, [R0]`.
3. Observe the Hex viewer at `0x8000`: the bytes read `34 12`, highlighting green to indicate a live write.
4. Step through `LOAD R2, [R4]`. Memory cell `0x8000` flashes blue to indicate a live read, and `R2` receives `0x1234`.

---

## Lesson 3: The Call Stack, Subroutines, & Recursion

### Objective
Understand the stack frame layout, observe return address tracking, and debug recursive execution using activation frames.

### Theoretical Foundation
Subroutines use the stack to save local variables and return addresses:
- `CALL target`: Pushes return address (`PC + 4`) onto `[SP - 2]`, decrements `SP` by 2, and jumps to `target`.
- `RET`: Pops 16-bit return address from `[SP]` into `PC`, and increments `SP` by 2.
- `PUSH Rs` / `POP Rd`: Saves and restores register state across calls.

### Recursive Factorial Example

```assembly
; Recursive Factorial (5! = 120)
.entry main

main:
    MOV R1, 5
    CALL factorial      ; R0 = factorial(5)
    OUT R0              ; Outputs 120
    HALT

factorial:
    CMP R1, 1
    JLE base_case
    PUSH R1             ; Push n onto stack
    SUB R1, 1           ; n = n - 1
    CALL factorial      ; R0 = factorial(n - 1)
    POP R1              ; Restore n
    MUL R0, R1          ; R0 = R0 * n
    RET

base_case:
    MOV R0, 1
    RET
```

1. Switch to the **Variables & Call Stack** tab at the bottom.
2. Step through each recursive call. Observe:
   - `SP` decrements by 4 bytes on each level (2 for return address, 2 for saved `R1`).
   - Call depth counter increases from 1 to 5.
3. When `R1 <= 1`, the recursion reaches the base case (`R0 = 1`).
4. The stack unwinds: each `RET` restores `PC`, and `MUL R0, R1` multiplies `1 * 2 * 3 * 4 * 5 = 120`.

---

## Lesson 4: High-Level Language Compilation & Control Flow

### Objective
Examine how high-level ForgeLang code transforms through lexing, parsing, semantic analysis, and assembly code generation.

### Theoretical Foundation
The **ForgeLang** compiler (`forge-cc`) implements a multi-stage compilation pipeline:
1. **Lexer**: Tokenizes source text into keywords, identifiers, literals, and operators.
2. **Parser**: Generates a strongly-typed Abstract Syntax Tree (AST).
3. **Semantic Analyzer**: Enforces scope rules, symbol declarations, and argument counts.
4. **Code Generator**: Generates standard Forge-16 assembly code using Frame Pointer (`FP`) activation records.
5. **Source Mapper**: Embeds `@line <n>` metadata enabling statement-level stepping in the visual debugger.

### Guided Walkthrough

```forgelang
// Lesson 4: High-Level Iterative Fibonacci Function
fn fib(n) {
    if n <= 0 {
        return 0
    }
    let prev = 0
    let curr = 1
    for (let i = 1; i < n; i = i + 1) {
        let next = prev + curr
        prev = curr
        curr = next
    }
    return curr
}

for (let idx = 0; idx < 6; idx = idx + 1) {
    print fib(idx)
}
```

1. Select **ForgeLang (.fl)** mode in the top toolbar.
2. Click **Compile .fl**.
3. Click the **AST** tab to view the hierarchical parse tree.
4. Click the **Generated ASM** tab to see how local variables map to `[FP - 2]`, `[FP - 4]`, and function parameters map to `[FP + 4]`.
5. Use **⇣ Step Line** to step line by line through the high-level algorithm. The active line in Monaco Editor highlights in synchrony!

---

## Lesson 5: Mini Operating System, Hardware Devices, & Graphics

### Objective
Interact with virtual hardware peripherals (timers, pseudo-random generator, keyboard queue), supervisor system calls, dynamic heap allocation, and the 64×32 pixel virtual display.

### Theoretical Foundation
Forge provides a host-managed supervisor execution environment:
- **System Calls (`OUT Rd, 3`)**:
  - `FORGE_SYS_EXIT` (0)
  - `FORGE_SYS_PRINT_CHAR` (1)
  - `FORGE_SYS_PRINT_INT` (2)
  - `FORGE_SYS_READ_CHAR` (3)
  - `FORGE_SYS_SBRK` (4): Dynamic memory allocation
  - `FORGE_SYS_CYCLES` (5): Hardware clock cycle counter
  - `FORGE_SYS_RANDOM` (6): Pseudo-random number generator
  - `FORGE_SYS_DRAW_PIXEL` (7): Framebuffer pixel write
  - `FORGE_SYS_CLEAR_SCREEN` (8): Framebuffer clear
- **Memory-Mapped I/O (MMIO)**: `0xFF00`–`0xFF08`.
- **Framebuffer VRAM**: `0xF000`–`0xF7FF` (64 columns × 32 rows, 2048 bytes).

### Guided Walkthrough

```forgelang
// Lesson 5: Dynamic Heap & 64x32 Framebuffer Bar Chart
clear_screen(0)

// Allocate 4 words on the ForgeOS heap
let data = alloc(8)
poke(data + 0, 8)
poke(data + 2, 16)
poke(data + 4, 24)
poke(data + 6, 20)

// Draw baseline
for (let bx = 4; bx < 60; bx = bx + 1) {
    draw_pixel(bx, 30, 15) // White line
}

// Render bars
for (let i = 0; i < 4; i = i + 1) {
    let h = peek(data + i * 2)
    let color = 10 + i
    for (let x = 8 + i * 13; x < 8 + i * 13 + 8; x = x + 1) {
        for (let y = 30 - h; y < 30; y = y + 1) {
            draw_pixel(x, y, color)
        }
    }
}

print 100
exit(0)
```

1. Click **Compile .fl**, switch to the **Display & ForgeOS** tab in the bottom panel.
2. Click **▶ Run**.
3. Watch the 64×32 virtual framebuffer screen render colorful bars!
4. Inspect the **ForgeOS State** cards to see `Heap Break (sbrk)` advance from `0x9000` to `0x9008`.

