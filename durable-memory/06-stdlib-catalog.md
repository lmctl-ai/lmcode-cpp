# Stdlib catalog

The catalog is a set of ordinary host functions declared in a schema fragment and registered with the SDK's `register_*` helper. It adds
no opcodes and no VM behavior; a host may ship all of it, part of it, or its own functions. The reference implementation is
`stdlib/string.hpp` with `stdlib/string.schema-fragment.json`.

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

## Testing catalog

`print(string) -> void` writes the bytes plus `\n` to the host's output sink (stdout by default; the sink is replaceable so vectors can capture it).
`assert_close(float, float, float) -> void` is a host error `"assert_close failed: <a> vs <b> (tol <t>)"` (floats formatted with 15 significant digits in the C locale, independent of the host process locale) unless `|a-b| <= tol`; NaN on either side fails.

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

The optional suite now contains 61 vectors: 18 string, 11 math, and 32
collection/testing cases. `runtime_error_exact` pins error codes and complete
base messages after removal of an SDK source-location prefix. Captured print
output is compared as hexadecimal bytes. The assertion comparison is exactly
`abs(a-b) <= tolerance`: equal infinities fail because their difference is NaN;
an infinite tolerance can accept an infinite difference.

## Built-in lmcode std v1

The default runtime surface is the union of the four catalogs above: 69 canonical
function/method signatures. `std/lmcode-std.schema.json` is the single versioned
source of truth for that surface. SDK copies are byte-identical, not independently
formatted or regenerated from local registrations. Each copy has a `std.lock`
recording the C++ source revision and SHA-256 of the exact bytes. A sync script
copies it and supports `--verify`; native tests and CI verify the pin and compare
registered standard signatures against the canonical declarations.

The individual `stdlib/*.schema-fragment.json` files and explicit binders remain
compatibility interfaces. They must agree with the corresponding canonical
subset; additions to std change the canonical file/version deliberately.

### Construction and opt-out

| SDK | Default-on construction | Empty/opt-out construction | Explicit standard binder |
| --- | --- | --- | --- |
| C++ | `Registry()` | `Registry(false)` | `stdlib::register_std(registry)` |
| Go | `NewRegistry()` | `NewRegistryWithoutStd()` | `RegisterStd(registry)`; `stdlib` compatibility forwarding is allowed |
| C# | `new Registry()` | `new Registry(false)` | `Stdlib.StandardCatalog.RegisterStd(registry)` |
| Rust | `Registry::new()` and `Default::default()` | `Registry::without_std()` | `stdlib::register_std(&mut registry)` |

Constructing the ordinary registry installs std without an extra import, host
callback, or host-owned collection arena. In particular, importing only Go's root
`lmcode` package is sufficient. The explicit standard binder is idempotent.
Legacy explicit subset binders remain usable on a default registry, including
selecting a testing sink. Ordinary duplicate host declarations are still errors.
Built-in fallback declarations may be replaced by a compatible explicit host
registration; signature checking cannot establish callback semantic equivalence.
A legacy host type with incompatible collection semantics should use an opt-out
registry or a distinct schema type, not silently share the built-in type name.

### Signatures and schema consistency

`signatures()` / `Signatures()` include all enabled built-ins and host entries.
A host-only accessor (`host_signatures` / `HostSignatures`) excludes canonical
standard signatures. Textual schema summaries likewise describe the full enabled
surface; the host-only accessor is available to tools that need application-only
declarations. There is no hidden name-prefix heuristic: membership is determined
by the canonical signature set, including parameters and return types.

The schema consistency API (`check_schema_signatures` / `CheckSchemaSignatures`)
compares the registry's complete signature set with the supplied schema signature
set UNION the enabled standard set. Thus a host-only schema and the same schema
with std merged explicitly both pass. Missing/extra host signatures and changed
parameter/return signatures still fail with `LOAD_UNBOUND`. An opt-out registry
normalizes against an empty built-in set until std is explicitly installed.

This normalization concerns declarations only. Loader SHA-256 binding remains
exact and is never weakened. Before compiler-side injection exists, the build
must merge std into the compiler schema and embed those exact merged bytes in the
host. A small merge helper accepts a host schema and the canonical std fragment,
merges types/functions while preserving host metadata, accepts identical repeated
entries, and rejects conflicting declarations instead of overwriting them.
After compiler injection lands, handwritten host schemas can omit std; the same
signature consistency rule already accepts that representation.

### SDK-owned collection lifetimes

Default registration owns the collection state. C++ and Go registry-backed arenas
are reset by the SDK at the outermost program-run boundary; nested calls sharing
a registry must not invalidate the outer run's handles. Objects remain usable for
host inspection until the next run. Explicit legacy arenas retain their existing
manual lifecycle. Concurrent runs must use independent registry-owned arenas
unless the SDK explicitly supplies synchronization/isolation.

Rust and C# keep collection values and their allocation budget in the run's
`Context`; a fresh context provides a fresh budget and preserves host access to
results until that context is dropped. In all cases, unrelated context objects do
not consume the collection quota, clearing a collection does not reclaim a slot,
and no host-owned collection arena is required for the default path.

### Migration boundaries

Small adapters may expose an SDK value through an existing compatible host
signature. Adapters must not change byte-string semantics, mutation/ordering,
quota behavior, or ownership. State-sharing or lifetime translation that needs a
substantial adapter is a reason to keep that host's implementation (and, where
needed, opt out of or rename the conflicting standard type). Existing explicit
registration and arena APIs remain compatibility paths for migrated native hosts.
Compiler injection and unrelated host feature changes are outside this runtime
transition.
