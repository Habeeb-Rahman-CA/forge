import { ExampleProgram } from './forge.models';

export interface TutorialLesson {
  id: string;
  title: string;
  category: 'architecture' | 'memory' | 'stack' | 'compiler' | 'os';
  badge: string;
  summary: string;
  theory: string;
  walkthrough: string[];
  sampleCode: string;
  language: 'assembly' | 'forgelang';
}

export interface GuidedExercise {
  id: string;
  title: string;
  difficulty: 'Beginner' | 'Intermediate' | 'Advanced';
  category: string;
  objective: string;
  instructions: string[];
  hints: string[];
  starterCode: string;
  solutionCode: string;
  language: 'assembly' | 'forgelang';
  expectedOutputSummary: string;
}

export const FORGE_TUTORIALS: TutorialLesson[] = [
  {
    id: 'tut_arch_alu',
    title: 'Lesson 1: CPU Architecture, Registers & ALU',
    category: 'architecture',
    badge: 'Hardware Core',
    summary: 'Master the 16-bit register file (R0–R7), status flags (Z, N, C, V), and the Arithmetic Logic Unit.',
    theory: `The Forge-16 is a 16-bit educational RISC-inspired processor.
It features eight 16-bit general-purpose registers (R0 through R7), a 16-bit Program Counter (PC), a Stack Pointer (SP), and a Status Flags register.

Status Flags:
- Z (Zero Flag): Set to 1 when an ALU operation result is 0.
- N (Negative Flag): Set to 1 when the most significant bit (bit 15) is 1 (representing negative two's complement).
- C (Carry Flag): Set on unsigned overflow in addition or borrow in subtraction.
- V (Overflow Flag): Set on signed two's complement overflow.

Every arithmetic and logic instruction (ADD, SUB, MUL, DIV, AND, OR, XOR, NOT, SHL, SHR, CMP) executes deterministically, updating the destination register and flags.`,
    walkthrough: [
      'Inspect the CPU panel to see registers R0–R7, PC, SP, and flags Z, N, C, V.',
      'Step through the instructions one at a time using "↷ Step".',
      'Observe R0 changing from 10 to 15 after the ADD instruction.',
      'Notice that the Zero flag (Z) stays 0 because the result is non-zero.',
      'Check the Console panel to see the final value (15) printed to Port 1.'
    ],
    sampleCode: `; Lesson 1: Basic Register Operations & ALU Flags
; Demonstrates MOV, ADD, and Console Output
MOV R0, 10          ; Load immediate decimal 10 into R0
MOV R1, 5           ; Load immediate decimal 5 into R1
ADD R0, R1          ; R0 = R0 + R1 (15)
OUT R0              ; Emit result (15) to Console Port 1
HALT                ; Terminate execution
`,
    language: 'assembly'
  },
  {
    id: 'tut_memory_endian',
    title: 'Lesson 2: 64 KB Memory & Little-Endian Addressing',
    category: 'memory',
    badge: 'Memory Subsystem',
    summary: 'Understand the 16-bit address space (0x0000–0xFFFF), word alignment, and little-endian byte ordering.',
    theory: `Forge-16 has a 64 KB byte-addressable flat memory space.
Registers are 16 bits (2 bytes). When storing a 16-bit word to memory with STORE:
- Low byte (bits 7..0) is stored at address A.
- High byte (bits 15..8) is stored at address A + 1.
This is the Little-Endian convention, identical to x86 and modern ARM architectures.

Memory Organization:
- 0x0000–0x7FFF (32 KB): Code and read-only text constants
- 0x8000–0x8FFF (4 KB): Static user data and global tables
- 0x9000–0xDFFF (20 KB): Dynamic ForgeOS Heap space
- 0xE000–0xEFFF (4 KB): Downward-growing Call Stack (SP starts at 0xEFFE)
- 0xF000–0xF7FF (2 KB): Framebuffer VRAM (64×32 display)
- 0xFF00–0xFF0F (16 B): Hardware MMIO registers`,
    walkthrough: [
      'Load the program and examine the Memory panel starting at address 0x8000.',
      'Step through the STORE instructions.',
      'Observe the memory cells highlighting in green as words are written.',
      'Verify that 0x000A is stored as [0A, 00] in the hex viewer due to little-endian ordering.',
      'Watch how LOAD retrieves the full 16-bit word back into register R2.'
    ],
    sampleCode: `; Lesson 2: Memory Access and Endianness
.equ DATA_ADDR, 0x8000

main:
    MOV R0, DATA_ADDR
    MOV R1, 0x1234      ; 16-bit word
    STORE R1, [R0]      ; Store word: [0x8000]=0x34, [0x8001]=0x12

    ADD R0, 2
    MOV R1, 0xABCD
    STORE R1, [R0]      ; Store next word at 0x8002

    ; Read back from 0x8000
    MOV R0, DATA_ADDR
    LOAD R2, [R0]       ; R2 = 0x1234
    OUT R2
    HALT
`,
    language: 'assembly'
  },
  {
    id: 'tut_call_stack',
    title: 'Lesson 3: Call Stack, Subroutines & Recursion',
    category: 'stack',
    badge: 'Stack & Control Flow',
    summary: 'Trace stack frames, return addresses, PUSH/POP mechanics, and recursive function execution.',
    theory: `The stack allows programs to make subroutine calls (CALL / RET) and save state across function invocations.
In Forge-16:
- The Stack Pointer (SP) points to the top of the stack and starts at 0xEFFE.
- PUSH decrements SP by 2, then writes the 16-bit word to [SP].
- POP reads the 16-bit word from [SP], then increments SP by 2.
- CALL pushes the Return Address (PC + 4) onto the stack and jumps to the target label.
- RET pops the return address off the stack and sets PC to it.

Activation frames use Frame Pointer FP (R7) to establish clean local variable scopes that survive recursion.`,
    walkthrough: [
      'Switch to the "Variables & Call Stack" tab at the bottom.',
      'Step into the factorial function using "↷ Step".',
      'Watch SP decrement by 4 on each recursive CALL (saving return address and N).',
      'Notice the Call Depth increasing up to 5.',
      'Observe the base case triggered when N <= 1, followed by unwinding with POP and MUL.'
    ],
    sampleCode: `; Lesson 3: Recursive Factorial (5! = 120)
.entry main

main:
    MOV R1, 5           ; Calculate 5!
    CALL factorial
    OUT R0              ; Outputs 120
    HALT

factorial:
    CMP R1, 1
    JLE base_case
    PUSH R1             ; Save n
    SUB R1, 1           ; n - 1
    CALL factorial      ; R0 = factorial(n - 1)
    POP R1              ; Restore n
    MUL R0, R1          ; R0 = R0 * n
    RET

base_case:
    MOV R0, 1
    RET
`,
    language: 'assembly'
  },
  {
    id: 'tut_forgelang',
    title: 'Lesson 4: ForgeLang High-Level Language & Compiler',
    category: 'compiler',
    badge: 'Compiler Pipeline',
    summary: 'Discover how high-level syntax maps through the lexer, parser, AST, and code generator.',
    theory: `Forge includes a custom high-level programming language, ForgeLang (.fl).
The ForgeLang compiler (forge-cc) implements a complete compiler pipeline in C:
1. Lexer: Converts source text into a stream of tokens.
2. Recursive-Descent Parser: Builds an Abstract Syntax Tree (AST).
3. Symbol Table & Semantic Analysis: Enforces variable declaration, scope, and function signatures.
4. Code Generator: Emits optimized Forge-16 assembly with activation frames ([FP - 2], [FP + 4]).
5. Bi-directional Source Maps: Maps every generated assembly instruction back to its original high-level line.

This enables source-level debugging: stepping statement by statement through high-level code!`,
    walkthrough: [
      'Click "Compile .fl" to compile the ForgeLang source into assembly.',
      'Click the "Generated ASM" and "AST" tabs in the center panel to inspect compiler outputs.',
      'Use "⇣ Step Line" to advance line by line through high-level statements.',
      'Watch the active line highlight in the Monaco Editor.',
      'Inspect the "Variables & Call Stack" tab to observe local variables and parameters updating in real time.'
    ],
    sampleCode: `// Lesson 4: High-Level Functions & Loops
fn fibonacci(n) {
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
    let val = fibonacci(idx)
    print val
}
`,
    language: 'forgelang'
  },
  {
    id: 'tut_forge_os',
    title: 'Lesson 5: ForgeOS Mini Operating System & Graphics',
    category: 'os',
    badge: 'OS & Hardware',
    summary: 'Interact with supervisor system calls, dynamic heap allocation (sbrk), and the 64×32 virtual framebuffer.',
    theory: `ForgeOS is a host-managed mini operating environment for the Forge-16 CPU.
Features:
- Supervisor System Calls via Port 3: OUT Rd, 3 triggers OS traps:
  - SYS_EXIT (0), SYS_PRINT_CHAR (1), SYS_PRINT_INT (2), SYS_READ_CHAR (3),
  - SYS_SBRK / ALLOC (4), SYS_TIME / CYCLES (5), SYS_RANDOM (6), SYS_DRAW_PIXEL (7),
  - SYS_CLEAR_SCREEN (8).
- Dynamic Heap Allocator: Manages memory from 0x9000 to 0xDFFF with bounds protection.
- 64×32 Virtual Framebuffer: Mapped to 0xF000–0xF7FF with a 16-color retro palette.
- Hardware MMIO Registers: Real-time cycle counters, pseudo-random generator, and keyboard input queue.`,
    walkthrough: [
      'Switch to the "Display & ForgeOS" tab at the bottom.',
      'Click "Run" or step through the program.',
      'Watch the 64×32 framebuffer render a colorful animated bar chart in real time.',
      'Observe the Heap Break (sbrk) advance as dynamic memory is allocated.',
      'Note the total number of system calls and framebuffer write operations logged.'
    ],
    sampleCode: `// Lesson 5: ForgeOS Dynamic Heap & 64x32 Framebuffer
clear_screen(0) // Clear display to black

// Allocate 8 words (16 bytes) on the dynamic heap
let scores = alloc(16)
poke(scores + 0, 6)
poke(scores + 2, 14)
poke(scores + 4, 22)
poke(scores + 6, 28)

// Draw horizontal baseline
for (let x = 4; x < 60; x = x + 1) {
    draw_pixel(x, 30, 15) // White line
}

// Render colored bars from heap data
for (let i = 0; i < 4; i = i + 1) {
    let h = peek(scores + i * 2)
    let color = 10 + i // Bright Green, Cyan, Red, Magenta
    for (let bx = 8 + i * 13; bx < 8 + i * 13 + 8; bx = bx + 1) {
        for (let by = 30 - h; by < 30; by = by + 1) {
            draw_pixel(bx, by, color)
        }
    }
}

print 100 // Done
exit(0)
`,
    language: 'forgelang'
  }
];

export const FORGE_EXERCISES: GuidedExercise[] = [
  {
    id: 'ex_popcount',
    title: 'Exercise 1: Bit Manipulation — Population Count',
    difficulty: 'Beginner',
    category: 'ALU & Bitwise Operations',
    objective: 'Calculate the number of set 1-bits in a 16-bit binary word using bitwise AND and SHR instructions.',
    instructions: [
      'Initialize R0 with the input value (e.g. 0xBEEF).',
      'Set R1 to 0 to serve as the 1-bit accumulator.',
      'Set R2 to 16 to count down the 16 bits of the word.',
      'In each iteration, isolate the lowest bit using "AND R3, 1", add it to R1, and shift R0 right by 1 ("SHR R0, 1").',
      'Repeat until all 16 bits have been checked, then emit the result with "OUT R1".'
    ],
    hints: [
      'Use "AND R3, 1" to check if the least-significant bit is 1.',
      'Use "SHR R0, 1" to move the next bit into the least-significant position.',
      'For 0xBEEF (1011 1110 1110 1111 binary), the correct population count is 13.'
    ],
    starterCode: `; Exercise 1: Population Count (Bit Counting)
; Count how many 1s are in R0 and output the result.
.entry main

main:
    MOV R0, 0xBEEF      ; Input test value
    MOV R1, 0           ; TODO: Accumulator for set bits
    MOV R2, 16          ; TODO: Bit counter (16 bits)

count_loop:
    ; TODO: Complete loop logic here
    HALT
`,
    solutionCode: `; Exercise 1 Solution: Population Count
.entry main

main:
    MOV R0, 0xBEEF      ; Input value (13 set bits)
    MOV R1, 0           ; Bit count accumulator
    MOV R2, 16          ; 16 bits to inspect

count_loop:
    CMP R2, 0
    JZ  done
    MOV R3, R0
    AND R3, 1           ; Test lowest bit
    ADD R1, R3          ; Add 1 if bit was set
    SHR R0, 1           ; Shift right by 1
    SUB R2, 1           ; bits_left--
    JMP count_loop

done:
    OUT R1              ; Outputs 13
    HALT
`,
    language: 'assembly',
    expectedOutputSummary: 'Console Port 1 should output 13 for test value 0xBEEF.'
  },
  {
    id: 'ex_array_max',
    title: 'Exercise 2: Memory Algorithms — Array Sum & Maximum',
    difficulty: 'Intermediate',
    category: 'Memory & Pointers',
    objective: 'Iterate through an array of 6 integers at RAM address 0x8000, find the maximum value, and compute the sum.',
    instructions: [
      'The array contains 6 words stored sequentially at 0x8000..0x800B.',
      'Use R4 as a memory pointer, starting at 0x8000 and incrementing by 2 on each step.',
      'Maintain the current maximum in R1 and running sum in R2.',
      'Compare each element against R1 using CMP; if the new element is greater, update R1.',
      'Output the sum (R2) and the maximum (R1) at the end of execution.'
    ],
    hints: [
      'Remember words in Forge-16 are 2 bytes wide, so use "ADD R4, 2" to advance to the next word.',
      'Use "LOAD R3, [R4]" to read from RAM into register R3.',
      'For array [15, 42, 8, 99, 23, 56], the sum is 243 and the max is 99.'
    ],
    starterCode: `; Exercise 2: Array Sum and Maximum
.entry main
.equ ARRAY_ADDR, 0x8000
.equ ARRAY_LEN, 6

main:
    ; Initialize test array in RAM: [15, 42, 8, 99, 23, 56]
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

    ; TODO: Implement iteration to find Sum and Max
    HALT
`,
    solutionCode: `; Exercise 2 Solution: Array Sum and Maximum
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
    MOV R4, ARRAY_ADDR  ; Pointer
    LOAD R1, [R4]       ; Max = first element (15)
    MOV R2, 0           ; Sum = 0
    MOV R0, 0           ; Loop counter

loop:
    CMP R0, ARRAY_LEN
    JGE done
    LOAD R3, [R4]       ; Current element
    ADD R2, R3          ; Sum += element
    CMP R3, R1
    JLE not_greater
    MOV R1, R3          ; Max = element

not_greater:
    ADD R4, 2
    ADD R0, 1
    JMP loop

done:
    OUT R2              ; Outputs Sum: 243
    OUT R1              ; Outputs Max: 99
    HALT
`,
    language: 'assembly',
    expectedOutputSummary: 'Outputs Sum (243) followed by Max (99).'
  },
  {
    id: 'ex_palindrome',
    title: 'Exercise 3: String Processing — Palindrome Checker',
    difficulty: 'Intermediate',
    category: 'Strings & Conditionals',
    objective: 'Determine whether a null-terminated ASCII string stored in memory reads the same forward and backward.',
    instructions: [
      'Initialize left pointer R1 and right pointer R2 to the start of the string.',
      'Seek R2 to the end of the string by checking for the null terminator (0x00).',
      'Step R2 back by 1 so it points to the last character.',
      'In a loop, compare characters at [R1] and [R2]. If they mismatch, output 0 and halt.',
      'Advance R1 (+1) and decrement R2 (-1). When R1 >= R2, all characters matched: output 1 and halt.'
    ],
    hints: [
      'Use "AND R3, 0x00FF" to isolate individual ASCII bytes.',
      'Use "JNZ mismatch" to detect when characters do not equal each other.',
      '"racecar" is a palindrome (outputs 1).'
    ],
    starterCode: `; Exercise 3: Palindrome String Checker
.entry main

main:
    MOV R1, text        ; Left pointer
    ; TODO: Find end of string, compare characters from outside in
    HALT

text:
    .asciz "racecar"
    .byte 0, 0
`,
    solutionCode: `; Exercise 3 Solution: Palindrome String Checker
.entry main

main:
    MOV R1, text        ; Left pointer
    MOV R2, text        ; Right pointer

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
    JGE is_palindrome
    LOAD R3, [R1]
    AND R3, 0x00FF
    LOAD R4, [R2]
    AND R4, 0x00FF
    CMP R3, R4
    JNZ not_palindrome
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
`,
    language: 'assembly',
    expectedOutputSummary: 'Outputs 1 for palindrome strings, or 0 for non-palindromes.'
  },
  {
    id: 'ex_framebuffer_pattern',
    title: 'Exercise 4: Graphics — Framebuffer Geometric Patterns',
    difficulty: 'Advanced',
    category: 'ForgeOS Graphics',
    objective: 'In ForgeLang, program the 64×32 virtual framebuffer to render a framed bordered pattern with a centered cross.',
    instructions: [
      'Clear the framebuffer to black with clear_screen(0).',
      'Draw an outer border using color 14 (Yellow) around the entire 64×32 screen.',
      'Draw a horizontal line across the middle (y = 16) using color 11 (Bright Cyan).',
      'Draw a vertical line down the center (x = 32) using color 12 (Bright Red).',
      'Print 1 when rendering is complete.'
    ],
    hints: [
      'Screen coordinates are 0 <= x < 64 and 0 <= y < 32.',
      'Use nested for-loops or separate linear for-loops with draw_pixel(x, y, color).',
      'Switch to the "Display & ForgeOS" tab in the bottom panel to admire your graphics!'
    ],
    starterCode: `// Exercise 4: Framebuffer Geometric Patterns
clear_screen(0)

// TODO: Draw border around (0..63, 0..31) in color 14

// TODO: Draw horizontal line at y = 16 in color 11

// TODO: Draw vertical line at x = 32 in color 12

print 1
exit(0)
`,
    solutionCode: `// Exercise 4 Solution: Framebuffer Geometric Patterns
clear_screen(0)

// 1. Outer border in Yellow (14)
for (let x = 0; x < 64; x = x + 1) {
    draw_pixel(x, 0, 14)
    draw_pixel(x, 31, 14)
}
for (let y = 0; y < 32; y = y + 1) {
    draw_pixel(0, y, 14)
    draw_pixel(63, y, 14)
}

// 2. Horizontal center line in Bright Cyan (11)
for (let cx = 1; cx < 63; cx = cx + 1) {
    draw_pixel(cx, 16, 11)
}

// 3. Vertical center line in Bright Red (12)
for (let cy = 1; cy < 31; cy = cy + 1) {
    draw_pixel(32, cy, 12)
}

print 1
exit(0)
`,
    language: 'forgelang',
    expectedOutputSummary: 'A 64x32 framed canvas with a centered bright crosshair rendered on the virtual display.'
  }
];

