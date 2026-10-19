export interface ForgeDisasmLine {
  address: number;
  line: number;
  sourceLine?: number;
  word0: number;
  word1: number;
  text: string;
}

export interface ForgeSourceMapEntry {
  sourceLine: number;
  asmLine: number;
  address: number;
}

export interface ForgeGlobalVar {
  name: string;
  address: number;
  value: number;
  signedValue: number;
  declLine: number;
}

export interface ForgeLocalVar {
  name: string;
  kind: 'param' | 'local';
  offset: number;
  address: number;
  value: number;
  signedValue: number;
}

export interface ForgeCallFrame {
  depth: number;
  function: string;
  calleeAddr: number;
  callerPc: number;
  callerSourceLine: number;
  returnAddr: number;
  frameFp: number;
  frameSp: number;
  locals: ForgeLocalVar[];
}

export interface ForgeSymbol {
  name: string;
  value: number;
  line: number;
  isConstant: boolean;
}

export interface ForgeMemoryWindow {
  start: number;
  length: number;
  lastRead: number;
  lastWrite: number;
  bytes: number[];
}

export interface ForgeWatchEntry {
  address: number;
  value16: number;
  value8: number;
}

export interface ForgeStackEntry {
  address: number;
  value: number;
}

export interface ForgeTraceEntry {
  step: number;
  pcBefore: number;
  pcAfter: number;
  spAfter: number;
  flagsAfter: number;
  cycles: number;
  mnemonic: string;
}

export interface ForgeDiagnostic {
  line: number;
  column: number;
  stage?: string;
  message: string;
}

export interface ForgeFramebuffer {
  width: number;
  height: number;
  vramStart: number;
  fbWrites: number;
  pixelsHex: string;
}

export interface ForgeOsState {
  heapStart: number;
  heapBrk: number;
  heapLimit: number;
  heapUsedBytes: number;
  allocCount: number;
  syscallCount: number;
  lastSyscall: number;
  exitCode: number;
  inputQueueCount: number;
}

export interface ForgeStateResponse {
  success: boolean;
  mode?: 'forgelang' | 'assembly';
  hasProgram: boolean;
  activeSourceLine?: number;
  activeAsmLine?: number;
  pc: number;
  prevPc: number;
  sp: number;
  prevSp: number;
  flags: number;
  prevFlags: number;
  flagZ: boolean;
  flagN: boolean;
  flagC: boolean;
  flagV: boolean;
  halted: boolean;
  errorCode: number;
  errorMessage: string;
  faultPc: number;
  cycles: number;
  instructionsExecuted: number;
  registers: number[];
  prevRegisters: number[];
  breakpoints: number[];
  disassembly: ForgeDisasmLine[];
  sourceMap?: ForgeSourceMapEntry[];
  generatedAsm?: string;
  astText?: string;
  globals?: ForgeGlobalVar[];
  callStack?: ForgeCallFrame[];
  symbols: ForgeSymbol[];
  memory: ForgeMemoryWindow;
  watches: ForgeWatchEntry[];
  stack: ForgeStackEntry[];
  trace: ForgeTraceEntry[];
  console: string;
  numericOutputs: number[];
  framebuffer?: ForgeFramebuffer;
  os?: ForgeOsState;
  diagnostics: ForgeDiagnostic[];
}

export interface ExampleProgram {
  id: string;
  language: 'forgelang' | 'assembly';
  title: string;
  description: string;
  source: string;
}

export const FORGE_LANG_EXAMPLES: ExampleProgram[] = [
  {
    id: 'fl_graphics_os',
    language: 'forgelang',
    title: 'ForgeLang: Graphics, Heap & ForgeOS Demo',
    description: 'Allocates an array on the ForgeOS heap (alloc/poke/peek) and renders a 5-bar chart onto the 64x32 framebuffer.',
    source: `// Project Forge — ForgeOS Graphics & Dynamic Heap Demo (graphics_os.fl)
fn draw_bar(x_start, width, height, color) {
    for (let x = x_start; x < x_start + width; x = x + 1) {
        for (let y = 30 - height; y < 30; y = y + 1) {
            draw_pixel(x, y, color)
        }
    }
    return height
}

clear_screen(0)
let bar_heights = alloc(10)
poke(bar_heights + 0, 8)
poke(bar_heights + 2, 14)
poke(bar_heights + 4, 20)
poke(bar_heights + 6, 26)
poke(bar_heights + 8, 18)

for (let bx = 2; bx < 62; bx = bx + 1) {
    draw_pixel(bx, 30, 15)
}

let sum = 0
for (let i = 0; i < 5; i = i + 1) {
    let h = peek(bar_heights + i * 2)
    sum = sum + h
    draw_bar(6 + i * 11, 7, h, 9 + i)
}

print sum
exit(0)
`
  },
  {
    id: 'fl_factorial',
    language: 'forgelang',
    title: 'ForgeLang: Recursive Factorial & Call Stack',
    description: 'Computes 5! (120) and 6! (720) recursively using FP activation frames.',
    source: `// Project Forge — Recursive Factorial (factorial_rec.fl)
fn fact(n) {
    if n <= 1 {
        return 1
    }
    return n * fact(n - 1)
}

let f5 = fact(5)
let f6 = fact(6)
print f5
print f6
`
  },
  {
    id: 'fl_primes',
    language: 'forgelang',
    title: 'ForgeLang: Prime Sieve (for / break / continue)',
    description: 'Finds the first 6 prime numbers using functions, for-loops, modulo (%), break, and continue.',
    source: `// Project Forge — Prime Number Sieve (primes.fl)
fn is_prime(n) {
    if n < 2 {
        return 0
    }
    let d = 2
    while d * d <= n {
        if (n % d) == 0 {
            return 0
        }
        d = d + 1
    }
    return 1
}

let count = 0
for (let candidate = 2; candidate <= 30; candidate = candidate + 1) {
    if !is_prime(candidate) {
        continue
    }
    print candidate
    count = count + 1
    if count >= 6 {
        break
    }
}
`
  },
  {
    id: 'fl_fibonacci',
    language: 'forgelang',
    title: 'ForgeLang: Iterative Fibonacci Function',
    description: 'Computes the first 8 Fibonacci terms using a function with local variables and a while loop.',
    source: `// Project Forge — Fibonacci Sequence in ForgeLang
fn fib_iter(n) {
    if n <= 0 {
        return 0
    }
    let prev = 0
    let curr = 1
    let idx = 1
    while idx < n {
        let next = prev + curr
        prev = curr
        curr = next
        idx = idx + 1
    }
    return curr
}

let i = 0
while i < 8 {
    print fib_iter(i)
    i = i + 1
}
`
  },
  {
    id: 'fl_add_fn',
    language: 'forgelang',
    title: 'ForgeLang: Spec Function & Globals',
    description: 'Section 6 specification program: defines add(x, y), stores result in a global, and prints 15.',
    source: `// Project Forge — Section 6 Specification Example
let a = 10
let b = 5

fn add(x, y) {
    return x + y
}

let result = add(a, b)
print result
`
  },
  {
    id: 'fl_bubble_sort',
    language: 'forgelang',
    title: 'ForgeLang: Heap Bubble Sort',
    description: 'Allocates an array on the ForgeOS heap, sorts it in ascending order, and prints elements.',
    source: `// Project Forge — Bubble Sort in ForgeLang
fn sort_array(ptr, len) {
    for (let i = 0; i < len; i = i + 1) {
        for (let j = 0; j < len - 1 - i; j = j + 1) {
            let addr_a = ptr + j * 2
            let addr_b = ptr + (j + 1) * 2
            let val_a = peek(addr_a)
            let val_b = peek(addr_b)
            if val_a > val_b {
                poke(addr_a, val_b)
                poke(addr_b, val_a)
            }
        }
    }
    return 0
}

let len = 6
let arr = alloc(12)
poke(arr + 0, 45)
poke(arr + 2, 12)
poke(arr + 4, 89)
poke(arr + 6, 7)
poke(arr + 8, 33)
poke(arr + 10, 21)

sort_array(arr, len)

for (let k = 0; k < len; k = k + 1) {
    print peek(arr + k * 2)
}
`
  },
  {
    id: 'fl_bounce',
    language: 'forgelang',
    title: 'ForgeLang: Framebuffer Bouncing Ball Animation',
    description: 'Demonstrates 64x32 virtual graphics display, color palette cycling, and physics loop.',
    source: `// Project Forge — Framebuffer Bouncing Ball Animation (bounce.fl)
clear_screen(0)

for (let bx = 0; bx < 64; bx = bx + 1) {
    draw_pixel(bx, 0, 8)
    draw_pixel(bx, 31, 8)
}
for (let by = 0; by < 32; by = by + 1) {
    draw_pixel(0, by, 8)
    draw_pixel(63, by, 8)
}

let x = 10
let y = 8
let vx = 1
let vy = 1
let color = 11

for (let step = 0; step < 120; step = step + 1) {
    draw_pixel(x, y, 0)
    x = x + vx
    y = y + vy
    if x <= 1 {
        x = 1
        vx = 1
        color = (color % 14) + 1
    }
    if x >= 62 {
        x = 62
        vx = 0 - 1
        color = (color % 14) + 1
    }
    if y <= 1 {
        y = 1
        vy = 1
        color = (color % 14) + 1
    }
    if y >= 30 {
        y = 30
        vy = 0 - 1
        color = (color % 14) + 1
    }
    draw_pixel(x, y, color)
}

print x
print y
`
  }
];

export const FORGE_EXAMPLES: ExampleProgram[] = [
  {
    id: 'bubble_sort',
    language: 'assembly',
    title: 'ASM: Bubble Sort (Ascending RAM 0x8000)',
    description: 'Sorts an array of 8 integers stored at RAM address 0x8000 in ascending order and prints output.',
    source: `; Project Forge — Bubble Sort in Forge-16 Assembly
.entry main
.equ ARRAY_ADDR, 0x8000
.equ ARRAY_LEN, 8

main:
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

    MOV R2, 0

outer_loop:
    CMP R2, ARRAY_LEN
    JGE print_results
    MOV R3, 0

inner_loop:
    MOV R4, ARRAY_LEN
    SUB R4, 1
    SUB R4, R2
    CMP R3, R4
    JGE next_outer

    MOV R5, R3
    MUL R5, 2
    ADD R5, ARRAY_ADDR

    LOAD R6, [R5]
    ADD R5, 2
    LOAD R7, [R5]
    SUB R5, 2

    CMP R6, R7
    JLE no_swap

    STORE R7, [R5]
    ADD R5, 2
    STORE R6, [R5]

no_swap:
    ADD R3, 1
    JMP inner_loop

next_outer:
    ADD R2, 1
    JMP outer_loop

print_results:
    MOV R0, ARRAY_ADDR
    MOV R1, 0

print_loop:
    CMP R1, ARRAY_LEN
    JGE finished
    LOAD R2, [R0]
    OUT R2
    ADD R0, 2
    ADD R1, 1
    JMP print_loop

finished:
    HALT
`
  },
  {
    id: 'popcount',
    language: 'assembly',
    title: 'ASM: Population Count (Bit Counting)',
    description: 'Counts set bits in word 0xBEEF using SHR and bitwise AND, outputting 13.',
    source: `; Project Forge — Population Count (Bit Counting)
.entry main

main:
    MOV R0, 0xBEEF      ; 13 set bits
    MOV R1, 0           ; bit count
    MOV R2, 16          ; 16 bits

count_loop:
    CMP R2, 0
    JZ  done
    MOV R3, R0
    AND R3, 1
    ADD R1, R3
    SHR R0, 1
    SUB R2, 1
    JMP count_loop

done:
    OUT R1
    HALT
`
  },
  {
    id: 'palindrome',
    language: 'assembly',
    title: 'ASM: Palindrome String Checker',
    description: 'Checks if ASCII string "racecar" is a palindrome using two-pointer verification (outputs 1).',
    source: `; Project Forge — Palindrome String Checker
.entry main

main:
    MOV R1, text
    MOV R2, text

find_end:
    LOAD R3, [R2]
    AND R3, 0x00FF
    JZ found_end
    ADD R2, 1
    JMP find_end

found_end:
    SUB R2, 1

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
`
  },
  {
    id: 'fibonacci',
    language: 'assembly',
    title: 'ASM: Fibonacci Sequence & RAM Table',
    description: 'Computes the first 8 Fibonacci numbers, stores them in RAM [0x8000..], and outputs each to the console.',
    source: `; Project Forge — Interactive Debugger Default Program
; Computes Fibonacci numbers and stores them into memory [0x8000..]
.equ COUNT, 8
.equ TABLE_ADDR, 0x8000

main:
    MOV R0, 0           ; a = 0
    MOV R1, 1           ; b = 1
    MOV R2, COUNT       ; remaining terms
    MOV R4, TABLE_ADDR  ; output pointer in RAM

loop:
    CMP R2, 0
    JZ  done
    STORE R0, [R4]      ; write current Fibonacci number to RAM
    OUT R0              ; emit to console
    MOV R3, R0
    ADD R3, R1          ; next = a + b
    MOV R0, R1          ; a = b
    MOV R1, R3          ; b = next
    ADD R4, 2           ; advance word pointer
    SUB R2, 1           ; remaining--
    JMP loop

done:
    HALT
`
  },
  {
    id: 'factorial',
    language: 'assembly',
    title: 'ASM: Recursive Factorial (5! = 120)',
    description: 'Demonstrates hardware stack operations (PUSH, POP, CALL, RET) computing 5! recursively.',
    source: `; Recursive Factorial (5! = 120)
.entry main

main:
    MOV R1, 5           ; n = 5
    CALL factorial      ; R0 = factorial(5)
    OUT R0              ; Output 120
    HALT

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
`
  },
  {
    id: 'hello_console',
    language: 'assembly',
    title: 'ASM: ASCII String Output (Port 0)',
    description: 'Uses .asciz string directive and indexed LOAD [R1] to stream characters to console Port 0.',
    source: `; ASCII Console Output via Port 0
.equ PORT_CHAR, 0

main:
    MOV R1, message     ; R1 = pointer to string

print_loop:
    LOAD R0, [R1]       ; Load 16-bit word at [R1]
    AND  R0, 0x00FF     ; Isolate low ASCII byte
    JZ   done           ; If null terminator, exit
    OUT  R0, PORT_CHAR  ; Emit character to Port 0
    ADD  R1, 1          ; Advance byte pointer
    JMP  print_loop

done:
    HALT

message:
    .asciz "Hello, Forge-16!"
    .byte 10, 0, 0
`
  },
  {
    id: 'add_out',
    language: 'assembly',
    title: 'ASM: Basic Addition & Output',
    description: 'Minimal introductory program: adds 10 + 5 and outputs 15.',
    source: `; Basic Addition and Output
MOV R0, 10
MOV R1, 5
ADD R0, R1
OUT R0
HALT
`
  }
];
