# Architecture and decisions

## Public API

The C++17 header defines `dslvm::Error`, `Value` (void, int64, binary64, string, bool, or host object handle), `Args`,
`Registry`, `Context`, and `Program::load` / `Program::run`. The remaining helpers support file input, schema hashing, and the
standard host result protocol.

## Current bytecode format

Version 5 uses a big-endian fixed-width container with a schema SHA-256, tagged string constants, canonical function and method
imports, context declarations, typed local metadata, a compiler-measured maximum stack reservation, instructions, and an
optional source line table. Integers are signed 64-bit two's-complement values; floating-point values are IEEE-754 binary64.
The loader accepts version 6 only (v5 files are refused; R49 rule entries).

## Decisions

- The optional `stdlib/` directory is a copyable catalog, not VM or compiler machinery. Its JSON fragment and C++ binder describe
  ordinary host functions. Hosts may choose their own catalog and semantics.
- The loader resolves canonical import signatures once and retains pointers to registry entries. The same registry must outlive
  the loaded program and remain unchanged while it runs.
- Typed opcodes keep type selection in the compiler. The VM also checks value kinds defensively when executing malformed or
  mismatched bytecode.
- Integer addition, subtraction, multiplication, and negation wrap in two's-complement arithmetic. Division and remainder by
  zero are runtime errors; `INT64_MIN / -1` wraps and `INT64_MIN % -1` is zero.
- Structured range and while loops are verified as paired regions. Only their paired loop instructions can transfer execution
  backward. Other jumps are forward and must respect lexical loop boundaries. Break and continue compile to permitted forward
  transfers within their loop.
- There are no VM execution, stack, local, or string-size budgets. Bytecode is trusted input; process resource policy belongs to
  the host. `max_stack` is a reservation hint, not an execution limit.
- SDK and bytecode versions move together. The loader rejects every version other than 5; compatibility with earlier formats is
  intentionally unsupported.
- Registry entries declare full parameter and return types. Overloads with different parameter lists are supported; duplicate
  parameter lists are rejected. The sorted registry signatures can be compared with the compiler's canonical schema signatures.
- Host callbacks report failures by throwing `dslvm::Error`. CLI hosts can use the shared JSON result helpers; hosts choose their
  own application behavior and process-level policy.

While loops must reach their paired condition test before executing the body or exiting. Stack depth at each loop head and each
control-flow join must agree. These are static loader checks; no execution budget is added.
