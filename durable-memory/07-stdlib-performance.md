# Standard catalog performance

`scripts/bench-stdlib.py` generates temporary schema-bound DSL workloads and runs prebuilt conformance executables. Compilation is outside the measured interval. Each workload gets one warmup execution, then the requested number of timed repetitions; the runner must report the expected short trace. The harness captures only the small runner result, never the large file or string trace.

The workloads exercise one million operations for `StrList` and `StrMap`, a one-million-element replace/split/match workload, and a 100 MB write/read/append/lines workload. `--size 1000` is the fast smoke case. Supply additional SDK runners with repeated `--runner name=command`; the command receives the generated vector directory. The harness does not build SDKs or impose process limits.

Example:

```sh
python3 scripts/bench-stdlib.py --size 1000 --repeat 1
python3 scripts/bench-stdlib.py --size 1000000 --repeat 3 --runner cpp=build/conformance
```

The 1M run below was measured on 2026-10-24 in the shared Linux development container (`lmcode-cpp`, GCC optimized build already present, one warmup and one measured repetition; wall time includes runner startup and excludes compilation):

| Runner | Workload | Size | Seconds |
| --- | --- | ---: | ---: |
| C++ | StrList add/at/contains/index_of | 1,000,000 | 0.203716262 |
| C++ | StrMap set/has/get_or/key_at/remove | 1,000,000 | 1.449995403 |
| C++ | replace/split/match | 1,000,000 | 0.069042330 |
| C++ | write/read/append/lines | 100 MB | 0.745624803 |

These are reference measurements for detecting regressions. They are not release gates and should be refreshed with the same workload size, warmup, repetition count, and machine description when comparing SDK implementations.
