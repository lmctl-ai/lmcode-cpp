# Invocation result sink — std slice 1

The canonical v3 standard now declares 24 `lmcode_return_*` imports. They are
host-bound callbacks, not VM opcodes: the host installs one invocation-owned
`ResultSink`, clears it before the next program, and keeps it alive across all
selected entry calls in that invocation.

The signatures are `lmcode_return_set(T)`, `lmcode_return_put(string,T)` for
the eleven value types in the canonical schema, plus `lmcode_return_null()` and
`lmcode_return_put_null(string)`. The sink owns a deep copy at assignment time;
later collection mutation or std arena reset cannot alter the captured value.

Slice 1 moves the canonical schema and docs first. The compiler's bundled std
catalog remains old until slice 3, so this is an intentional std-skew window:
do not publish an npm compiler between slices 1 and 3. Both `std.lock` copies are refreshed for this schema. Consumer sdk/suite locks
move in their assigned adopter slices; this repository owns neither lock.
No DSLB version change is implied.

Conversion validates UTF-8, finite numbers, safe mathematical integers, and
underflow before changing the prior sink value. Field writes require an object
or null root; JSON pointers escape `~` and `/`. Failed writes leave the sink
unchanged, and a failed invocation emits no result frame. Frame formatting uses
shortest-round-trip binary64 output (`std::to_chars` general), so `0.1 + 0.2`
is serialized as `0.30000000000000004`, never through `%.15g`.

The canonical generated reference is [docs/std.md](../docs/std.md); locks are
`std.lock` and `std/std.lock`.


The native binding API is `Registry::set_result_sink(ResultSink*)`; the pointer
is host-owned and must outlive the complete logical invocation. Separate
concurrent invocations require distinct sinks and the normal registry execution
discipline. `result_frame(sink)` returns LF bytes without I/O;
`write_result_frame(stream,sink)` requires an explicit LF-preserving stream
(binary stdout on Windows). Call it only after successful body/entry dispatch
and application validation, never on an error path.

`conformance/return-vectors` is an independently executed family, with a schema,
compiler-produced v6 bytecode and `vectors.lock`. It covers all 24 imports,
shortest float spelling, entry HALT values, repeated calls, null and failed
invocation silence. Its fault hosts frame too early or reset per entry;
`tests/test_return_controls.py` additionally compiles isolated SDK faults for
arena teardown, borrowed sources, missing sinks, raw numbers and `%.15g`.


Keep the registry/sink binding unchanged for the complete body-plus-entry
sequence. Registry copies share their standard runtime; they are not independent
concurrent invocation containers. Use separate registries/runtimes and sinks for
parallel invocations, or serialize a registry's complete logical invocations.
The explicit stream helper flushes before reporting success so buffered I/O
failures surface as errors.
