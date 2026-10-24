# Stdlib catalog

The catalog is a set of ordinary host functions declared in a schema fragment and registered with the SDK's `register_*` helper. It adds
no opcodes and no VM behavior; a host may ship all of it, part of it, or its own functions. The reference implementation is
`stdlib/string.hpp` with `stdlib/string.schema-fragment.json`.

## Built-in lmcode std v1

The canonical public surface is `std/lmcode-std.schema.json`, a schema-2 fragment named `lmcode_std_v1` with `types` and `functions` (and an empty context). The built-in standard has exactly four areas: string (16 signatures), math (16), collections (35), and file read/write (5), totaling 72 canonical signatures. Testing, print, assertions, and domain helpers are not std. The optional testing fragment is separate and is not pinned as part of canonical std. It adds no VM opcodes or dependencies. The implementation-group fragments below remain compatibility fixtures, not independent standard definitions; a generated-surface parity check prevents drift from the canonical file.

All four SDKs copy the canonical file byte-for-byte from lmcode-cpp and pin its source commit, std version and SHA-256 in `std.lock`. Sync verification and CI check the actual bytes. The schema name carries version 1 because current compilers require a name and reject arbitrary metadata fields. The schema is exactly the surface a future compiler can inject. Compiler injection is not implemented here, and schema-hash binding continues to use the exact schema bytes supplied to compilation/loading.

### Default registration and opt-out

| SDK | Default registry | Explicit opt-out | Idempotent complete registration |
| --- | --- | --- | --- |
| C++ | `Registry()` | `Registry(StdMode::Disabled)` | `dslvm::register_std(registry)` |
| Rust | `Registry::new()` | `Registry::new_without_std()` | `register_std(&mut registry)` |
| Go | `NewRegistry()` | `NewRegistryWithoutStd()` | `RegisterStd(registry)` |
| C# | `new Registry()` | `new Registry(includeStd: false)` | `Std.RegisterStd(registry)` |

Default construction installs std without requiring an import, host schema declaration, host-owned arena or host registration call. C++ keeps the VM definitions in `include/dslvm.hpp` and loads `stdlib/std.hpp` at the end; the aggregator includes the implementation-group headers. Hosts vendor the VM and every `stdlib/*.hpp` together through their pinned sync script. Standalone inclusion of an individual catalog header remains supported. Go must keep default registration in the core package (compatibility catalog packages can re-export it) to avoid a core-to-catalog import cycle.

Existing C++ group binders remain available: manual registration into an opted-out registry preserves their existing behavior. Calling a built-in group binder after default std registration is idempotent. Testing remains explicitly registered and declared by a host; it is never installed by register_std. New hosts use default std and remove group-registration code. A host may not replace an existing built-in parameter tuple with another implementation or return type; ordinary duplicate registration is still an error.

### Signatures and schema self-check

`signatures()` and the default schema-summary text expose the complete runtime surface, std included. `host_signatures()` exposes only host registrations; `std_signatures()` exposes the installed built-ins. Their language-equivalent accessors follow the same rule. Opted-out registries have no installed std signatures until explicitly enabled.

`matches_schema_signatures(schema_signatures)` compares the complete registry with the sorted set union of the compiler's schema signatures and installed std signatures. Missing host declarations, extra declarations and incompatible return types still fail. Missing built-in declarations alone do not fail. The canonical std names/parameter tuples determine built-in membership, rather than a name prefix; host functions such as `str_text_prefix` remain host declarations.

`scripts/merge-std.py HOST_SCHEMA OUTPUT` (also available through the compatibility name `merge-std-schema.py`) is the temporary compiler bridge: it merges the canonical fragment into a full host schema, preserves context/domain metadata and overloads, and refuses conflicting parameter tuples/return types. It also supplies signature normalization and host-only schema extraction modes. Already-merged schemas are accepted idempotently. The helper reads a vendored `cpp/std/lmcode-std.schema.json` in host checkouts, or the canonical `std/` file in the SDK. It does not modify the input file or infer adapters.

Until compiler injection is released, migrated hosts keep only host declarations in a checked-in source schema and generate the effective merged schema during their build. The existing compiler-facing schema path can remain a generated, ignored file so examples continue to compile after building. `--dsl-schema --json` prints the exact effective embedded schema bytes today; the loader hashes those same bytes. Future injection can use the host-only schema bytes instead; default std import resolution already works with either form. Signature self-checks normalize std membership, so they accept both forms without hiding host/schema drift.

### Collection lifetime and native adapters

A default registry owns its std collection arena. A run starts with an empty arena and clears it on success or exception; list/map handles are valid only during that run. All four collection types share the existing 65,536-allocation budget. Clearing an object does not reclaim an allocation slot. Registry copies sharing runtime state cannot execute overlapping/nested runs; SDKs must reject such reuse rather than invalidate live handles. The guard is shared and atomic; a loaded program guards its bound standard state even when the run call supplies another registry. Separate registries can execute independent runs.

C++ native callbacks create lists/maps through `registry.std_collections().make_*` while a run is active. Host-owned context values can be exposed through a small value-copy adapter; a copied list is an independent snapshot and mutations do not modify its source. The existing explicit `Collections` binder remains a manual/opt-out compatibility API, but default std no longer needs a host arena member. `foreach` continues to use the built-in `len` and `at` methods.

Adapters must remain small and preserve ownership and meaning. A type needing state sharing, a different lifetime or different mutation semantics remains a host type under its own name (or the host opts out); std is not expanded to emulate it. HTML/header-name snapshots fit the common lists. DOM node/document lists, physics matrices and SSH ResultList remain domain objects.

`print` and `assert_close` belong to the optional testing catalog, not std. Hosts retain their own declarations and explicit registration. Finite-only domain variants and their diagnostics remain separate host functions; no migration of the remaining assertion helpers is planned.

## File read/write catalog (`file_*`)

`stdlib/file.hpp` and its compatibility fragment implement five built-in functions using only OS/standard-library facilities. They add no sandbox or root checks. Relative paths resolve against the process cwd, symlinks are followed, and file contents are always raw bytes: no encoding conversion or newline translation.

| Function | Exact behavior |
| --- | --- |
| `file_read(path) -> string` | Read the complete regular file, including empty content, NUL and invalid UTF-8 bytes. |
| `file_write(path, bytes) -> void` | Create or truncate a regular file, write all bytes, check flush/close errors. |
| `file_append(path, bytes) -> void` | Create if missing; otherwise append all bytes, checking flush/close errors. |
| `file_exists(path) -> bool` | True for a regular file when metadata can be read, even if its content is unreadable. False for empty/missing paths, broken symlinks, directories and other nonregular objects. Metadata errors other than missing/not-a-directory trap. |
| `file_lines(path) -> StrList` | Read bytes then split on every LF byte, preserving empty fields, CR bytes and a final empty field: empty file gives `[""]`, `"a\n"` gives `["a", ""]`, `"\n"` gives `["", ""]`. The resulting list uses the registry-owned arena. |

Read/write/append/lines reject directories and other nonregular objects. Every failure traps with `RUN_HOST` and base message `FUNCTION: PATH: REASON`, e.g. `file_read: missing.bin: No such file or directory`. PATH is the original path bytes in byte-capable diagnostics. Rust and C# expose text error messages, so invalid-UTF-8 paths use the reversible spelling `bytes:<lowercase hex of the complete path>`; valid UTF-8 paths remain unchanged. This diagnostic representation never changes the path passed to the OS. REASON is the OS reason and is platform-dependent. Tests compare the function/path prefix and a nonempty reason, not localized OS wording. Short writes, read errors and flush/close errors trap; writes are not transactional and a failed write can leave a partial/truncated file. No arbitrary content-size cap is imposed.

Paths are native OS paths used as given. Unix passes path bytes unchanged; the native filesystem can reject a name (for example APFS rejects invalid UTF-8 with EILSEQ), which traps with the OS reason. Windows interprets VM path bytes as strict UTF-8, converts to UTF-16 and uses wide OS APIs; absolute drive/UNC paths use the extended-length prefix when required, and long relative paths are resolved against cwd first. Invalid UTF-8 on Windows and embedded NUL on every OS trap with the corresponding invalid-argument/OS conversion reason instead of silently truncating. The content byte-string contract is independent of path encoding.

Shared file vectors use a test-only `test_temp_dir() -> string` callback, declared in their manifest schema rather than std. Each vector runner creates a fresh temporary directory and cleans it after the case. The 16 file vectors cover empty/NUL/non-UTF-8 content, >1 MiB data, append, truncation, LF splitting, missing paths, directories and an unwritable path. The test-only `test_long_path()` fixture creates nested directories and returns an ordinary absolute Unicode path longer than 260 characters; Windows fixture setup and cleanup use extended paths independently of the catalog being tested. An existing regular file used as an intermediate directory component provides a deterministic unwritable-path failure even when the test process is privileged. Optional testing callbacks remain explicitly registered by the vector runner.

## Open items / left in hosts and why

- Printing and assertions stay in host schemas or the optional testing catalog: output routing, flushing, tolerance rules and finite-domain checks differ. The other 13 assertion hosts are not migration targets.
- DOM/document/node lists, SSH ResultList and physics/matrix objects remain domain types because their ownership or semantics do not match value collections.
- Native file wrappers with root restrictions, stream state, special encoding or domain-specific error handling remain host APIs; std offers only unrestricted byte file operations.
- Compiler-side implicit schema injection is separate future work; current hosts generate effective schemas using the pinned merge helper.
- File OS reason wording and NaN payloads are platform-specific; shared tests pin portable behavior rather than those representations.
- CSV/JSON catalogs are future work only. They can later build on StrList, StrMap and byte-clean strings to exchange data between tools; no format parser or dependency is introduced now.

## Byte-string rule

VM strings are byte sequences in every SDK: lengths and offsets are bytes, no decoding or validation happens, NUL bytes are legal,
and invalid UTF-8 survives constants, concatenation, equality and host callbacks. Catalog functions therefore operate on bytes.
`str_lower` / `str_upper` change ASCII letters only; `str_substr` can cut a multibyte character. Host-facing helpers that need text
convert explicitly (validated or lossy). Rust follows the same rule with a byte-backed string value.

## String functions (`str_*`)

| Function | Semantics |
| --- | --- |
| `str_len(s) -> int` | byte length |
| `str_starts_with(s, p)`, `str_ends_with(s, p)`, `str_contains(s, n) -> bool` | byte comparison; empty argument is always true |
| `str_find(s, n) -> int` | first byte offset, or -1 |
| `str_substr(s, start, count) -> string` | byte offsets; start < 0 clamps to 0; start >= len or count <= 0 gives empty; count clamps to the remaining bytes |
| `str_lower`, `str_upper` | ASCII `A-Z` / `a-z` only |
| `str_trim` | strips bytes ` ` and `\t`..`\r` from both ends |
| `str_repeat(s, n)` | error if n < 0 or the result exceeds 64 MiB (67108864 bytes); empty `s` with n >= 0 gives empty |
| `str_replace(s, from, to)` | all non-overlapping occurrences left to right; empty `from` is a no-op; error if the result exceeds 64 MiB |
| `str_is_int(s)`, `str_to_int(s)` | optional single leading `+` or `-`, decimal digits only, must fit int64; `str_to_int` returns 0 when not an int |
| `str_is_float(s)`, `str_to_float(s)` | decimal syntax `[+-]digits[.digits][e[+-]digits]` (digits before or after the dot), no whitespace/hex/nan/inf; C locale; overflow or underflow-to-zero is not a float; `str_to_float` returns 0.0 when invalid |
| `str_match(text, pattern) -> bool` | glob over bytes: `*`, `?`, `[abc]`, `[a-z]`, `[!x]`, backslash escape; unterminated `[` is literal, `]` first in a class is literal, descending ranges are empty; error after 2^27 work steps |

`stdlib/string.hpp` is the exact specification for edge cases; ports must reproduce it byte for byte.

## Adding a function in all four SDKs

1. Add the signature to the C++ fragment (`stdlib/string.schema-fragment.json`) and `tests/string.schema.json` (they must be identical).
2. Implement it in `stdlib/string.hpp` and register it in `register_string`.
3. Port it: `lmcode-rust/dslvm/src/stdlib.rs` + `lmcode-rust/stdlib/` fragment, `lmcode-go/stdlib`, `lmcode-csharp/stdlib` (fragment + binder).
4. Run each SDK's signature-parity check against `lmscript --dsl-signatures` (sorted registry signatures must equal the compiler's output).
5. Add a stdlib conformance vector (below) and make every SDK pass it.
6. Update each README's catalog line and this table.

## Stdlib conformance vectors

`conformance/stdlib-vectors/` holds language-neutral cases: `NAME.dsl` (source), `NAME.dslb` (bytecode compiled by `lmscript` against the
catalog schema), `NAME.expect` (expected output or error) and a manifest entry. The runner registers the catalogs selected by each schema and compares
results, so every SDK that ships a catalog function must pass the vectors that use it. Failure errors use the same codes as the main vectors.

## Math functions (`math_*`)

`stdlib/math.hpp` and `stdlib/math.schema-fragment.json` define eight ordinary host function names, each overloaded for `int` and `float` (16 canonical signatures). Unary functions return their argument type; binary functions take two arguments of the same type and return that type. Call `dslvm::stdlib::register_math(registry)` once. The prefix avoids compiler built-in names; these functions add no opcodes or implicit numeric conversions.

| Name | int overload | float overload |
| --- | --- | --- |
| `math_abs` | two's-complement wrapping absolute value; `INT64_MIN` remains `INT64_MIN` | absolute value, clearing the sign of zero |
| `math_min`, `math_max` | signed comparison | either NaN propagates; equal operands, including signed zeros, return the first argument |
| `math_floor`, `math_ceil` | identity | round toward negative/positive infinity; preserve signed zero |
| `math_round` | identity | nearest integer-valued binary64, halfway away from zero; preserve signed zero |
| `math_pow` | exponentiation by squaring modulo 2^64; exponent must be nonnegative; `0^0 = 1` | IEEE/libm power, with the NaN exceptions below |
| `math_sqrt` | exact floor square root; argument must be nonnegative | IEEE square root; negative values produce NaN, `-0` remains `-0` |

Integer domain errors have code `RUN_HOST`, with exact messages `math_pow: integer exponent must be nonnegative` and `math_sqrt: integer argument must be nonnegative`. Integer floor/ceil/round never convert through floating point. Integer square root uses integer comparisons without squaring overflow; values adjacent to large perfect squares remain exact.

For float powers, any base to either signed zero exponent and base `+1` to any exponent return `1`, including NaN operands. Base `-1` to an infinite exponent also returns `1`. Other NaN inputs produce NaN. Finite negative bases with non-integral finite exponents produce NaN; signed-zero powers retain the IEEE odd-integer sign rules. Overflow produces signed infinity and underflow produces signed zero or a representable subnormal. Float operations retain infinity and NaN classification; NaN payload and sign are not part of the catalog contract. Finite non-exact libm results use the shared 1e-12 relative trace tolerance, rather than requiring cross-platform bit identity.

The 11 `math-*.dsl` vectors in `conformance/stdlib-vectors/` use `math.schema.json` and compiler-emitted bytecode. They cover every overload, wrapping arithmetic, exact large integer roots, ties-away rounding, NaN rules, signed zero and located domain errors. Together with the 18 string vectors, the string/math portion contains 29 vectors.

## Collections catalog

Collections are ordinary schema object types with methods; `len()` and `at(i)` make them `foreach`-able. No compiler or VM change.
A fragment (`stdlib/collections.schema-fragment.json`) declares `types` and `functions`; each SDK ships `register_collections`.

Common rules (every list and map type):
- Elements are values by kind: `StrList` holds byte strings, `IntList` int64, `FloatList` binary64. Insertion order is preserved.
- Index arguments are 0-based; an out-of-range `at`/`set` is a host error `"<Type>.<method>: index out of range: <i>"` (code `RUN_HOST`).
- C++ and Go bind a `Collections` arena to the registry; hosts may reset it between runs only after old handles expire. Go creates a private arena when none is supplied. Rust and C# store collection values in each run's `Context` arena. Handles must not be shared between runs.
- The shared collection budget is 65536 allocated objects across all four types, excluding unrelated host context objects. The next allocation raises `"<Type> object limit exceeded (65536)"` with code `RUN_HOST`. Clearing a collection does not reset this budget.
- Mutators return the receiver (`returns: "<Type>"`) so calls chain. `clear()` keeps the object valid.

| Type | Methods |
| --- | --- |
| `StrList` | `len() -> int`, `at(int) -> string`, `add(string) -> StrList`, `set(int, string) -> StrList`, `contains(string) -> bool`, `index_of(string) -> int` (-1 if absent), `join(string) -> string`, `clear() -> StrList` |
| `IntList` | `len`, `at(int) -> int`, `add(int)`, `set(int, int)`, `contains(int)`, `index_of(int)`, `clear()` |
| `FloatList` | same as `IntList` with `float`; `contains`/`index_of` use `==` (NaN never matches) |
| `StrMap` | `len() -> int`, `has(string) -> bool`, `get(string) -> string` (missing key: host error `"StrMap.get: missing key"`), `get_or(string, string) -> string`, `set(string, string) -> StrMap` (existing key keeps its position), `key_at(int) -> string` (insertion order), `remove(string) -> bool` (later keys shift up), `clear() -> StrMap` |

Constructors: `str_list()`, `int_list()`, `float_list()`, `str_map()` return a new empty object.
`str_split(string, string) -> StrList`: splits on every non-overlapping occurrence of a non-empty separator, left to right, byte-wise; `"a,,b"` gives `["a","","b"]`,
`""` gives `[""]`, an empty separator is a host error `"str_split: separator must not be empty"`. No numeric `join` (float formatting is not part of the collection contract).

## Optional testing catalog (not std)

`print(string) -> void` writes the bytes plus `\n` to the host's output sink (stdout by default; the sink is replaceable so vectors can capture it).
`assert_close(float, float, float) -> void` is a host error `"assert_close failed: <a> vs <b> (tol <t>)"` (floats formatted with 15 significant digits in the C locale, independent of the host process locale) unless `|a-b| <= tol`; NaN on either side fails. Diagnostics spell every NaN `nan`, regardless of sign or payload, matching all four SDKs.

## Host adoption

C++ hosts vendor `include/dslvm.hpp` plus the stdlib headers they use (`stdlib/string.hpp`, `collections.hpp`, `math.hpp`, `testing.hpp`) through `scripts/sync-sdk.sh`,
with every vendored file pinned (path, sha256) in `sdk.lock`. The host schema embeds the fragment entries unchanged; a host with a narrower type keeps it under its own name.

C++ uses `dslvm::stdlib::Collections` with stable-address `std::deque` storage per type. Native hosts create objects through `make_str_list(values)`, `make_int_list(values)`, `make_float_list(values)` and `make_str_map()` so host-returned collections obey the same shared arena limit. List elements are in `.items`; maps store insertion-ordered key/value pairs in `.items`. `reset()` invalidates every handle and is permitted only between runs after all prior handles have expired. Collection `clear()` methods remove elements without freeing an arena slot or invalidating their receiver.

## Collection/testing integration in the SDKs

| SDK | Collections binder | Testing binder / sink |
| --- | --- | --- |
| C++ | `register_collections(registry, arena)` | `register_testing(registry, ostream*)`; null selects stdout |
| Go | `stdlib.RegisterCollections(registry, arena)`; arena optional | `stdlib.RegisterTesting(registry, io.Writer)`; nil selects stdout |
| C# | `CollectionsCatalog.RegisterCollections(registry)` | `TestingCatalog.RegisterTesting(registry, Stream)`; null selects stdout |
| Rust | `stdlib::register_collections(&mut registry)` | `stdlib::register_testing(&mut registry, Option<Sink>)`; None selects stdout |

Go exports collection types with snapshot `Values()`/`Entries()` accessors;
C# exposes read-only `Values`/`Entries` views. Rust and C++ collection structs
expose their item storage to host callbacks. Strings remain bytes at these boundaries.

The shared suite contains 77 vectors: 18 string, 11 math, 32
collection/optional-testing cases, and 16 file cases. `runtime_error_exact` pins error codes and complete
base messages after removal of an SDK source-location prefix. Captured print
output is compared as hexadecimal bytes. The assertion comparison is exactly
`abs(a-b) <= tolerance`: equal infinities fail because their difference is NaN;
an infinite tolerance can accept an infinite difference.
