# lmcode-cpp

The C++ runtime for lmscript `_lmcode_` bytecode: loader, verifier, VM and host registry in one header, `include/dslvm.hpp`.
C++17, standard library only, no dependencies. This repository is the single source of truth for the header; hosts vendor it
by pinned hash (see "Consuming it").

## Status

Experimental. The current runtime reads bytecode version 6 only; older and newer versions are refused. Use a matching
`lmscript` compiler from the published `@lmctl-ai/lmctl` npm package. The published compiler requires Node.js >=24.15.0.

## Build

The SDK is header-only. Include `include/dslvm.hpp` and compile a C++17 host; no library build or external dependency is needed.
See [Embedding it in twenty lines](#embedding-it-in-twenty-lines) for a minimal host.

## What it is

lmscript compiles a `_lmcode_` block (a small strongly typed configuration language whose names come from a JSON schema) into a
portable bytecode file (`savedsl`). A host program written in C++ embeds this header, implements the schema's objects and
functions in a `dslvm::Registry`, loads the file with the exact schema bytes, and runs the chunk once per event with a
`dslvm::Context` of live host objects. The host owns the process, the objects and their lifetime; the chunk only calls what
the registry offers. lmscript never executes the DSL; this SDK never reads the schema's content, only its bytes (for the
SHA-256 binding).

## Gallery

| **Bytecode Conformance Suite** | **Unit Tests & Invariants** |
| --- | --- |
| <img src="docs/images/conformance.png" width="440" alt="Bytecode Conformance Suite"> | <img src="docs/images/unit-tests.png" width="440" alt="Unit Tests & Invariants"> |
| All 70 core bytecode vectors pass across opcodes, types, rule entries, error paths, and control flow.<br>`make conformance` | 115 unit tests verifying VM type invariants, line tables, and registry semantics.<br>`make test` |

| **Standard Library Conformance** | **Same Bytecode, Four Runtimes** |
| --- | --- |
| <img src="docs/images/stdlib-conformance.png" width="440" alt="Standard Library Conformance"> | <img src="docs/images/same-bytecode-four-runtimes.png" width="440" alt="Same Bytecode, Four Runtimes"> |
| 118 catalog vectors covering collections, math, string, file, and optional testing.<br>`build/conformance conformance/stdlib-vectors` | Cross-language parity: C++, Rust, Go, and C# run the exact same bytecode.<br>`PASS 51/51 vectors across 4 runtimes` |

## Embedding it in twenty lines

```cpp
#include "dslvm.hpp"
struct Greeter { std::string last; };

int main(int argc, char** argv) {
  dslvm::Registry reg;
  reg.context("g", "Greeter");                                                       // the schema's context variable and its type
  reg.method("Greeter", "say", {"string"}, "void", [](void* self, dslvm::Args& a) { // Greeter.say(string) -> void, declared as in the schema
    static_cast<Greeter*>(self)->last = a[0].s; return dslvm::Value::none(); });
  reg.function("now", {}, "string", [](dslvm::Args&) { return dslvm::Value::string("2026-10-18"); });  // now() -> string
  if (argc > 3 && std::string(argv[3]) == "--dsl-signatures") { for (auto& s : reg.signatures()) std::cout << s << "\n"; return 0; }  // diffed against `lmscript --dsl-signatures`

  dslvm::Program program;
  try { program = dslvm::Program::load(dslvm::read_file(argv[1]), dslvm::read_file(argv[2]), reg); }  // bytecode, schema
  catch (const dslvm::Error& e) { std::cerr << "refused: " << e.what() << "\n"; return 3; }       // nothing ran

  Greeter greeter;
  dslvm::Context ctx; ctx.set("g", &greeter);
  try { program.run(ctx, reg); } catch (const dslvm::Error& e) { std::cerr << "dsl error: " << e.what() << "\n"; return 3; }
  std::cout << greeter.last << "\n";                                     // the host continues with what the chunk did
}
```

`Registry` is declared with the schema's types (parameter types and return type, as strings); a name may have several overloads with distinct parameter tuples, while registering the same parameter tuple twice is refused. `load()` resolves canonical import signatures (including parameter and return types), and refuses a file whose imports the registry does not provide, before anything runs. `reg.signatures()` lists the
sorted canonical strings, including the built-in standard. Compare them with `lmscript --dsl-signatures`
for the merged schema, or use `matches_schema_signatures(...)` with a host-only schema. That is a consistency
check, not a defense: by design the VM performs no comprehensive call-time argument or return checks; it still checks method receiver kinds and other defensive VM invariants. Bytecode is trusted like the host's
own code. `Value` carries `INT` (int64), `FLOAT` (binary64), `STR`, `BOOL` and `OBJ` (a host pointer plus the schema type name); a
void method returns `Value::none()`. Host code throws `dslvm::Error` to report a run-time failure.

**Result line and exit codes.** CLI hosts print exactly one JSON line on stdout through `dslvm::report_ok(json)`,
`dslvm::report_error("load"|"run", e)` (returns 3) or `dslvm::report_app_failure(message)` (returns 4) and exit with the returned
code; lmscript's own compile failures are exit 2. `Error::code` is `LOAD_BAD_BYTECODE`, `LOAD_SCHEMA_MISMATCH`, `LOAD_UNBOUND`,
`RUN_ERROR` (a VM error), `RUN_HOST` (thrown by a host closure) or `APP_FAIL`. When a non-empty line table maps the faulting
opcode, `Error` carries numeric `line`/`column` and prefixes the message with `dsl line N:C: `; the standard JSON result includes
those fields; a zero column means the line is known and only `line` is emitted. Empty line tables preserve the original message and omit the fields. Server hosts (lmweb) keep their own protocol.

## Versioning rule

`dslvm::VERSION` is the bytecode format version this SDK reads. The SDK version equals that number (currently 6) and the
loader refuses every other version: there is no backward compatibility. When the compiler's format changes, the compiler,
this header and every host move together. Version 6 adds the rule entry table to version 5's structured `while` and `range`
loops, lexical `break` and `continue`, integer and floating-point math, typed imports, and line tables.

## Rule entries (v6)

An annotated rule (`@KIND fn r(...) -> R implements I`) compiles to an entry in the file's entries section: a
self-contained mini-program (own locals, `max_stack`, code ending with HALT, own line table) sharing the file's constant,
import and context pools. Parameters are the unhidden locals `0..nparams-1`, stored by the host before the call; a
non-void rule's result is the single value on the operand stack at HALT. The loader verifies each entry exactly like the
body, plus one rule: the stack depth at HALT is 0 for a `-> void` interface and 1 otherwise.

```cpp
struct Entry { std::string name, annotation, iface, returns; std::vector<std::string> params; };  // Program::Entry
const std::vector<Program::Entry>& entries = program.entries();        // source order
dslvm::Value v = program.call(i, ctx, reg, {dslvm::Value::integer(41)}); // runs entry i with args in its parameter slots
```

`call()` shares one interpreter with `run()` (dry mode, RunStats, std run scope and located errors behave the same),
returns `Value::none()` for a void interface, and treats a wrong argument count or out-of-range index as a host
programming error (`RUN_HOST`, naming the entry). Argument kinds are not checked (the host built them from its own
schema). `run()` is unchanged and executes only the body.

## What load() guarantees

Wrong magic, another version (v3/v4 included), a schema-hash mismatch, truncation, trailing bytes, unknown constant tags,
undecodable instructions, out-of-range constant/local/import/context indexes, calls whose import kind or arity differ, ordinary
jumps that miss an instruction start, go backwards outside a paired `LOOP_BACK` or cross a loop boundary, malformed or misnested
range/while loops, unknown
MATH function IDs, writes to
their read-only hidden locals, a missing HALT, and
context declarations or import signatures the registry does not provide (the message names the host's own signature for that
name) are refused with a message that names the problem. `run()` checks value kinds defensively and imposes no limits
(design decision): no stack cap, no locals cap, no string cap; `max_stack` from the file is a reservation hint.
Integer arithmetic is 64-bit two's complement and wraps; division and remainder by zero are the run-time errors.

## Test

    make test           # native runtime and optional string catalog tests
    make conformance    # 406 core runtime vectors and 268 standard catalog vectors
    make test-string-stdlib # stdlib/string.hpp unit cases and schema/registry/compiler signature parity

## Standard library catalog

`stdlib/` implements the built-in standard installed by every default registry. The canonical
`std/lmcode-std.schema.json` supplies declarations to the current compiler through the merge helper.
The individual fragments and group binders remain available for manual opted-out registries:

```cpp
#include "stdlib/string.hpp"
dslvm::Registry registry(dslvm::StdMode::Disabled);
dslvm::stdlib::register_string(registry);
```

Register once per registry, not once per chunk. Both chunks must have been compiled against the exact schema bytes supplied to their loads:

```cpp
const auto schema = dslvm::read_file("host.schema.json");
auto first = dslvm::Program::load(dslvm::read_file("first.dslb"), schema, registry);
auto second = dslvm::Program::load(dslvm::read_file("second.dslb"), schema, registry);
dslvm::Context context; // Set any host context objects here.
first.run(context, registry);
second.run(context, registry);
```

Copy the fragment's `functions` object into the host schema. `make test` runs the unit host and compares its sorted
`Registry::signatures()` against the current compiler's `--dsl-signatures` output for `tests/string.schema.json`. It uses
`$LMSCRIPT_BIN` when set; otherwise the check uses a compatible `lmscript` on PATH, or runs the published package with `npx` if the PATH command is too old. Set `LMSCRIPT_BIN` to select a specific CLI.
The parity test also checks the fixture against the shipped fragment and uses the executable selected by `BUILD`, including sanitizer builds.
The catalog header is compiled alone with `-std=c++17 -Wall -Wextra -Werror`.

| Function | Parameters → result | Meaning |
|---|---|---|
| `str_starts_with` | string, string → bool | Prefix test; empty prefix matches. |
| `str_ends_with` | string, string → bool | Suffix test; empty suffix matches. |
| `str_contains` | string, string → bool | Substring test; empty needle matches. |
| `str_lower` | string → string | ASCII lowercase. |
| `str_upper` | string → string | ASCII uppercase. |
| `str_substr` | string, int, int → string | Byte slice with clamping. |
| `str_len` | string → int | Byte length, including NUL bytes. |
| `str_match` | string, string → bool | Whole-string glob match; second argument is the pattern. |
| `str_find` | string, string → int | First byte offset, -1 if absent; empty needle returns 0. |
| `str_replace` | string, string, string → string | Replace non-overlapping matches. |
| `str_trim` | string → string | Trim ASCII whitespace at both ends. |
| `str_repeat` | string, int → string | Repeat count times; zero returns empty. |
| `str_to_int` | string → int | Parse signed 64-bit decimal, or return 0. |
| `str_is_int` | string → bool | Whether integer parsing succeeds. |
| `str_to_float` | string → float | Parse finite double, or return 0.0. |
| `str_is_float` | string → bool | Whether floating-point parsing succeeds. |

String operations use byte offsets, byte lengths, and byte matching; UTF-8 is not decoded. Embedded NUL bytes are preserved,
and NUL is not whitespace. In C++, construct NUL-containing values with an explicit-length `std::string`. `str_lower` and `str_upper` only map
ASCII letters. `str_substr(s,start,count)` clamps negative starts to zero and overlong counts to the end; negative counts or starts
beyond the end return empty. `str_find` returns `-1` when absent. `str_replace` replaces non-overlapping matches and leaves the
input unchanged for an empty search string. It preflights the result size before allocation and throws `dslvm::Error`
(code `RUN_HOST`, message `str_replace: result must be at most 67108864 bytes`) above 64 MiB, including no-op copies. `str_trim` removes ASCII space and bytes `0x09` through `0x0d`. Integer and float
 parsing validates a complete, in-range decimal token with at most one leading sign; integer conversion uses `std::from_chars`, while float conversion uses `strtod_l`/`_strtod_l` in a per-call C numeric locale.
Whitespace, embedded NULs, hexadecimal forms, overflow and unrepresentable floating-point underflow are invalid; `str_to_int`/`str_to_float` return zero on failure, and `str_is_int`/
`str_is_float` distinguish failure (float `nan`/infinity are invalid). `str_repeat` rejects negative counts and results above
64 MiB with `dslvm::Error` (code `RUN_HOST`, message
`str_repeat: count must be nonnegative and result at most 67108864 bytes`). An empty string with any nonnegative count returns empty. `str_match` supports `*` (including slash), `?`, byte classes/ranges, `[!class]`, and backslash escapes inside and outside classes.
A leading `]` in a class is literal; descending ranges match no bytes. An unterminated `[` and a trailing backslash are literal. It uses a
non-recursive matcher with O(1) auxiliary memory and a deterministic per-call limit of 2^27 work units.
Each matcher iteration, character-class entry scanned (a byte or range, including malformed classes), and trailing star skipped
consumes one unit. Attempting the next unit throws `dslvm::Error`, code `RUN_HOST`, message
`str_match: pattern exceeds the 2^27 work limit`; it does not return a false match. These catalog limits are host policy, not VM limits.

`conformance/vectors` is generated by the lmscript compiler's conformance generator
from small DSL programs that cover every opcode, type and error path: `NAME.dsl` (source), `NAME.dslb` (bytecode),
`NAME.expect` (line 1 `trace`, `runtime_error` or `load_error`, then the expected trace lines; load_error requires an exact code on line 2 and message substring on line 3) and
`manifest.json`, whose `host` entry describes the stub a runtime must implement (context `t` records calls; `root`, `thing`,
`twice`, `half`, `yes`, `fail`). A line beginning `~ ` in an expected trace keeps its prefix exact and accepts a finite numeric
token within relative tolerance 1e-12, for platform libm results. Another runtime (e.g. a future Rust one) can implement the same stub and reads
the same files; nothing in the vectors is C++-specific.

## Consuming it

Hosts keep a verbatim copy at `cpp/dslvm.hpp` plus `sdk.lock` (SDK commit and the header's sha256) and `scripts/sync-sdk.sh`,
which copies the header from a sibling checkout `../lmcode-cpp` and rewrites the lock; each host's `test.sh` fails when the
vendored header's hash differs from the lock. Example hosts: lmcad (CAD/rendering), lmhtml (HTML tooling), lmor-dsl (OR-Tools).

## License

This project, including `include/dslvm.hpp` and the optional `stdlib/` catalog, is licensed under the MIT License. See [`LICENSE`](LICENSE). These are project-authored files and require no additional third-party notices.

## Sibling runtimes

The same bytecode v6 and the same conformance vectors are implemented by [lmcode-cpp](https://github.com/lmctl-ai/lmcode-cpp),
[lmcode-rust](https://github.com/lmctl-ai/lmcode-rust), [lmcode-go](https://github.com/lmctl-ai/lmcode-go) and
[lmcode-csharp](https://github.com/lmctl-ai/lmcode-csharp). Each is a library that turns an AOT executable into an lmscript host.

Standard catalog conformance fixtures and their portable byte-tracing host contract are in
[`conformance/stdlib-vectors`](conformance/stdlib-vectors/README.md).

`stdlib/collections.hpp` provides byte-string, int64 and binary64 lists plus insertion-ordered string maps. Default registries own their arena; native callbacks use `registry.std_collections().make_*` to return standard values valid for the run. `stdlib/testing.hpp` provides optional `print` and `assert_close`; hosts declare them in their own schema and explicitly call `register_testing(registry, optional_sink)`. `make test` checks all catalog behavior and compiler signature parity.

## Built-in lmcode std

Every `dslvm::Registry` now includes the 206-signature standard by default. `Registry(StdMode::Disabled)` opts out; `dslvm::register_std(registry)` enables it idempotently. `signatures()` includes std, `host_signatures()` excludes it, and `matches_schema_signatures(...)` accepts either host-only or already-merged declarations while still rejecting host drift. Collections belong to the registry and are cleared at entry and exit of each `Program::run`, including failures; native callbacks allocate through `registry.std_collections()`. Printing and assertions are outside std and require the optional testing catalog or host implementations.

`std/lmcode-std.schema.json` is the canonical versioned surface. Until the compiler injects it implicitly, `python3 scripts/merge-std.py host.schema.json effective.schema.json` generates the effective compile/load schema without copying std declarations into host sources. The helper also extracts host-only schemas and normalizes compiler signature lists. Vendor `include/dslvm.hpp` together with all `stdlib/*.hpp`; default registration has no external library dependency. Individual group headers and explicit binders remain supported for manual opted-out registries. Domain-specific types and stricter assertions remain host APIs.

`stdlib/file.hpp` supplies built-in `file_read`, `file_write`, `file_append`, `file_exists`, and `file_lines`: raw bytes, native paths, no newline translation. LF splitting preserves CR and a trailing empty field; failures report function, original path and OS reason.

The [generated std reference](docs/std.md) lists every built-in signature, its byte semantics, and errors.

## Complete teaching host and shared integration corpus

Start at [lmcode-cpp-sample](https://github.com/lmctl-ai/lmcode-cpp-sample) for a
small C++17 shop/ledger host and a ten-step embedding guide. Its vendored copy of
[`sample-suite/`](sample-suite/README.md) is shared by the C, C++, Rust, Go, C#,
Java, Kotlin and Scala samples. The canonical suite lives here: 88 programs plus
three loader mutations, exact stdout/exit fixtures, CLI protocol checks and an audit
of every one of the 182 std imports, including the v2 data and Json programs.

```sh
# With the C++ sample checked out beside this SDK:
make sample-suite
# Or run any host implementation against these exact bytes:
./sample-suite/run-suite.sh /path/to/host
```

Compiler/language changes must recompile and review this corpus, then run every
sample port. New std functions fail the import audit until covered. The C++
reference passes all 91 cases; the 69 pre-v2 cases also matched an independent
Go SDK host exactly. Run `./sample-suite/run-all-samples.sh --all` to check all
ports and compare their actual output bytes; unfinished ports fail visibly.


The v3 catalog adds strict byte Base64/hex codecs and independent insertion-order map `keys()`/`values()` snapshots; see [the canonical std inventory](docs/std.md).

## Runnable annotated interface example

[examples/rule-entries.lms](examples/rule-entries.lms) defines two `@item`
functions and one `@total` function, each with `implements Adjuster`. The
schema declares `Adjuster(Money) -> void`; the compiler checks the signature.
The body prints first, then the host calls entries by annotation. `@never`
demonstrates an entry that this host does not dispatch.

```sh
lmscript examples/rule-entries.lms /tmp/rule-entries.dslb
../lmcode-cpp-sample/build/lmcode-cpp-sample --dsl /tmp/rule-entries.dslb
```

Use lmscript 0.2.444 (DSLB v6). The expected trace is pinned in
[sample-suite/expected/068-rule-entries.out](sample-suite/expected/068-rule-entries.out).
For a non-void interface and returned entry value, see
[069-rule-value-entry.lms](sample-suite/programs/069-rule-value-entry.lms).

## Run checks with lmscript

```sh
npm install -g @lmctl-ai/lmctl
lmctl test.lms
# Equivalent: lmscript test.lms
# Include a built reference host:
lmscript --no-preflight test.lms ../lmcode-cpp-sample/build/lmcode-cpp-sample
```

The npm package installs both executables. Both run local `.lms` files;
`lmctl` also accepts a URL ending in `.lms`. Repository tests require this
checkout and its native build tools. `test.lms` runs SDK checks, canonical
signature coverage, bytecode reproduction and negative runner controls.
See [the sample driver documentation](sample-suite/README.md#lmscript-test-drivers)
for byte-exact process capture and corpus-directory overrides.

## Text templates

The standard `template_render(string, Json) -> string` function renders arbitrary text with nested context, indexed lists, loops and boolean conditions. Inserted values remain literal. See the [portable contract and examples](std/template.md).


## Invocation-owned JSON results (std slice 1)

`dslvm::ResultSink` is an optional host-owned invocation sink. Configure it before
running the body, retain it across selected `Program::call` entries, and reset it
only before the next logical program. Per-region arena reset never resets it:

```cpp
dslvm::Registry registry;
dslvm::ResultSink sink;
registry.set_result_sink(&sink);
sink.reset();
program.run(context, registry);
for (size_t i = 0; i < program.entries().size(); ++i)
  program.call(i, context, registry, {}); // supply the interface's actual args
// After successful dispatch AND application validation only:
dslvm::write_result_frame(output, sink);
```

A successful host appends `LMCODE_RETURN_BEGIN\n<compact JSON>\nLMCODE_RETURN_END\n`
to its diagnostics. `result_frame(sink)` returns these bytes without printing;
`write_result_frame(stream,sink)` writes to an explicitly selected stream.
The stream must preserve LF bytes (binary stdout mode on Windows). Failed host
invocations do not call either framing helper. Existing `report_ok`/error helpers
remain diagnostic reporters; SDK installation does not add implicit CLI output.

The 24 ordinary standard imports are `lmcode_return_set(T)` and
`lmcode_return_put(string,T)` for int, float, bool, string, Json, the three typed
lists and the three typed maps, plus `lmcode_return_null()` and
`lmcode_return_put_null(string)`. Values are deep owned copies. Missing sink,
invalid UTF-8, nonfinite/unsafe integer values, nonzero underflow, cycles and
unsupported handles throw `RUN_HOST`; failed writes leave the prior value intact.
Field writes promote null to an insertion-ordered object and refuse scalar/list
roots. Overwriting a key preserves its position. Unset and explicit null frame
as `null`. Binary64 uses `std::to_chars` shortest-round-trip formatting, not the
legacy `str(float)` formatter: `0.1+0.2` frames as `0.30000000000000004`.

The new canonical fixtures in `conformance/return-vectors` execute all 24 imports,
entry lifetime/HALT behavior and failure silence independently of the existing
trace corpus. `make test` and `make conformance` include these checks and their
fault controls. The older shared sample corpus/adopter cutover is slice 6.

**Std skew:** this canonical standard moves before the compiler's bundled std in
slice 3. Do not release an npm compiler in that window. DSLB stays version 6 and
the catalog stays version 3. Explicit fixture schemas can declare these ordinary
imports for the pinned compiler; `$return` lowering and executable blocks are
separate slices. Consumer SDK/suite pins are updated by their assigned adopters.
See [result-sink lifetime and skew notes](durable-memory/12-result-sink.md).
Boundary and malformed-wire coverage is described in [conformance/BOUNDARIES.md](conformance/BOUNDARIES.md).
