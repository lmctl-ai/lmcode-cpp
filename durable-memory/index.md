# lmcode-cpp durable memory

This repository contains the canonical, header-only C++ runtime (`cpp/dslvm.hpp`), native tests, standard library catalog modules, and language-neutral conformance fixtures for lmscript bytecode (format version 6).
Repository: https://github.com/lmctl-ai/lmcode-cpp.

## 60-second takeover

```sh
make test                             # Unit tests, assembler checks, string catalog tests
make conformance                      # Run language-neutral conformance vectors (70 core + 267 stdlib)
scripts/matrix.sh                     # Cross-SDK verification across cpp, rust, go, csharp
```

## Document map

| File | Contents |
| --- | --- |
| [01-purpose-and-place.md](01-purpose-and-place.md) | Host integration, architecture role, and conformance vector suite |
| [02-architecture-and-decisions.md](02-architecture-and-decisions.md) | Public C++ API, bytecode verifier, VM dispatch loop, memory semantics |
| [03-build-test-conformance.md](03-build-test-conformance.md) | Build targets, test runner, vector manifests, and 4-SDK verification matrix |
| [04-gotchas-and-limits.md](04-gotchas-and-limits.md) | Bytecode format versioning, trust boundaries, vendoring contracts |
| [05-open-items.md](05-open-items.md) | Backlog, potential optimizations, and future format versions |
| [06-stdlib-catalog.md](06-stdlib-catalog.md) | Catalog contract, standard library modules, byte-string rules, vector authoring |
| [07-stdlib-performance.md](07-stdlib-performance.md) | Standard catalog benchmarks, StrList/StrMap performance measurements |
| [07-compiler-readiness-sweep.md](07-compiler-readiness-sweep.md) | Compiler readiness sweep methodology, test methodology, and rollout plan |
| [08-host-adoption-inventory.md](08-host-adoption-inventory.md) | Host adoption inventory for _lmcode_ block syntax and std migration (data: [08-host-adoption-inventory.json](08-host-adoption-inventory.json)) |
| [09-compiler-readiness-results.md](09-compiler-readiness-results.md) | Host-by-host compiler readiness results and baseline status (data: [09-compiler-readiness-results.json](09-compiler-readiness-results.json)) |
| [10-std-migration-validation.md](10-std-migration-validation.md) | Standard library host migration validation plan and oracle verification |
| [tools/README.md](tools/README.md) | The sweep, the rename-audit driver and the site-migration scripts |
| [`12-dslb-v6-rule-entries.md`](12-dslb-v6-rule-entries.md) | lmcode bytecode (DSLB v6) rule entry table: format, loader/verifier rules, entries()/call() API, sample-suite dispatch |
| [12-result-sink.md](12-result-sink.md) | Invocation-owned result sink imports, canonical return signatures, numeric/frame contract and std-skew window |

## Adding a conformance vector or function

### Adding a conformance vector
1. Create a vector folder under `conformance/vectors/<suite>/<name>/`.
2. Provide source file, compiled bytecode v6, expected stdout/trace, and `manifest.json`.
3. Run `make conformance` to ensure the vector passes in the C++ test runner.
4. Run `scripts/matrix.sh` to verify cross-SDK conformance across Go, Rust, and C#.

### Adding a stdlib catalog function
1. Define function signature in `stdlib/<domain>/schema.json`.
2. Implement typed C++ binder in `stdlib/<domain>/<domain>.hpp`.
3. Add unit test under `tests/` and run `make test`.

## Releases and host vendoring

`lmcode-cpp` is a header-only SDK. Releases and tags correspond to bytecode format and SDK versions. Downstream lmscript hosts vendor `cpp/dslvm.hpp` directly into their trees (usually at `cpp/dslvm.hpp` or `sdk/dslvm.hpp`) and pin the exact commit and sha256 in `sdk.lock`.
Continuous Integration tests all pushes across 5 platforms: Linux x64/arm64, macOS arm64/x64, and Windows x64.
- [update-sample-suite.md](update-sample-suite.md): canonical sample-suite update and all-port verification rule.
- [Re-runnable std migrator](11-lmstd-migrator.md): lmscript transactional previews, known host patterns, compilation limits and historical SSL/SSH checks.
- [13-lmscript-test-drivers.md](13-lmscript-test-drivers.md): lmscript-driven SDK/sample checks, binary adapter, exact-output and negative controls.
