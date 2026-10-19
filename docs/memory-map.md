# Forge-16 Memory Map Specification

## 1. Address Space Layout (64 KB)

Forge-16 provides a flat, byte-addressable 64 KB (`0x0000`–`0xFFFF`, 65,536 bytes) address space organized into six functional segments:

```text
  0xFFFF +------------------------------------------+
         | Memory-Mapped I/O & Device Registers     |  256 Bytes
  0xFF00 +------------------------------------------+
         | Video / Character Framebuffer (MMIO)     |  3,840 Bytes
  0xF000 +------------------------------------------+  <-- Initial SP (0xF000)
         | Hardware Stack Segment (Grows Downward)  |  4 KB (4,096 Bytes)
  0xE000 +------------------------------------------+  <-- Stack Limit (0xE000)
         | General RAM / Dynamic Heap Segment       |  24 KB (24,576 Bytes)
  0x8000 +------------------------------------------+
         | Program Code (.text) & Static Data       |  31 KB (31,744 Bytes)
  0x0400 +------------------------------------------+  <-- Default Entry PC (0x0400)
         | System Vector Table & Kernel Data        |  1 KB (1,024 Bytes)
  0x0000 +------------------------------------------+
```

---

## 2. Segment Details

| Address Range | Size | Name | Permissions | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0x0000`–`0x03FF` | 1 KB | **System / Vectors** | Read / Kernel Write | Reset vector (`0x0000`), trap vector (`0x0004`), OS system data |
| `0x0400`–`0x7FFF` | 31 KB | **Program (`.text`/`.data`)** | Read / Write / Exec | User machine code instructions and static data constants |
| `0x8000`–`0xDFFF` | 24 KB | **User RAM / Heap** | Read / Write | General-purpose data storage and dynamic allocator heap |
| `0xE000`–`0xEFFF` | 4 KB | **Stack Segment** | Read / Write | Hardware call/data stack (`SP` initialized to `0xF000`, grows down to `0xE000`) |
| `0xF000`–`0xFEFF` | 3,840 B | **Framebuffer** | Read / Write | Display memory for text/pixel console visualization |
| `0xFF00`–`0xFFFF` | 256 B | **MMIO Registers** | Read / Write | Memory-mapped I/O device control and status registers |

---

## 3. Alignment, Endianness, and Bounds Checking

1. **Byte Ordering**:
   - All 16-bit words are stored in **little-endian** format:
     - `Mem[addr]` holds bits `7..0` (least significant byte).
     - `Mem[addr + 1]` holds bits `15..8` (most significant byte).
2. **Word Boundary Check**:
   - A 16-bit read or write at address `0xFFFF` attempts to access `0xFFFF` and `0x10000` (outside the 64 KB space).
   - The memory subsystem rejects any 16-bit access at `addr == 0xFFFF` and raises `FORGE_ERR_MEMORY_OUT_OF_BOUNDS`.
3. **Instruction Fetch Bounds**:
   - Because every instruction is 4 bytes (2 words), fetching an instruction at `PC > 0xFFFC` raises `FORGE_ERR_MEMORY_OUT_OF_BOUNDS`.
4. **Stack Segment Protection**:
   - `PUSH` and `CALL` verify that `SP >= 0xE002` and `SP <= 0xF000` before decrementing `SP` by 2. Attempting to push below `0xE000` raises `FORGE_ERR_STACK_OVERFLOW`.
   - `POP` and `RET` verify that `SP >= 0xE000` and `SP <= 0xEFFE` before reading and incrementing `SP` by 2. Attempting to pop when `SP >= 0xF000` raises `FORGE_ERR_STACK_UNDERFLOW`.

---

## 4. I/O Port Map (`IN` / `OUT` Instructions)

In addition to memory-mapped I/O, Forge-16 provides 256 port-mapped I/O channels (`0x00`–`0xFF`):

| Port | Symbol | Direction | Description |
| :--- | :--- | :--- | :--- |
| `0` (`0x00`) | `FORGE_PORT_CONSOLE_CHAR` | R/W | Write: emit ASCII character (`Rd & 0xFF`). Read: pop next character from console input queue (`0` if empty). |
| `1` (`0x01`) | `FORGE_PORT_CONSOLE_INT` | R/W | Write: emit 16-bit integer value (`Rd`) to output stream and numeric output log (default port for `OUT Rd`). |
| `2` (`0x02`) | `FORGE_PORT_KEYBOARD_STATUS` | R | Returns `1` if input character is available in queue, `0` otherwise. |
| `3` (`0x03`) | `FORGE_PORT_SYSCALL` | W | Invokes host/OS system call number in `Rd` with arguments in `R1`–`R3`, returning result in `R0`. |
| `4`–`255` | General User Ports | R/W | General-purpose virtual device ports stored in `cpu->io_ports[port]`. |

---

## 5. Virtual Framebuffer (`0xF000`–`0xF7FF`, 64×32 Pixels)

The video display adapter maps a 64×32 pixel screen directly into VRAM:
- **Dimensions**: 64 pixels wide × 32 pixels high (2,048 bytes total).
- **Addressing**: `addr = 0xF000 + (y * 64) + x` for `x in 0..63`, `y in 0..31`.
- **Palette**: 16-color VGA/CGA palette index stored in low nibble (`color & 0x0F`):
  - `0`: Black (`#0b0f17`)
  - `1`: Blue (`#1d4ed8`)
  - `2`: Green (`#15803d`)
  - `3`: Cyan (`#0e7490`)
  - `4`: Red (`#b91c1c`)
  - `5`: Magenta (`#a21caf`)
  - `6`: Amber (`#b45309`)
  - `7`: Light Gray (`#94a3b8`)
  - `8`: Dark Gray (`#475569`)
  - `9`: Bright Blue (`#38bdf8`)
  - `10`: Bright Green (`#4ade80`)
  - `11`: Bright Cyan (`#22d3ee`)
  - `12`: Bright Red (`#f87171`)
  - `13`: Bright Magenta (`#e879f9`)
  - `14`: Yellow (`#facc15`)
  - `15`: White (`#f8fafc`)

---

## 6. Memory-Mapped I/O (MMIO) Registers (`0xFF00`–`0xFFFF`)

| Address | Constant | Mode | Description |
| :--- | :--- | :--- | :--- |
| `0xFF00` | `FORGE_MMIO_TIMER_LO` | R | CPU cycle counter low 16 bits (`cpu->cycles & 0xFFFF`). |
| `0xFF02` | `FORGE_MMIO_KEY_STATUS` | R | `1` if keyboard input queue has pending characters, else `0`. |
| `0xFF04` | `FORGE_MMIO_KEY_DATA` | R | Dequeues and returns the next ASCII character from the keyboard queue. |
| `0xFF06` | `FORGE_MMIO_HEAP_BRK` | R | Current dynamic heap break pointer (`cpu->os.heap_brk`). |
| `0xFF08` | `FORGE_MMIO_RANDOM` | R | Advances deterministic 16-bit PRNG and returns pseudo-random integer. |

---

## 7. ForgeOS System Calls (`OUT Rd, 3`)

| ID | Name | Arguments | Return (`R0`) | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0` | `FORGE_SYS_EXIT` | `R1` = exit code | `exit_code` | Terminates program execution cleanly (`cpu->halted = true`). |
| `1` | `FORGE_SYS_PRINT_INT` | `R1` = integer | - | Prints signed/unsigned integer to console output stream. |
| `2` | `FORGE_SYS_PRINT_CHAR` | `R1` = ASCII char | - | Emits ASCII character to console. |
| `3` | `FORGE_SYS_PRINT_STR` | `R1` = RAM pointer | - | Prints null-terminated string from RAM to console. |
| `4` | `FORGE_SYS_READ_CHAR` | - | `char` / `0` | Dequeues next ASCII key from keyboard queue. |
| `5` | `FORGE_SYS_KEY_AVAILABLE` | - | `0` or `1` | Returns `1` if input is pending in keyboard queue. |
| `6` | `FORGE_SYS_ALLOC` | `R1` = bytes | pointer | Allocates `R1` bytes on dynamic heap (`0x9000+`), returns base pointer or `0` on OOM. |
| `7` | `FORGE_SYS_FREE_ALL` | - | `0` | Resets dynamic heap break back to base `0x9000`. |
| `8` | `FORGE_SYS_DRAW_PIXEL` | `R1`=x, `R2`=y, `R3`=col | - | Plots pixel at `(x, y)` in VRAM with color `col & 0x0F`. |
| `9` | `FORGE_SYS_CLEAR_SCREEN` | `R1` = color | - | Fills 64×32 framebuffer (`0xF000..0xF7FF`) with color. |
| `10` | `FORGE_SYS_GET_PIXEL` | `R1`=x, `R2`=y | `color` | Reads pixel color at `(x, y)` from VRAM. |
| `11` | `FORGE_SYS_CYCLES` | - | `cycles` | Returns low 16 bits of CPU execution cycle counter. |
| `12` | `FORGE_SYS_RANDOM` | - | `rand16` | Advances PRNG and returns 16-bit pseudo-random value. |


