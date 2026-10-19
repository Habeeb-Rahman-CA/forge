# Forge-16 Native C Bridge API Specification

## 1. Overview

The **Forge Native Bridge (`forge-bridge`)** is a lightweight, zero-dependency C11 HTTP/JSON server built on POSIX sockets and linked directly against `libforge_core.a` (the C CPU emulator), `libforge_asm_lib.a` (the C assembler), and `libforge_compiler_lib.a` (the ForgeLang compiler).

It keeps the C emulator, assembler, and compiler completely independent of Angular while allowing the Angular visual debugger to drive real hardware state transitions and source-level debugging over `http://127.0.0.1:8080/api/*`.

---

## 2. Endpoints

| Method | Path | Description |
| :--- | :--- | :--- |
| `GET` | `/api/state` | Returns complete CPU register/flag state, disassembly, memory window, stack words, watches, trace history, console output, source map (`sourceMap`), active `.fl`/`.asm` lines (`activeSourceLine`, `activeAsmLine`), live global variables (`globals`), and live call stack frames (`callStack`) with local/parameter values. Query params: `memStart`, `memLen`, `watch`. |
| `POST` | `/api/assemble` | Assembles assembly source text via `forge_assemble_source()`, resets the CPU, loads the generated `.fbin` image via `forge_cpu_load_fbin()`, and returns the updated state + any line-numbered syntax diagnostics. |
| `POST` | `/api/compile` | Compiles ForgeLang (`.fl`) source code via `forge_compile_source()`, assembles the emitted assembly, populates the bi-directional source map and symbol metadata, loads the `.fbin` image, and returns the updated state including `generatedAsm`, `astText`, `sourceMap`, `globals`, and `callStack`. |
| `POST` | `/api/step` | Executes instruction or source-level steps (`mode`: `"instruction"`, `"line"`, `"over"`, `"out"`) and returns the updated CPU state and register deltas. |
| `POST` | `/api/run` | Runs the CPU via `forge_cpu_run()` until `HALT`, breakpoint, fault, or `maxSteps` and returns the updated state. |
| `POST` | `/api/reset` | Resets registers, flags, `PC`, `SP`, call stack, trace, and console, reloading the current assembled/compiled `.fbin` image. |
| `POST` | `/api/breakpoint` | Toggles, adds, removes, or clears hardware execution breakpoints in `forge_cpu_t` by `address` or by high-level `sourceLine` (resolved via the active source map). |
| `POST` | `/api/memory/write` | Edits a permitted 8-bit or 16-bit memory location via `forge_mem_write8()` / `forge_mem_write16()`. |
| `POST` | `/api/io/input` | Pushes ASCII text or a 16-bit integer value into the CPU's virtual I/O input queue. |

---

## 3. Response Schema Additions (Milestone 6)

Every state response includes real-time telemetry from the virtual framebuffer and ForgeOS supervisor:

```json
{
  "framebuffer": {
    "width": 64,
    "height": 32,
    "vramStart": 61440,
    "fbWrites": 128,
    "pixelsHex": "0000... (2048 hex characters for 64x32 pixels, 0..F palette)"
  },
  "os": {
    "heapStart": 36864,
    "heapBrk": 36872,
    "heapLimit": 57344,
    "heapUsedBytes": 8,
    "allocCount": 1,
    "syscallCount": 3,
    "lastSyscall": 0,
    "exitCode": 42,
    "inputQueueCount": 0
  }
}
```

