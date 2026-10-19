# Forge-16 CPU Architecture Specification

## 1. Overview

**Forge-16** is a 16-bit educational Von Neumann processor designed for **Project Forge**. It combines a clean, orthogonal register-based architecture with a fixed-length instruction format, deterministic cycle and flag semantics, memory protection boundaries, and integrated I/O facilities.

### Key Architectural Parameters

| Parameter | Specification |
| :--- | :--- |
| **Data Width** | 16-bit words (`uint16_t` / two's complement `int16_t`) |
| **Address Space** | 64 KB (`0x0000`–`0xFFFF`), byte-addressable |
| **Endianness** | Little-endian (least significant byte at lower address) |
| **General Registers** | 8 × 16-bit (`R0`–`R7`) |
| **Special Registers** | `PC` (16-bit Program Counter), `SP` (16-bit Stack Pointer), `FLAGS` (16-bit Status Register) |
| **Instruction Width** | Fixed 32-bit (two 16-bit words = 4 bytes per instruction) |
| **Default Reset `PC`** | `0x0400` (Start of Program `.text` segment) |
| **Default Reset `SP`** | `0xF000` (Top of Stack segment `0xE000`–`0xEFFF`, full-descending) |
| **I/O Architecture** | 256 I/O ports (`IN`/`OUT`) + Memory-Mapped I/O (`0xF000`–`0xFFFF`) |

---

## 2. Register File

### 2.1 General-Purpose Registers (`R0`–`R7`)

All eight general-purpose registers are 16 bits wide and completely orthogonal at the hardware level—any instruction accepting a register operand can use any of `R0` through `R7`.

For software interoperability (assembler macros, high-level compiler code generation, and OS system calls), Forge-16 defines the following calling convention (ABI):

| Register | Encoding | ABI Role | Preserved Across Calls? |
| :--- | :--- | :--- | :--- |
| `R0` | `000` (0) | Return value / Primary accumulator / Argument 0 | No (Caller-saved) |
| `R1` | `001` (1) | Argument 1 / Scratch register | No (Caller-saved) |
| `R2` | `010` (2) | Argument 2 / Scratch register | No (Caller-saved) |
| `R3` | `011` (3) | Argument 3 / Scratch register | No (Caller-saved) |
| `R4` | `100` (4) | General-purpose local variable register | Yes (Callee-saved) |
| `R5` | `101` (5) | General-purpose local variable register | Yes (Callee-saved) |
| `R6` | `110` (6) | General-purpose local variable register | Yes (Callee-saved) |
| `R7` (`FP`) | `111` (7) | Frame Pointer (`FP`) for stack frames | Yes (Callee-saved) |

### 2.2 Special-Purpose Registers

1. **Program Counter (`PC`)**:
   - Holds the 16-bit byte address of the current instruction.
   - Initialized to `0x0400` on CPU reset (or to the binary header entry point when loading a program).
   - During the fetch-decode-execute cycle, `PC` advances by `4` (one 32-bit instruction) unless modified by a control-flow instruction (`JMP`, `JZ`, `JNZ`, `JC`, `JNC`, `JN`, `JL`, `JLE`, `JG`, `JGE`, `CALL`, `RET`, `SYS`).

2. **Stack Pointer (`SP`)**:
   - Holds the 16-bit byte address of the top of the hardware stack.
   - Initialized to `0xF000` on reset.
   - The stack uses a **full-descending** convention within `0xE000`–`0xEFFF`:
     - `PUSH` / `CALL`: Decrements `SP` by 2 (`SP <- SP - 2`), then writes the 16-bit value to `[SP]`. If `SP - 2 < 0xE000`, the CPU raises `FORGE_ERR_STACK_OVERFLOW` and halts in an error state.
     - `POP` / `RET`: Reads the 16-bit value from `[SP]`, then increments `SP` by 2 (`SP <- SP + 2`). If `SP >= 0xF000` prior to popping, the CPU raises `FORGE_ERR_STACK_UNDERFLOW` and halts in an error state.

3. **Status Flags Register (`FLAGS`)**:
   - A 16-bit register whose lower 4 bits hold arithmetic and logical condition codes:

| Bit | Mask | Symbol | Name | Condition Set |
| :--- | :--- | :--- | :--- | :--- |
| 0 | `0x0001` | `Z` | **Zero** | Set to `1` if the 16-bit operation result is `0x0000`; otherwise `0`. |
| 1 | `0x0002` | `N` | **Negative** | Set to `1` if bit 15 (sign bit) of the 16-bit result is `1`; otherwise `0`. |
| 2 | `0x0004` | `C` | **Carry** | Set to `1` on unsigned overflow in `ADD`, unsigned borrow (`a < b`) in `SUB`/`CMP`, or last bit shifted out in `SHL`/`SHR`. |
| 3 | `0x0008` | `V` | **Overflow** | Set to `1` on signed two's-complement overflow in `ADD`, `SUB`, `CMP`, or `MUL`. |
| 4–15 | `0xFFF0` | — | Reserved | Always read as `0`. |

---

## 3. Fetch-Decode-Execute Cycle

Each single step (`forge_cpu_step`) executes one instruction deterministically:

1. **Pre-Execution Check**:
   - Verify the CPU is not halted (`halted == false`) and has no active fault (`error == FORGE_OK`).
   - Verify the cycle/instruction execution limit has not been exceeded.
2. **Instruction Fetch**:
   - Verify `PC` is within executable bounds and `PC <= 0xFFFC` (so all 4 bytes of the instruction reside within the 64 KB address space).
   - Read Word 0 (`instr_hi`, 16-bit little-endian at `PC`) and Word 1 (`imm16`, 16-bit little-endian at `PC + 2`).
   - Compute `next_pc = PC + 4`.
3. **Instruction Decode**:
   - Extract `opcode` (bits `15..10`), `mode` (bits `9..8`), `rd` (bits `7..5`), `rs` (bits `4..2`), and `reserved` (bits `1..0`) from Word 0, plus `imm16` from Word 1.
   - Validate that `opcode`, `mode`, and `reserved` bits form a legal combination; otherwise raise `FORGE_ERR_INVALID_INSTRUCTION`.
4. **Execute & Writeback**:
   - Resolve source/effective address operands according to `mode`.
   - Perform ALU, memory, stack, control-flow, or I/O operation.
   - Update destination register/memory and `FLAGS` if no error occurred.
   - Update `PC` (`next_pc` or branch target) and increment `cycles` and `instructions_executed`.
5. **Trace Recording**:
   - If tracing is enabled, record a structured trace entry containing the pre/post state, decoded instruction, and cycle count.

---

## 4. Error Handling and Exceptions

Forge-16 never invokes undefined C behavior on malformed programs. All hardware faults transition the CPU into an explicit error state (`cpu->error != FORGE_OK`, `cpu->halted = true`) with the faulting `PC` preserved for debugging:

| Error Code | Name | Trigger Condition |
| :--- | :--- | :--- |
| `0` | `FORGE_OK` | Normal execution |
| `1` | `FORGE_ERR_INVALID_INSTRUCTION` | Unknown opcode, illegal addressing mode for opcode, or non-zero reserved bits |
| `2` | `FORGE_ERR_MEMORY_OUT_OF_BOUNDS` | 16-bit word access at `0xFFFF` (crosses 64 KB boundary) or write to protected ROM/system area when write-protection is active |
| `3` | `FORGE_ERR_DIVISION_BY_ZERO` | `DIV` instruction with a divisor of `0` |
| `4` | `FORGE_ERR_STACK_OVERFLOW` | `PUSH` or `CALL` when `SP - 2 < 0xE000` |
| `5` | `FORGE_ERR_STACK_UNDERFLOW` | `POP` or `RET` when `SP >= 0xF000` |
| `6` | `FORGE_ERR_EXECUTION_LIMIT` | Step/run loop reached configured maximum instruction/cycle limit |
| `7` | `FORGE_ERR_INVALID_IO_PORT` | `IN` or `OUT` targeting an unmapped or invalid I/O port (`> 255`) |
| `8` | `FORGE_ERR_LOAD_FAILURE` | Binary image too large or invalid header during program load |

