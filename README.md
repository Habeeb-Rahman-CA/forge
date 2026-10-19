# Project Forge — Educational Computer Systems Laboratory

**Forge** is a complete, software-based computer systems laboratory designed for learning, programming, executing, visualizing, and debugging a custom 16-bit computer architecture entirely in software.

The platform spans the full hardware/software stack:
- **Forge-16 CPU Emulator**: Modular C emulator implementing a 16-bit RISC processor with 32 fixed instructions, 8 registers (`R0`–`R7`, `FP=R7`), status flags (`Z`, `N`, `C`, `V`), and a deterministic cycle and error model.
- **Forge-16 Assembler (`forge-asm`)**: Two-pass assembler supporting symbol tables, forward references, constants (`.equ`), data directives (`.word`, `.byte`, `.asciz`, `.space`), and `.fbin` executable emission and disassembly.
- **ForgeLang Compiler (`forge-cc`)**: Multi-stage C compiler for a high-level programming language featuring lexer, recursive-descent parser, AST builder, semantic analyzer, activation-frame code generator, and bi-directional source mapping.
- **ForgeOS Mini Operating Environment**: Host-managed supervisor runtime with system calls (`OUT Rd, 3`), dynamic heap management (`alloc`/`sbrk`), memory-mapped I/O (cycle counters, PRNG, keyboard input queue), and a 64×32 virtual framebuffer.
- **Native C HTTP/JSON Bridge (`forge-bridge`)**: High-performance local native bridge exposing a documented REST API (`/api/assemble`, `/api/compile`, `/api/step`, `/api/run`, `/api/state`, `/api/source-step`, `/api/input`, `/api/memory/write`).
- **Angular Visual Debugger**: Dark-themed IDE-inspired interface with Monaco Editor syntax highlighting, CPU register cards, flags, memory inspector with live read/write highlights, stack viewer, call stack frame inspector, virtual graphics display, CPU architecture diagram, and interactive tutorials.

---

## Architecture Specifications

| Parameter | Specification |
| :--- | :--- |
| **Data Width** | 16-bit little-endian words |
| **Address Space** | 64 KB flat byte-addressable space (`0x0000`–`0xFFFF`) |
| **General Registers** | Eight 16-bit registers: `R0`–`R7` (`R7` is also Frame Pointer `FP`) |
| **Special Registers** | Program Counter (`PC`), Stack Pointer (`SP` initialized to `0xEFFE`) |
| **Status Flags** | `Z` (Zero), `N` (Negative), `C` (Carry / Borrow), `V` (Overflow) |
| **Instruction Size** | Fixed 32-bit (two 16-bit words: `Word 0` = Opcode/Mode/Rd/Rs, `Word 1` = Immediate/Displacement) |
| **Instruction Set** | 32 instructions covering Data Movement, Arithmetic, Bitwise, Shifts, Compares, Jumps, Calls, Stack, and I/O |
| **Clock Cycle Model** | Deterministic: 1 cycle (ALU/MOV), 2 cycles (LOAD/STORE/JMP/Stack/IO), 3 cycles (MUL/CALL/RET), 4 cycles (DIV) |
| **Graphics Display** | 64×32 virtual pixel framebuffer (`0xF000`–`0xF7FF`) with 16-color palette |
| **Memory Map** | Code (`0x0000`–`0x7FFF`), RAM (`0x8000`–`0x8FFF`), Heap (`0x9000`–`0xDFFF`), Stack (`0xE000`–`0xEFFF`), VRAM (`0xF000`–`0xF7FF`), MMIO (`0xFF00`–`0xFF0F`) |

---

## Repository Structure

```text
forge/
├── docs/
│   ├── architecture.md        # CPU register model, ALU flags, and cycle model
│   ├── instruction-set.md     # Complete 32-instruction ISA specifications
│   ├── memory-map.md          # 64 KB memory regions, VRAM & MMIO ports
│   ├── binary-format.md       # .fbin executable container file format
│   ├── assembly-reference.md  # Assembly language syntax, directives & labels
│   ├── language-spec.md       # ForgeLang grammar, AST, and semantic rules
│   ├── bridge-api.md          # Native C HTTP/JSON Bridge REST API specification
│   ├── tutorials.md           # Step-by-step computer systems learning curriculum
│   └── exercises.md           # Guided student exercises with solutions
├── emulator/
│   ├── include/               # Public C headers (forge_cpu.h, forge_os.h, etc.)
│   ├── src/                   # CPU state, memory, decoder, ALU, execution engine, forge-emu
│   ├── tests/                 # CTest automated emulator test suite
│   └── CMakeLists.txt
├── assembler/
│   ├── include/               # Public C headers (forge_asm.h, forge_bridge.h)
│   ├── src/                   # Lexer, symtab, two-pass core, disassembler, forge-asm, forge-bridge
│   ├── tests/                 # CTest automated assembler test suite
│   └── CMakeLists.txt
├── compiler/
│   ├── include/               # Public C headers (forge_compiler.h)
│   ├── src/                   # Lexer, parser, AST, semantic analyzer, codegen, forge-cc
│   ├── tests/                 # CTest automated compiler test suite
│   └── CMakeLists.txt
├── frontend/                  # Angular 21 + Tailwind CSS + Monaco Editor Visual Debugger
│   ├── src/app/               # Debugger components, bridge client, models, tutorials, ISA data
│   └── package.json
├── examples/
│   ├── assembly/              # .asm examples (Fibonacci, Sort, Factorial, Popcount, Palindrome)
│   ├── programs/              # .fl examples (Primes, Heap Sort, Graphics Bar Chart, Bouncing Ball)
│   └── binaries/              # Pre-compiled .fbin executable binaries
├── scripts/
│   ├── test_all.sh            # Automated test runner across C, CLI, and Angular
│   ├── run-debugger.sh        # Starts C native bridge and Angular debugger
│   └── package_release.sh     # Bundles release package tarball
└── README.md
```

---

## Quick Start & Build Instructions

### Prerequisites
- **C Compiler**: GCC or Clang supporting C11
- **Build System**: CMake (>= 3.16) and Make/Ninja
- **Frontend**: Node.js (>= 18) and npm

### 1. Build and Test the C Core
```bash
# Configure and build in Release mode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run all C test suites
ctest --test-dir build --output-on-failure
```

### 2. Run All Automated Tests (C + Programs + Angular)
```bash
./scripts/test_all.sh
```

### 3. Launch the Visual Debugger Lab
```bash
# Starts the native C HTTP bridge (port 8080) and launches the Angular UI
./scripts/run-debugger.sh
```
Navigate your browser to `http://localhost:4200` (or `http://localhost:8080`).

---

## Command-Line Tools (CLI Reference)

### 1. Standalone CPU Emulator (`forge-emu`)
Executes `.fbin` binaries directly from the terminal without the frontend.

```bash
# Run a binary to completion
./build/emulator/forge-emu examples/binaries/fibonacci.fbin

# Run with single-step instruction tracing enabled
./build/emulator/forge-emu examples/binaries/add_out.fbin --trace

# Limit execution steps to prevent infinite loops
./build/emulator/forge-emu examples/binaries/factorial_rec.fbin --max-steps 1000
```

### 2. Two-Pass Assembler & Disassembler (`forge-asm`)
Converts `.asm` source files into `.fbin` machine code binaries.

```bash
# Assemble assembly source into an executable binary
./build/assembler/forge-asm examples/assembly/bubble_sort.asm -o /tmp/sort.fbin

# Assemble and print human-readable disassembly
./build/assembler/forge-asm examples/assembly/popcount.asm -o /tmp/pop.fbin --disasm

# Disassemble an existing binary
./build/assembler/forge-asm --disasm-only /tmp/sort.fbin
```

### 3. High-Level Language Compiler (`forge-cc`)
Compiles high-level `.fl` source code into Forge-16 assembly and machine code.

```bash
# Compile and run immediately in the integrated emulator
./build/compiler/forge-cc examples/programs/bubble_sort.fl --run

# Emit generated Forge-16 assembly code
./build/compiler/forge-cc examples/programs/factorial_rec.fl --emit-asm /tmp/fact.asm

# Print the Abstract Syntax Tree (AST) and Source Map
./build/compiler/forge-cc examples/programs/primes.fl --print-ast --sourcemap

# Compile directly to .fbin executable
./build/compiler/forge-cc examples/programs/bounce.fl -o /tmp/bounce.fbin
```

### 4. Native C HTTP/JSON Debugger Bridge (`forge-bridge`)
Provides a native REST API server for external tools and the Angular visual debugger.

```bash
# Launch bridge server on port 8080
./build/assembler/forge-bridge 8080

# Assemble and inspect state via curl
curl -X POST http://127.0.0.1:8080/api/assemble \
     -H "Content-Type: application/json" \
     -d '{"source": "MOV R0, 10\nADD R0, 5\nOUT R0\nHALT\n"}'

# Step one instruction
curl -X POST http://127.0.0.1:8080/api/step
```

---

## Visual Debugger Features

The Forge-16 visual debugger provides a professional, IDE-grade developer experience:
- **Dual-Language Monaco Editor**: Switch seamlessly between **Forge-16 Assembly (`.asm`)** and **ForgeLang (`.fl`)** with custom syntax highlighting, active line indicators, and gutter breakpoints.
- **CPU Registers & Live ALU Breakdown**: Real-time inspection of registers `R0`–`R7`, `PC`, `SP`, clock cycles, status flags (`Z`, `N`, `C`, `V`), and decoded ALU operations (Operand A, Operand B, Result, and flag changes).
- **Execution Views**: Disassembly view with breakpoints, Generated Assembly view, AST view, and a 32-bit Machine Code Instruction Decoder.
- **Memory Subsystem Viewer**: Hexadecimal + ASCII memory inspector with live color-coded read/write tracking, memory poke controls, and address watchlist.
- **Variables & Call Stack Inspector**: Hardware stack viewer and high-level activation frame inspection showing function names, return addresses, and local variables (`[FP - 2]`, `[FP + 4]`).
- **Display & ForgeOS Runtime**: 64×32 virtual framebuffer display with retro 16-color palette, dynamic heap allocation inspector (`sbrk`), system call counters, and interactive keyboard input queue.
- **Microarchitecture & Profiler**: Cycles Per Instruction (CPI) statistics, instruction category mix, conceptual 5-stage classic RISC pipeline tracker, and direct-mapped L1 cache simulator.
- **Interactive Tutorials & Guided Exercises**: Built-in curriculum with theory, guided walkthroughs, and one-click "Load into Editor" exercises.
- **CPU Architecture Block Diagram**: Interactive SVG diagram illustrating the Control Unit, Register Bank, ALU, Buses, Memory Subsystem, and I/O Peripherals.
- **ISA Quick Reference**: Searchable catalog of all 32 instructions with opcodes, addressing modes, clock cycles, and flag behavior.

---

## Release Packaging

To build and package a standalone production release archive:

```bash
./scripts/package_release.sh
```

This generates `dist/forge-v1.0.0-linux-x86_64.tar.gz` containing:
- `bin/`: Release C binaries (`forge-emu`, `forge-asm`, `forge-cc`, `forge-bridge`, `forge-lab.sh`)
- `web/`: Production Angular frontend bundle
- `include/`: Public C library headers
- `examples/`: Assembly sources, ForgeLang programs, and pre-compiled `.fbin` binaries
- `docs/`: Complete architectural specifications, ISA reference, tutorials, and exercises

---

## License

Educational Open-Source Laboratory — Built from scratch for computer systems engineering and computer architecture education.

```bash
# Assemble assembly source into an executable binary
./build/assembler/forge-asm examples/assembly/bubble_sort.asm -o /tmp/sort.fbin

# Assemble and print human-readable disassembly
./build/assembler/forge-asm examples/assembly/popcount.asm -o /tmp/pop.fbin --disasm

# Disassemble an existing binary
./build/assembler/forge-asm --disasm-only /tmp/sort.fbin
```

### 3. High-Level Language Compiler (`forge-cc`)
Compiles high-level `.fl` source code into Forge-16 assembly and machine code.

```bash
# Compile and run immediately in the integrated emulator
./build/compiler/forge-cc examples/programs/bubble_sort.fl --run

# Emit generated Forge-16 assembly code
./build/compiler/forge-cc examples/programs/factorial_rec.fl --emit-asm /tmp/fact.asm

# Print the Abstract Syntax Tree (AST) and Source Map
./build/compiler/forge-cc examples/programs/primes.fl --print-ast --sourcemap

# Compile directly to .fbin executable
./build/compiler/forge-cc examples/programs/bounce.fl -o /tmp/bounce.fbin
```

### 4. Native C HTTP/JSON Debugger Bridge (`forge-bridge`)
Provides a native REST API server for external tools and the Angular visual debugger.

```bash
# Launch bridge server on port 8080
./build/assembler/forge-bridge 8080

# Assemble and inspect state via curl
curl -X POST http://127.0.0.1:8080/api/assemble \
     -H "Content-Type: application/json" \
     -d '{"source": "MOV R0, 10\nADD R0, 5\nOUT R0\nHALT\n"}'

# Step one instruction
curl -X POST http://127.0.0.1:8080/api/step
```

---

## Visual Debugger Features

The Forge-16 visual debugger provides a professional, IDE-grade developer experience:
- **Dual-Language Monaco Editor**: Switch seamlessly between **Forge-16 Assembly (`.asm`)** and **ForgeLang (`.fl`)** with custom syntax highlighting, active line indicators, and gutter breakpoints.
- **CPU Registers & Live ALU Breakdown**: Real-time inspection of registers `R0`–`R7`, `PC`, `SP`, clock cycles, status flags (`Z`, `N`, `C`, `V`), and decoded ALU operations (Operand A, Operand B, Result, and flag changes).
- **Execution Views**: Disassembly view with breakpoints, Generated Assembly view, AST view, and a 32-bit Machine Code Instruction Decoder.
- **Memory Subsystem Viewer**: Hexadecimal + ASCII memory inspector with live color-coded read/write tracking, memory poke controls, and address watchlist.
- **Variables & Call Stack Inspector**: Hardware stack viewer and high-level activation frame inspection showing function names, return addresses, and local variables (`[FP - 2]`, `[FP + 4]`).
- **Display & ForgeOS Runtime**: 64×32 virtual framebuffer display with retro 16-color palette, dynamic heap allocation inspector (`sbrk`), system call counters, and interactive keyboard input queue.
- **Microarchitecture & Profiler**: Cycles Per Instruction (CPI) statistics, instruction category mix, conceptual 5-stage classic RISC pipeline tracker, and direct-mapped L1 cache simulator.
- **Interactive Tutorials & Guided Exercises**: Built-in curriculum with theory, guided walkthroughs, and one-click "Load into Editor" exercises.
- **CPU Architecture Block Diagram**: Interactive SVG diagram illustrating the Control Unit, Register Bank, ALU, Buses, Memory Subsystem, and I/O Peripherals.
- **ISA Quick Reference**: Searchable catalog of all 32 instructions with opcodes, addressing modes, clock cycles, and flag behavior.

---

## Release Packaging

To build and package a standalone production release archive:

```bash
./scripts/package_release.sh
```

This generates `dist/forge-v1.0.0-linux-x86_64.tar.gz` containing:
- `bin/`: Release C binaries (`forge-emu`, `forge-asm`, `forge-cc`, `forge-bridge`, `forge-lab.sh`)
- `web/`: Production Angular frontend bundle
- `include/`: Public C library headers
- `examples/`: Assembly sources, ForgeLang programs, and pre-compiled `.fbin` binaries
- `docs/`: Complete architectural specifications, ISA reference, tutorials, and exercises

---

## License

Educational Open-Source Laboratory — Built from scratch for computer systems engineering and computer architecture education.

