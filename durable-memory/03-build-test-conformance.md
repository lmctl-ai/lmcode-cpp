# Build, test, and conformance

The SDK requires a C++17 compiler and has no external build dependencies. Run:

```sh
make test
make conformance
```

The unit tests build an in-test assembler for malformed programs and cover valid execution, arithmetic, loops, imports, source
locations, host errors, and loader refusals. Conformance runs the language-neutral fixtures under `conformance/vectors`.

The vectors contain source, compiler-emitted bytecode, expected traces or errors, and a manifest describing the stub host. A
runtime implementation supplies that host and runs the same fixtures; the vector format does not depend on C++.

The version in `dslvm::VERSION` must match the compiler's emitted format. The loader rejects every other bytecode version.

`Program::RunStats` can optionally count instruction dispatches for measurements. It is observational and does not impose a
budget or limit.

## Standard library catalog

`stdlib/` contains optional schema declarations and C++ binders for ordinary host functions. `make test` includes the string
catalog unit tests and `tests/string-signature-parity.sh`, which compares the test host's canonical registry signatures with the
published compiler's output for the fixture schema. Install `@lmctl-ai/lmctl` so `lmscript` is on `PATH`, or set `LMSCRIPT_BIN`
to an explicit compiler executable.

## Four-SDK verification matrix

Run `scripts/matrix.sh` with sibling checkouts named `lmcode-cpp`, `lmcode-rust`,
`lmcode-go`, and `lmcode-csharp`. An optional positional argument selects their
parent directory; `LMCODE_CPP`, `LMCODE_RUST`, `LMCODE_GO`, and `LMCODE_CSHARP`
override individual checkout paths. Paths containing spaces are supported.

The script runs each SDK's full native tests, then its standalone runner against
both the C++ checkout's core and string-catalog vectors. C# additionally builds
Release with warnings as errors. It continues after a failed stage, prints one
PASS/FAIL table, and exits nonzero if any stage failed. Builds run in the supplied
checkouts; it does not pull, modify sources, or sync fixture copies.

Install C++/make, Rust/Cargo, Go, .NET 8 and a matching `lmscript` compiler first.
Compiler-parity skipping/fallback behavior remains controlled by each native test
suite. Each SDK/stage has a separate log. Set `MATRIX_LOG_DIR` for a specific log
directory, or `TMPDIR` for the parent of an automatically created log directory.
`make conformance` also runs both the core and optional catalog suites locally.

`scripts/test-matrix.sh` checks orchestration with isolated fake toolchains: success,
nonzero test commands, missing checkouts, continuation after failure, and paths
containing spaces. Linux CI runs these checks without requiring sibling clones.
