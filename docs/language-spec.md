# ForgeLang High-Level Language Specification

## 1. Overview

**ForgeLang** (`.fl`) is a structured, statically-checked imperative programming language designed for the Forge-16 computer architecture. The `forge-cc` compiler translates ForgeLang source programs through lexical analysis, recursive-descent parsing, AST construction, semantic/type analysis, Forge-16 assembly generation (with `@line <n>` source directives), and machine-code emission (`.fbin`).

---

## 2. Lexical Structure

- **Comments**: `//` or `#` to the end of the line.
- **Keywords**: `let`, `fn`, `return`, `if`, `else`, `while`, `for`, `break`, `continue`, `print`, `putc`, `in`.
- **Identifiers**: `[A-Za-z_][A-Za-z0-9_]*`.
- **Literals**:
  - Decimal integers (`0`, `42`, `-10`)
  - Hexadecimal integers (`0x002A`, `0xFF00`)
  - Character literals (`'A'`, `'\n'`)
- **Operators (in order of precedence, highest to lowest)**:
  1. Primary / Unary: `-x`, `~x`, `!x`, `(expr)`, `fn_call(args...)`, `in()`
  2. Multiplicative: `*`, `/`, `%`
  3. Additive: `+`, `-`
  4. Bitwise Shift: `<<`, `>>`
  5. Relational: `<`, `<=`, `>`, `>=`
  6. Equality: `==`, `!=`
  7. Bitwise AND / XOR / OR: `&`, `^`, `|`
  8. Short-circuit Logical AND: `&&`
  9. Short-circuit Logical OR: `||`
- **Statement Terminators**: Newlines or optional semicolons (`;`).

---

## 3. Grammar Summary

```ebnf
program        := statement*
statement      := var_decl
                | assignment
                | print_stmt
                | putc_stmt
                | if_stmt
                | while_stmt
                | for_stmt
                | break_stmt
                | continue_stmt
                | fn_decl
                | return_stmt
                | expr_stmt

var_decl       := "let" IDENT "=" expr
assignment     := IDENT "=" expr
print_stmt     := "print" expr
putc_stmt      := "putc" expr
if_stmt        := "if" expr block ("else" (if_stmt | block))?
while_stmt     := "while" expr block
for_stmt       := "for" "(" (var_decl | assignment)? ";" expr? ";" (assignment | expr)? ")" block
break_stmt     := "break"
continue_stmt  := "continue"
fn_decl        := "fn" IDENT "(" param_list? ")" block
return_stmt    := "return" expr
block          := "{" statement* "}"
```

---

## 4. Calling Convention, Stack Frame Layout (`FP` / `R7`), & Source Map

- **Global Variables**: Allocated in RAM starting at `0x8000` (2 bytes per 16-bit variable).
- **Function Calls**:
  1. Caller evaluates arguments left-to-right and pushes each 16-bit argument onto the hardware stack (`PUSH R0`).
  2. Caller invokes `CALL fn_<name>` (which pushes the 16-bit return `PC` and records a hardware call stack activation record).
  3. Callee Prologue:
     ```asm
     PUSH FP
     MOV FP, SP
     ```
  4. Stack Frame Layout relative to `FP` (`R7`):
     - `[FP + 4 + 2*(N - 1 - i)]`: Parameter `i` (0-indexed out of `N` parameters)
     - `[FP + 2]`: Return `PC`
     - `[FP + 0]`: Saved caller `FP`
     - `[FP - 2]`, `[FP - 4]`, ...: Local variables declared inside the function
  5. Callee Epilogue (`return expr`):
     - Evaluates return value into `R0`
     - Restores `SP` and `FP` and executes `RET`.
  6. Caller pops argument slots from the stack after `CALL` returns.
- **Source Mapping (`@line <n>`)**:
  - Every generated assembly block emitted for a high-level AST statement is preceded by `; @line <n>`.
  - The assembler and native C bridge map every `.fl` source line to its exact machine-code PC range (`startAddr..endAddr`) and assembly line range (`firstAsmLine..lastAsmLine`), enabling source-line breakpoints, `Step Line`, `Step Over`, and `Step Out`.

---

## 5. Built-in ForgeOS & Device Intrinsics

ForgeLang provides direct high-level access to the ForgeOS supervisor, dynamic heap, framebuffer, and virtual devices without external assembly wrappers:

| Intrinsic Function | Arguments | Return Value | Description |
| :--- | :--- | :--- | :--- |
| `alloc(bytes)` | `bytes: expr` | 16-bit address | Allocates `bytes` (word-aligned) on dynamic heap (`0x9000+`); returns pointer or `0` on OOM. |
| `free_all()` | none | `0` | Resets the dynamic heap break back to `0x9000`. |
| `peek(addr)` | `addr: expr` | 16-bit word | Reads 16-bit memory word directly from `[addr]`. |
| `poke(addr, val)` | `addr, val` | `val` | Writes 16-bit memory word `val` to `[addr]`. |
| `clear_screen(color)` | `color: 0..15` | - | Fills the 64×32 pixel virtual framebuffer (`0xF000..0xF7FF`) with `color`. |
| `draw_pixel(x, y, color)` | `x, y, color` | - | Plots pixel `(x in 0..63, y in 0..31)` with palette color `0..15`. |
| `get_pixel(x, y)` | `x, y` | `color: 0..15` | Returns palette color at `(x, y)` from the framebuffer. |
| `key_ready()` | none | `0` or `1` | Returns `1` if input is queued in keyboard queue, `0` otherwise. |
| `read_key()` | none | ASCII code | Dequeues and returns next ASCII key from keyboard queue (`0` if empty). |
| `cycles()` | none | 16-bit counter | Returns low 16 bits of the CPU execution cycle counter. |
| `rand()` | none | 16-bit random | Steps PRNG and returns pseudo-random integer. |
| `exit(code)` | `code: expr` | - | Halts the CPU cleanly with exit status `code`. |

