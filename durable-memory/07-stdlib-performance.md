# Standard catalog performance

`scripts/bench-stdlib.py` generates temporary schema-bound DSL workloads and runs prebuilt conformance executables. Compilation is outside the measured interval. Each workload gets one warmup execution, then the requested number of timed repetitions; the runner must report the expected short trace. The harness captures only the small runner result, never the large file or string trace.

The workloads exercise one million operations for `StrList` and `StrMap`, a one-million-element replace/split/match workload, and a 100 MB write/read/append/lines workload. `--size 1000` is the fast smoke case. Supply additional SDK runners with repeated `--runner name=command`; the command receives the generated vector directory. The harness does not build SDKs or impose process limits.

Example:

```sh
python3 scripts/bench-stdlib.py --size 1000 --repeat 1
python3 scripts/bench-stdlib.py --size 1000000 --repeat 3 --runner cpp=build/conformance
```

The four-runner measurement used `--timeout 300`, with optimized prebuilt commands for `build/conformance`, Rust `target/release/dsl-conformance`, the Go conformance binary, and the C# Release conformance DLL.

The 1M run below was measured on 2026-10-24 in the shared Linux development container (GCC/Rust/Go/.NET optimized builds; one warmup and three measured repetitions; wall time includes runner startup and excludes compilation). Values are medians of the three samples:

| Workload | C++ | Rust | Go | C# |
| --- | ---: | ---: | ---: | ---: |
| Runner startup | 0.001491 | 0.001191 | 0.001593 | 0.066542 |
| StrList add/at/contains/index_of/join | 0.170643 | 0.199916 | 0.560864 | 0.638771 |
| StrMap set/has/get/get_or/key_at/remove/update/reinsert | 5.306035 | 1.711034 | 1.715219 | 1.576544 |
| replace/split/match | 0.062723 | 0.064047 | 0.046193 | 0.219099 |
| write/read/append/lines (200 MB total) | 1.399735 | 0.658817 | 0.806082 | 5.547340 |

These are reference measurements for detecting regressions. They are not release gates and should be refreshed with the same workload size, warmup, repetition count, and machine description when comparing SDK implementations.
