# Gotchas and limits

- The loader accepts bytecode version 6 only. Regenerate older fixtures with a compiler that emits version 6.
- `LOOP_BACK` must match its structural while-loop markers. Arbitrary backward edges are rejected. Range loops use their own
  paired instructions; break and continue are constrained to their lexical loop.
- There is no loop budget or VM resource cap. A nonterminating program can run indefinitely. Hosts may apply process-level
  limits where their product requires them.
- Hosts vendor `include/dslvm.hpp`; `sdk.lock` records the source commit and header SHA-256. `scripts/sync-sdk.sh --verify`
  detects a header that does not match the pin. To refresh a host, copy the header through its sync script and update the lock.
- The verifier checks structure, imports, local/context indexes, stack flow, and loop pairing. It does not perform a general
  type analysis of arbitrary bytecode. The VM retains defensive checks for value kinds and invalid host invocation.
- Context variables must be registered with their schema types and supplied with live objects in each `Context`. Object handles
  are raw pointers; hosts own their lifetime and must keep objects alive through execution.
- Range bounds are evaluated once. Loop counters and end values use hidden compiler-owned locals. Nesting has no VM-imposed depth
  cap; hosts remain responsible for process resources.
