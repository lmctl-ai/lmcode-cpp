# Standard catalog performance

`scripts/bench-stdlib.py` generates temporary schema-bound DSL workloads and runs prebuilt conformance executables. Compilation is outside the measured interval. Each workload gets one warmup execution, then the requested number of timed repetitions; the runner must report the expected short trace. The harness captures only the small runner result, never the large file or string trace.

The workloads exercise one million operations for `StrList` and `StrMap`, including full indexed reads, updates, removals and joins, a one-million-element replace/split/match workload, and a 200 MB write/read/append/lines workload. `--size 1000` is the fast smoke case. Supply additional SDK runners with repeated `--runner name=command`; the command receives the generated vector directory. The harness does not build SDKs; `--timeout` bounds each runner process.

Example:

```sh
python3 scripts/bench-stdlib.py --size 1000 --repeat 1
python3 scripts/bench-stdlib.py --size 1000000 --repeat 3 --runner cpp=build/conformance
```

The four-runner measurement used `--timeout 300`, with an explicitly compiled `-O2` C++ runner in scratch storage, Rust `target/release/dsl-conformance`, the Go conformance binary, and the C# Release conformance DLL.

The 1M run below was measured on 2026-10-24 in the shared Linux development container (GCC/Rust/Go/.NET optimized builds; one warmup and three measured repetitions; wall time includes runner startup and excludes compilation). Each sample starts a fresh process, so runtime/JIT startup is included. The untimed run warms filesystem/OS caches, not a shared JIT process. Values are medians of the three samples, in **seconds**:

| Workload | C++ | Rust | Go | C# |
| --- | ---: | ---: | ---: | ---: |
| Runner startup | 0.001260 | 0.000817 | 0.001200 | 0.048088 |
| StrList add/at/contains/index_of/join | 0.282612 | 0.360028 | 0.374842 | 0.407212 |
| StrMap set/has/get/get_or/key_at/remove/update/reinsert | 1.967395 | 1.643596 | 2.497043 | 5.115520 |
| replace/split/match | 0.111809 | 0.170703 | 0.089678 | 0.554300 |
| write/read/append/lines (200 MB total) | 1.008403 | 0.480234 | 0.648787 | 1.336262 |

Machine: x86_64, 4 available CPUs, Intel Core i9-14900HX; GCC 11.4.0, rustc 1.92.0, Go 1.26.3, .NET 8.0.132. Exact heads, flags, raw samples and method metadata are in `benchmarks/results/2026-10-24-linux-x64.json`. These are reference measurements for detecting regressions, not release gates.
