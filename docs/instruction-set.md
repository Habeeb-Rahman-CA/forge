# Forge-16 Instruction Set Architecture (ISA)

## 1. Instruction Encoding Format

Every Forge-16 instruction is **32 bits (4 bytes)** long, stored in little-endian byte order as two consecutive 16-bit words (`Word 0` at `PC`, `Word 1` at `PC + 2`):

```text
 Word 0 (at PC, 16 bits):
 +-----------------------+------------+------------+------------+------------+
 | 15 14 13 12 11 10     |  9      8  |  7   6   5 |  4   3   2 |  1      0  |
 |      OPCODE (6b)      |  MODE (2b) |   RD (3b)  |   RS (3b)  |  RSVD (2b) |
 +-----------------------+------------+------------+------------+------------+

 Word 1 (at PC + 2, 16 bits):
 +---------------------------------------------------------------------------+
 | 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0                           |
 |                              IMM16 (16b)                                  |
 +---------------------------------------------------------------------------+
```

### Bitfield Definitions

- **`OPCODE` (Bits 15–10, 6 bits)**: Identifies the operation (`0x00`–`0x1F`). Values `0x20`–`0x3F` are unallocated and fault with `FORGE_ERR_INVALID_INSTRUCTION`.
- **`MODE` (Bits 9–8, 2 bits)**: Addressing / Operand Mode:
  - `00` (`FORGE_MODE_REG`): **Register Mode** — Source operand is register `RS`. `IMM16` must be `0x0000` (unless otherwise specified).
  - `01` (`FORGE_MODE_IMM`): **Immediate Mode** — Source operand or target address is the 16-bit immediate `IMM16`.
  - `10` (`FORGE_MODE_MEM_DIR`): **Direct Memory Mode** — Effective memory address is `EA = IMM16` (`[imm16]`).
  - `11` (`FORGE_MODE_MEM_IDX`): **Indexed / Register-Indirect Mode** — Effective memory address is `EA = (RS + IMM16) & 0xFFFF` (`[RS + imm16]`).
- **`RD` (Bits 7–5, 3 bits)**: Primary / destination register `R0`–`R7` (`0`–`7`).
- **`RS` (Bits 4–2, 3 bits)**: Secondary / source / base register `R0`–`R7` (`0`–`7`).
- **`RSVD` (Bits 1–0, 2 bits)**: Reserved bits. Must be `00`; non-zero values trigger `FORGE_ERR_INVALID_INSTRUCTION`.
- **`IMM16` (Word 1, 16 bits)**: Immediate value, absolute memory address, I/O port number, or signed 16-bit displacement.

---

## 2. Complete Opcode Table

| Opcode (Hex) | Opcode (Dec) | Mnemonic | Category | Allowed Modes | Cycles | Flags Updated | Summary |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `0x00` | 0 | `NOP` | Control | `REG` | 1 | None | No operation |
| `0x01` | 1 | `HALT` | Control | `REG` | 1 | None | Stop CPU execution cleanly |
| `0x02` | 2 | `MOV` | Data | `REG`, `IMM` | 1 | None | `RD <- src` |
| `0x03` | 3 | `LOAD` | Data | `MEM_DIR`, `MEM_IDX` | 2 | None | `RD <- Mem16[EA]` |
| `0x04` | 4 | `STORE` | Data | `MEM_DIR`, `MEM_IDX` | 2 | None | `Mem16[EA] <- RD` |
| `0x05` | 5 | `ADD` | Arithmetic | `REG`, `IMM` | 1 | `Z, N, C, V` | `RD <- RD + src` |
| `0x06` | 6 | `SUB` | Arithmetic | `REG`, `IMM` | 1 | `Z, N, C, V` | `RD <- RD - src` |
| `0x07` | 7 | `MUL` | Arithmetic | `REG`, `IMM` | 3 | `Z, N, C, V` | `RD <- (RD * src) & 0xFFFF` |
| `0x08` | 8 | `DIV` | Arithmetic | `REG`, `IMM` | 4 | `Z, N, C, V` | `RD <- RD / src` (unsigned 16-bit) |
| `0x09` | 9 | `AND` | Bitwise | `REG`, `IMM` | 1 | `Z, N` (`C=0, V=0`) | `RD <- RD & src` |
| `0x0A` | 10 | `OR` | Bitwise | `REG`, `IMM` | 1 | `Z, N` (`C=0, V=0`) | `RD <- RD \| src` |
| `0x0B` | 11 | `XOR` | Bitwise | `REG`, `IMM` | 1 | `Z, N` (`C=0, V=0`) | `RD <- RD ^ src` |
| `0x0C` | 12 | `NOT` | Bitwise | `REG` | 1 | `Z, N` (`C=0, V=0`) | `RD <- ~RD` |
| `0x0D` | 13 | `SHL` | Shift | `REG`, `IMM` | 1 | `Z, N, C` (`V=0`) | `RD <- RD << (src & 0x1F)` |
| `0x0E` | 14 | `SHR` | Shift | `REG`, `IMM` | 1 | `Z, N, C` (`V=0`) | `RD <- RD >> (src & 0x1F)` (logical) |
| `0x0F` | 15 | `CMP` | Compare | `REG`, `IMM` | 1 | `Z, N, C, V` | Compute `RD - src`, update flags only |
| `0x10` | 16 | `JMP` | Control Flow | `IMM`, `REG` | 2 | None | `PC <- target` |
| `0x11` | 17 | `JZ` | Control Flow | `IMM`, `REG` | 2 | None | Jump if `Z == 1` |
| `0x12` | 18 | `JNZ` | Control Flow | `IMM`, `REG` | 2 | None | Jump if `Z == 0` |
| `0x13` | 19 | `CALL` | Control Flow | `IMM`, `REG` | 3 | None | Push `PC + 4`, `PC <- target` |
| `0x14` | 20 | `RET` | Control Flow | `REG` | 3 | None | Pop `PC` from stack |
| `0x15` | 21 | `PUSH` | Stack | `REG`, `IMM` | 2 | None | `SP <- SP - 2; Mem16[SP] <- src` |
| `0x16` | 22 | `POP` | Stack | `REG` | 2 | None | `RD <- Mem16[SP]; SP <- SP + 2` |
| `0x17` | 23 | `IN` | I/O | `IMM`, `REG` | 2 | None | `RD <- Port[port]` |
| `0x18` | 24 | `OUT` | I/O | `IMM`, `REG` | 2 | None | `Port[port] <- RD` |
| `0x19` | 25 | `JC` | Control Flow | `IMM`, `REG` | 2 | None | Jump if `C == 1` (unsigned `<` after `CMP` if borrow, or carry set) |
| `0x1A` | 26 | `JNC` | Control Flow | `IMM`, `REG` | 2 | None | Jump if `C == 0` |
| `0x1B` | 27 | `JN` | Control Flow | `IMM`, `REG` | 2 | None | Jump if `N == 1` |
| `0x1C` | 28 | `JL` | Control Flow | `IMM`, `REG` | 2 | None | Signed `<`: Jump if `N != V` |
| `0x1D` | 29 | `JLE` | Control Flow | `IMM`, `REG` | 2 | None | Signed `<=`: Jump if `Z == 1` or `N != V` |
| `0x1E` | 30 | `JG` | Control Flow | `IMM`, `REG` | 2 | None | Signed `>`: Jump if `Z == 0` and `N == V` |
| `0x1F` | 31 | `JGE` | Control Flow | `IMM`, `REG` | 2 | None | Signed `>=`: Jump if `N == V` |

---

## 3. Detailed Instruction Semantics

### 3.1 Data Movement Instructions

#### `MOV` (`0x02`)
- **Syntax**: `MOV Rd, Rs` (`MODE=REG`) or `MOV Rd, imm16` (`MODE=IMM`)
- **Operation**: Copies the 16-bit value from `Rs` or `imm16` into `Rd`.
- **Flags**: Unchanged.
- **Cycles**: 1.

#### `LOAD` (`0x03`)
- **Syntax**: `LOAD Rd, [imm16]` (`MODE=MEM_DIR`) or `LOAD Rd, [Rs + imm16]` (`MODE=MEM_IDX`)
- **Operation**: Computes effective address `EA = imm16` (for `MEM_DIR`) or `EA = (Rs + imm16) & 0xFFFF` (for `MEM_IDX`). Reads 16-bit little-endian word from `Mem[EA]` and `Mem[EA + 1]` into `Rd`.
- **Flags**: Unchanged.
- **Cycles**: 2.
- **Errors**: `FORGE_ERR_MEMORY_OUT_OF_BOUNDS` if `EA == 0xFFFF`.

#### `STORE` (`0x04`)
- **Syntax**: `STORE Rd, [imm16]` (`MODE=MEM_DIR`) or `STORE Rd, [Rs + imm16]` (`MODE=MEM_IDX`)
- **Operation**: Computes effective address `EA` and writes the 16-bit value of `Rd` in little-endian order to `Mem[EA]` and `Mem[EA + 1]`.
- **Flags**: Unchanged.
- **Cycles**: 2.
- **Errors**: `FORGE_ERR_MEMORY_OUT_OF_BOUNDS` if `EA == 0xFFFF` or if writing to a write-protected region.

---

### 3.2 Arithmetic and Comparison Instructions

Let `a` be the 16-bit unsigned value of `Rd`, `b` be the 16-bit unsigned source operand (`Rs` or `imm16`), and `sa = (int16_t)a`, `sb = (int16_t)b`.

#### `ADD` (`0x05`)
- **Operation**: `res32 = (uint32_t)a + (uint32_t)b`, `res16 = (uint16_t)res32`, `Rd <- res16`.
- **Flags**:
  - `Z`: `res16 == 0`
  - `N`: `(res16 & 0x8000) != 0`
  - `C`: `res32 > 0xFFFF` (unsigned overflow)
  - `V`: `((~(a ^ b)) & (a ^ res16) & 0x8000) != 0` (signed overflow: operands have same sign, result has opposite sign)
- **Cycles**: 1.

#### `SUB` (`0x06`) and `CMP` (`0x0F`)
- **Operation**: `res16 = (uint16_t)(a - b)`. `SUB` writes `res16` to `Rd`; `CMP` discards `res16` and only updates `FLAGS`.
- **Flags**:
  - `Z`: `res16 == 0`
  - `N`: `(res16 & 0x8000) != 0`
  - `C`: `a < b` (unsigned borrow occurred)
  - `V`: `(((a ^ b) & (a ^ res16)) & 0x8000) != 0` (signed overflow: operands have different signs, result sign differs from `a`)
- **Cycles**: 1.

#### `MUL` (`0x07`)
- **Operation**: `prod_u32 = (uint32_t)a * (uint32_t)b`, `prod_s32 = (int32_t)sa * (int32_t)sb`, `res16 = (uint16_t)(prod_u32 & 0xFFFF)`, `Rd <- res16`.
- **Flags**:
  - `Z`: `res16 == 0`
  - `N`: `(res16 & 0x8000) != 0`
  - `C`: `(prod_u32 >> 16) != 0` (upper 16 bits of unsigned product non-zero)
  - `V`: `prod_s32 < -32768 || prod_s32 > 32767` (signed product overflows 16-bit signed range)
- **Cycles**: 3.

#### `DIV` (`0x08`)
- **Operation**: Unsigned 16-bit integer division `res16 = a / b`, `Rd <- res16`.
- **Flags**:
  - `Z`: `res16 == 0`
  - `N`: `(res16 & 0x8000) != 0`
  - `C`: `0`
  - `V`: `0`
- **Cycles**: 4.
- **Errors**: If `b == 0`, raises `FORGE_ERR_DIVISION_BY_ZERO`; `Rd` and `FLAGS` remain unmodified.

---

### 3.3 Bitwise and Shift Instructions

#### `AND` (`0x09`), `OR` (`0x0A`), `XOR` (`0x0B`), `NOT` (`0x0C`)
- **Operation**:
  - `AND`: `Rd <- Rd & src`
  - `OR`: `Rd <- Rd | src`
  - `XOR`: `Rd <- Rd ^ src`
  - `NOT`: `Rd <- ~Rd` (`MODE=REG`, `RS` and `IMM16` ignored/zero)
- **Flags**: `Z` and `N` updated from result; `C <- 0`, `V <- 0`.
- **Cycles**: 1.

#### `SHL` (`0x0D`) and `SHR` (`0x0E`)
- **Operation**:
  - Shift count is `cnt = src & 0x1F` (masked to 0–31).
  - If `cnt == 0`: `res16 = a`, `carry = 0`.
  - For `SHL`:
    - If `cnt <= 16`: `carry = (a >> (16 - cnt)) & 1`.
    - If `cnt > 16`: `carry = 0`.
    - `res16 = (cnt < 16) ? (uint16_t)(a << cnt) : 0`.
  - For `SHR` (logical shift right):
    - If `cnt <= 16`: `carry = (a >> (cnt - 1)) & 1`.
    - If `cnt > 16`: `carry = 0`.
    - `res16 = (cnt < 16) ? (uint16_t)(a >> cnt) : 0`.
  - `Rd <- res16`.
- **Flags**: `Z`, `N` updated from `res16`; `C <- carry`; `V <- 0`.
- **Cycles**: 1.

---

### 3.4 Control-Flow and Stack Instructions

#### `JMP` (`0x10`), `JZ` (`0x11`), `JNZ` (`0x12`), `JC` (`0x19`), `JNC` (`0x1A`), `JN` (`0x1B`), `JL` (`0x1C`), `JLE` (`0x1D`), `JG` (`0x1E`), `JGE` (`0x1F`)
- **Target**: `target = (MODE == IMM) ? imm16 : Rs`.
- **Operation**: If condition holds, `PC <- target`; otherwise `PC <- PC + 4`.
- **Flags**: Unchanged.
- **Cycles**: 2.

#### `CALL` (`0x13`)
- **Target**: `target = (MODE == IMM) ? imm16 : Rs`.
- **Operation**:
  1. Check stack bounds: if `SP - 2 < 0xE000`, raise `FORGE_ERR_STACK_OVERFLOW`.
  2. `SP <- SP - 2`
  3. `Mem16[SP] <- PC + 4` (return address)
  4. `PC <- target`
- **Flags**: Unchanged.
- **Cycles**: 3.

#### `RET` (`0x14`)
- **Operation**:
  1. Check stack bounds: if `SP >= 0xF000`, raise `FORGE_ERR_STACK_UNDERFLOW`.
  2. `return_addr <- Mem16[SP]`
  3. `SP <- SP + 2`
  4. `PC <- return_addr`
- **Flags**: Unchanged.
- **Cycles**: 3.

#### `PUSH` (`0x15`)
- **Source**: `val = (MODE == IMM) ? imm16 : Rd` (Note: in `MODE=REG`, `PUSH Rd` pushes `Rd`).
- **Operation**:
  1. If `SP - 2 < 0xE000`, raise `FORGE_ERR_STACK_OVERFLOW`.
  2. `SP <- SP - 2`
  3. `Mem16[SP] <- val`
- **Flags**: Unchanged.
- **Cycles**: 2.

#### `POP` (`0x16`)
- **Destination**: `Rd` (`MODE=REG`).
- **Operation**:
  1. If `SP >= 0xF000`, raise `FORGE_ERR_STACK_UNDERFLOW`.
  2. `Rd <- Mem16[SP]`
  3. `SP <- SP + 2`
- **Flags**: Unchanged.
- **Cycles**: 2.

---

### 3.5 Input/Output and Program Control Instructions

#### `IN` (`0x17`)
- **Syntax**: `IN Rd, port_imm` (`MODE=IMM`, `port = imm16`) or `IN Rd, Rs` (`MODE=REG`, `port = Rs`)
- **Operation**: Reads a 16-bit value from I/O port `port` into `Rd`.
- **Errors**: If `port > 255`, raises `FORGE_ERR_INVALID_IO_PORT`.
- **Flags**: Unchanged.
- **Cycles**: 2.

#### `OUT` (`0x18`)
- **Syntax**: `OUT Rd, port_imm` (`MODE=IMM`, `port = imm16`; assembly shorthand `OUT Rd` defaults to `port = 1` numeric output) or `OUT Rd, Rs` (`MODE=REG`, `port = Rs`)
- **Operation**: Writes the 16-bit value of `Rd` to I/O port `port`.
  - Port `0` (`FORGE_PORT_CONSOLE_CHAR`): Appends `(char)(Rd & 0xFF)` to the console output buffer.
  - Port `1` (`FORGE_PORT_CONSOLE_INT`): Formats `Rd` (and signed representation) to the console output buffer and records value in the numeric output log.
- **Errors**: If `port > 255`, raises `FORGE_ERR_INVALID_IO_PORT`.
- **Flags**: Unchanged.
- **Cycles**: 2.

#### `NOP` (`0x00`) and `HALT` (`0x01`)
- **`NOP`**: Advances `PC <- PC + 4`. Consumes 1 cycle.
- **`HALT`**: Sets `cpu->halted = true`, advances `PC <- PC + 4`. Consumes 1 cycle.

