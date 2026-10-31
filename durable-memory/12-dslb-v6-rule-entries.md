# lmcode bytecode (DSLB v6): rule entry table (2026-10-28)

Version 6 = version 5 bytes (byte-identical through the body line table) plus an `entries` section; the version field is
the only change for a file without rules (it ends with `u16 0`). **6 is the only accepted version**: v5 and all others are
refused at load with `unsupported bytecode version N (this runtime reads version 6)`.

## Format

After the body line table: `u16 entry_count`, then per entry: u32 name/annotation/interface constants (the interface text
is canonical `interface I(T1, ...) -> R`, parameter names omitted), u16 nparams, u16 nlocals + (u32 type constant,
u8 hidden) × nlocals, u16 max_stack, u32 code length + code ending in HALT, u32 nlines + (u32 pc, u32 line, u16 column) ×
nlines. Entries share the file's constant/import/context pools; PCs and jumps are region-relative. The file ends after the
last entry.

## Runtime decisions (include/dslvm.hpp)

- The loader validates per entry: constant indexes in range, `nparams <= nlocals`, `nparams` equals the interface's
  parameter count (`entry 'NAME' declares N parameter(s); interface I(...) -> R has M`), each parameter local's type equals
  the interface's (`entry 'NAME' parameter I has type T; the interface expects U`, I 1-based), and no parameter local is
  hidden (`entry 'NAME' parameter I is hidden`); then the existing verifier runs per region
  (`verify_region(code, locals, lines, loops, halt_depth)`) with `verify_stack` unconditional for entries (the no-loop
  early return applies only to the body, halt_depth −1). The extra rule: every reachable HALT leaves exactly 0 values for
  a `-> void` interface, 1 otherwise (`entry 'NAME': HALT leaves N values`). Entry verification failures are
  LOAD_BAD_BYTECODE re-thrown with an `entry 'NAME': ` prefix where the message does not already name the entry.
- `Program::Entry {name, annotation, iface, returns, params}`; params are the type names of local slots 0..nparams-1,
  returns is parsed from the interface text. `entries()` exposes them in source order.
- `call(entry, ctx, reg, args, stats)` stores args into slots 0..nparams-1 (kinds unchecked, R34) and runs the same
  private `execute()` as `run()`; the result is the stack value at HALT, `Value::none()` for void. Wrong arity or a bad
  index is RUN_HOST: `call entry 'increment' expects 1 argument(s); found 0`,
  `call entry index 5 out of range (2 entries)`.
- Conformance: `dsl-conformance/3`, 70 core vectors (new expect kinds `entries`, `call`, `call_error`, `call_error_at`;
  `load-other-version` pins a v5 file). The generator's `runtime-empty-lines-fallback` mutation was v5-shaped (sliced off
  the entries count); fixed upstream in the generator, not by hand. `runtime-type-error` was dropped by the v6 generator.
- Sample suite: schema gains `Score (Money) -> int`; cases 068 (item/total/never dispatch), 069 (value entries),
  093-v5-refused (mutation of the version field), compile-errors/rule-annotation-without-implements. The host calls
  `@item` entries once per item in order and `@total` once with `shop.total()`, prints non-void results as
  `entry NAME = VALUE`, and reports the call count as `"entries_called"` in the result object (the design's `"entries"`
  name was taken by the ledger entry count).
