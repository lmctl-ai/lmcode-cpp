# Canonical invocation envelopes (R1)

The amendment supersedes bare payload/per-program framing. Construct one native
`InvocationResults::from_command(parsed_files)` after CLI parsing/introspection.
It reads LMCODE_INVOCATION_FILE when present; an explicitly supplied native
optional path overload supports embedding/testing. The environment is read,
never modified. Nested language executions must replace inherited metadata.

`registrations()` preserves parser order and supplies each unchanged file string
with its canonical slot index. Manifest indices are contiguous add order; file
strings must be absolute and unique, and registrations a complete permutation.
Without metadata, one occurrence is single; several are builder mode in CLI order,
including duplicate file occurrences. Do not deduplicate these records.

Call `select(index, registry, contract)` before the body and again for interleaved
entry work. Active selection retains its sink. Each slot has a different owned
ResultSink; complete validates/snapshots/seals it. A completion validation failure latches
its slot/run association before rethrowing. Contracts cannot change on
reselect. Sealed sinks refuse routing/writes/reset/completion. After a failure,
new slots cannot start, but already-started work may drain; the first failure wins.
The registry/collector lifetime and synchronization discipline remain host-owned.
Serialize collector bookkeeping calls; parallel VM work uses distinct slots and
registries and must drain before the writer. No collector method waits for work.

`ResultContract::from_schema(raw_bytes)` reads schema 2's optional result object.
Types/nullability match the approved amendment. Known native writes preserve type
identity except int->float and IntList/IntMap->FloatList/FloatMap widening without
conversion. A Json handle defers to actual JSON shape. A declared Json root permits all
supported non-null JSON shapes without conversion, or root null when nullable.
This matches the concurrent R2 compiler's JSON-wide contract rule. Json field
construction accepts JSON values.
Map elements cannot be null even when its root is nullable. Initial/reset null is
unfinished; explicit null and completion validate immediately. No declaration is
unconstrained. Copies validate before replacing state. The final payload shape is
revalidated independently at completion.

After all work and global application validation, call write_frame(out) once.
Use fail(optional_index, phase, error) for controlled failures; builder-only part
association and the first phase/code/location are retained. Missing completion is
a failure, never a silent null. One-slot builder remains an array with parts:1;
single list payload remains single. A failing aggregate never has result/parts.
Global `failure_frame(out, phase, error)` covers errors before collector creation;
return its exit status immediately. It has no process-global invocation state.

The single-shot collector never writes twice, even after flush/write failure.
Success returns 0; cli/load-or-run/app failure returns 2/3/4. Output failure returns
4 without appending another frame. The frame wraps the compact envelope and ends
in LF. Exact float formatting and JSON numeric/ownership validation stay canonical.
Sink-only framing and unframed report helpers are removed; no compatibility alias.

R1 intentionally precedes compiler R2 and host R6. No release during the mixed
protocol window. The canonical std stays v3 / 206 signatures / 24 return imports,
and bytecode stays DSLB v6. Typed bytecode fixtures belong to R2; R1 native contract
checks do not pretend the older compiler supports result metadata.

Windows adapters supply UTF-8 argv (convert the wide CRT argv without splitting)
and binary stdout. The SDK reads the wide environment variable and converts its
manifest path to UTF-8 before filesystem access. Native goldens include spaces
and Unicode in private slot filenames. The environment is never mutated.
