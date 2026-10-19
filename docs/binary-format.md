# Forge-16 Executable Binary Format (`.fbin`)

## 1. Supported Loader Modes

The Forge-16 program loader supports two binary formats:

1. **Raw Machine Code Image**:
   - A raw sequence of 32-bit (4-byte, little-endian) instructions and data loaded directly at a specified base address (default `0x0400`), with `PC` initialized to the load address.
2. **Forge Executable Header Format (`.fbin`)**:
   - A structured 16-byte header followed by the payload bytes.

---

## 2. `.fbin` Header Layout (16 Bytes, Little-Endian)

| Offset | Size | Field | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | 4 B | `magic` | ASCII magic bytes `"FRG1"` (`0x46`, `0x52`, `0x47`, `0x31`) |
| `0x04` | 2 B | `version` | Format version (`0x0001`) |
| `0x06` | 2 B | `load_addr` | Target memory load address (default `0x0400`) |
| `0x08` | 2 B | `entry_pc` | Initial Program Counter address (default `0x0400`) |
| `0x0A` | 2 B | `code_size` | Byte length of code/data payload following the header |
| `0x0C` | 2 B | `flags` | Reserved flags (`0x0000`) |
| `0x0E` | 2 B | `checksum` | Simple 16-bit additive word checksum or `0x0000` |
| `0x10` | `code_size` B | `payload` | Machine code and static data bytes |

