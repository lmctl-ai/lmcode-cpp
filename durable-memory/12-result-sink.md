# Owned program result values

The original return boundary supplied 24 imports and deep owned numeric/JSON
copies. Those remain unchanged, but framing and lifetime now follow the
[invocation-envelope amendment](13-invocation-envelope.md).

Each program slot has a distinct `ResultSink` spanning its body and selected entries.
Completion snapshots/seals it; successful reset is only an unfinished-state
operation before sealing. Failed writes are atomic. Controlled invocation
failure emits one failure envelope with no payload, including after partial
writes. There is no sink-only framing or unframed success reporter API.

Canonical standard metadata is std/lmcode-std.docs.json, generated reference is
[docs/std.md](../docs/std.md), schema locks remain std.lock and std/std.lock.
No imports, signature count, catalog version or bytecode version change in R1.

The original return fixture family retains its bytecode/HALT/source positions
and gains envelope expectations. Its compiler-library generator preserves the
original virtual header line. Invocation goldens additionally cover independent
slots, reverse dispatch/shared state, duplicates and controlled failures. Native
contract tests run without claiming compiler typed-fixture support (R2).
